#!/usr/bin/env python3
"""Generic FFMedia parameter-driven Python pipeline CLI.

This is the Python counterpart of demo/ffmedia.cpp.  Modules are created with
their default constructors, configured only through MediaParameter, connected
as an explicit directed graph, and initialized in topological order.
"""

import argparse
import errno
import math
import os
import re
import signal
import sys
import threading
import time
from dataclasses import dataclass, field

import ff_pymedia as ff


UINT32_MAX = (1 << 32) - 1
READABLE = int(ff.PARAMETER_FLAG_READABLE)
WRITABLE = int(ff.PARAMETER_FLAG_WRITABLE)
RUNTIME = int(ff.PARAMETER_FLAG_RUNTIME)
DEPRECATED = int(ff.PARAMETER_FLAG_DEPRECATED)
STATE_ANY = int(ff.PARAMETER_STATE_ANY)

_signal_stop = threading.Event()


class CliError(RuntimeError):
    def __init__(self, message, code=-errno.EINVAL):
        super().__init__(message)
        self.code = code if code <= 0 else -code


class CliArgumentParser(argparse.ArgumentParser):
    def error(self, message):
        raise CliError(message, -errno.EINVAL)


def error_text(result):
    if result >= 0:
        return "success"
    code = -result
    if code <= 0 or code > 4095:
        return "unknown error"
    try:
        return os.strerror(code)
    except ValueError:
        return "unknown error"


def status_name(status):
    names = {
        ff.MediaStatus.CREATED: "created",
        ff.MediaStatus.STARTED: "started",
        ff.MediaStatus.EOS: "eos",
        ff.MediaStatus.STOPPED: "stopped",
        ff.MediaStatus.ABNORMAL: "abnormal",
    }
    return names.get(status, "unknown")


def valid_identifier(value):
    return re.fullmatch(r"[A-Za-z_][A-Za-z0-9_-]*", value or "") is not None


@dataclass(frozen=True)
class ModuleDescriptor:
    type_name: str
    category: str
    description: str
    graph_supported: bool
    class_name: str
    source: bool = False
    dynamic_source: bool = False

    def create(self):
        module_class = getattr(ff, self.class_name)
        return module_class()


def build_module_descriptors():
    descriptors = []

    def add(type_name, category, description, graph_supported, class_name,
            source=False, dynamic_source=False):
        if hasattr(ff, class_name):
            descriptors.append(ModuleDescriptor(
                type_name, category, description, graph_supported,
                class_name, source, dynamic_source))

    add("cam", "vi", "V4L2 camera source", True, "ModuleCam", True)
    add("file-reader", "vi", "File/container source", True,
        "ModuleFileReader", True)
    add("rtsp-client", "vi", "RTSP source", True,
        "ModuleRtspClient", True)
    add("rtmp-client", "vi",
        "RTMP source or publisher selected by source/publish", True,
        "ModuleRtmpClient", True, True)
    add("mem-reader", "vi", "Application-fed memory source", False,
        "ModuleMemReader", True)
    add("video-stack", "vp",
        "Composite processor with configured input layouts", True,
        "ModuleVideoStack")
    add("ffmpeg-demux", "vi", "FFmpeg input source", True,
        "ModuleFFmpegDemux", True)
    add("alsa-capture", "vi", "ALSA capture source", True,
        "ModuleAlsaCapture", True)

    add("mpp-dec", "vp", "Rockchip MPP decoder", True, "ModuleMppDec")
    add("mpp-enc", "vp", "Rockchip MPP encoder", True, "ModuleMppEnc")
    add("rga", "vp", "Rockchip RGA processor", True, "ModuleRga")
    add("image-processor", "vp", "EGL/OpenGL image processor", True,
        "ModuleImageProcessor")
    add("inference", "vp", "RKNN inference processor", True,
        "ModuleInference")
    add("aac-dec", "vp", "FDK-AAC decoder", True, "ModuleAacDec")
    add("aac-enc", "vp", "FDK-AAC encoder", True, "ModuleAacEnc")

    add("drm-display", "vo", "DRM display sink", True,
        "ModuleDrmDisplay")
    add("file-writer", "vo", "File output sink", True,
        "ModuleFileWriter")
    add("rtsp-server", "vo", "RTSP server sink", True,
        "ModuleRtspServer")
    add("rtmp-server", "vo", "RTMP server sink", True,
        "ModuleRtmpServer")
    add("gb28181-client", "vo", "GB28181 client sink", True,
        "ModuleGB28181Client")
    add("ffmpeg-mux", "vo", "FFmpeg output muxer", True,
        "ModuleFFmpegMux")
    add("renderer-video", "vo", "Window video renderer", True,
        "ModuleRendererVideo")
    add("alsa-playback", "vo", "ALSA playback sink", True,
        "ModuleAlsaPlayBack")
    return descriptors


MODULE_DESCRIPTORS = build_module_descriptors()
MODULE_BY_TYPE = {item.type_name: item for item in MODULE_DESCRIPTORS}


def parameter_type_name(parameter_type):
    names = {
        ff.ParameterType.BOOLEAN: "boolean",
        ff.ParameterType.INTEGER: "integer",
        ff.ParameterType.DOUBLE: "double",
        ff.ParameterType.STRING: "string",
        ff.ParameterType.OBJECT: "object",
    }
    return names.get(parameter_type, "invalid")


def apply_mode_name(mode):
    names = {
        ff.ParameterApplyMode.IMMEDIATE: "immediate",
        ff.ParameterApplyMode.RECONFIGURE: "reconfigure",
        ff.ParameterApplyMode.NEXT_START: "next-start",
        ff.ParameterApplyMode.CONSTRUCT_ONLY: "construct-only",
    }
    return names.get(mode, "unknown")


def access_name(flags):
    result = ""
    if int(flags) & READABLE:
        result += "r"
    if int(flags) & WRITABLE:
        result += "w"
    return result or "-"


def writable_states_name(states):
    states = int(states)
    if states == STATE_ANY:
        return "any"
    result = []
    for status, name in (
            (ff.MediaStatus.CREATED, "created"),
            (ff.MediaStatus.STARTED, "started"),
            (ff.MediaStatus.EOS, "eos"),
            (ff.MediaStatus.STOPPED, "stopped"),
            (ff.MediaStatus.ABNORMAL, "abnormal")):
        if states & (1 << int(status)):
            result.append(name)
    return "|".join(result) if result else "none"


def parameter_native_value(value):
    if isinstance(value, ff.ParameterValue):
        return value.toPython()
    return value


def is_v4l2_format_parameter(info):
    if (info.type != ff.ParameterType.INTEGER
            or info.name not in ("format", "pixel-format")
            or not info.has_minimum or not info.has_maximum):
        return False
    minimum = parameter_native_value(info.minimum)
    maximum = parameter_native_value(info.maximum)
    return minimum == 0 and maximum == UINT32_MAX


def v4l2_format_name(value):
    try:
        integer = int(parameter_native_value(value))
    except (TypeError, ValueError):
        return None
    if integer <= 0 or integer > UINT32_MAX:
        return None
    name = ff.v4l2GetFmtName(integer)
    if not name or name == "Unknow V4L2 Format":
        return None
    return name


def scalar_text(value):
    value = parameter_native_value(value)
    if isinstance(value, bool):
        return "true" if value else "false"
    if value is None:
        return "invalid"
    return str(value)


def parameter_value_text(info, value):
    native = parameter_native_value(value)
    if info.type == ff.ParameterType.INTEGER and info.enum_values:
        try:
            integer = int(native)
        except (TypeError, ValueError):
            integer = None
        if integer is not None:
            for item in info.enum_values:
                if int(item.value) == integer:
                    return item.name
    if is_v4l2_format_parameter(info):
        name = v4l2_format_name(native)
        if name:
            return name
    return scalar_text(native)


def print_parameter_info(module, info, path, depth):
    indent = " " * (depth * 2)
    attributes = [parameter_type_name(info.type), access_name(info.flags)]
    if info.type == ff.ParameterType.OBJECT and info.atomic:
        attributes.append("atomic")
    if int(info.flags) & RUNTIME:
        attributes.append("runtime")
    if int(info.flags) & DEPRECATED:
        attributes.append("deprecated")
    attributes.append("apply={}".format(apply_mode_name(info.apply_mode)))
    if int(info.flags) & WRITABLE:
        attributes.append("states={}".format(
            writable_states_name(info.writable_states)))

    line = "{}{} [{}]".format(indent, path, ", ".join(attributes))
    if info.type != ff.ParameterType.OBJECT:
        if int(info.flags) & READABLE:
            line += " default={}".format(
                parameter_value_text(info, info.default_value))
            try:
                current = module.getParameter(path)
                line += " current={}".format(
                    parameter_value_text(info, current))
            except Exception as error:  # pybind converts errno to exceptions.
                line += " current=<{}>".format(error)
        else:
            line += " default=<hidden> current=<write-only>"
        if info.has_minimum:
            line += " min={}".format(info.minimum.toString())
        if info.has_maximum:
            line += " max={}".format(info.maximum.toString())
        if info.unit:
            line += " unit={}".format(info.unit)
    print(line)

    if info.enum_values:
        values = " ".join("{}={}".format(item.name, item.value)
                          for item in info.enum_values)
        print("{}  enum: {}".format(indent, values))
    if info.description:
        print("{}  {}".format(indent, info.description))

    if info.type == ff.ParameterType.OBJECT:
        for child in info.object_members:
            child_path = path + "/" + child.name if path else child.name
            print_parameter_info(module, child, child_path, depth + 1)


def print_module_parameters(module, display_name, path=""):
    suffix = " ({})".format(path) if path else ""
    print("Parameters for {}{}:".format(display_name, suffix))
    if path:
        try:
            info = module.queryParameter(path)
        except Exception as error:
            raise CliError(
                "Parameter path '{}' not found: {}".format(path, error),
                -errno.ENOENT) from error
        print_parameter_info(module, info, path, 1)
        return
    for info in module.queryParameters():
        print_parameter_info(module, info, info.name, 1)


@dataclass
class RawParameterAssignment:
    module_id: str
    path: str
    value: str
    order: int
    block: int


class ParameterBlockParser:
    def __init__(self, text, module_id, next_order, block, output):
        self.text = text
        self.module_id = module_id
        self.next_order = next_order
        self.block = block
        self.output = output
        self.position = 0

    def parse(self):
        before = len(self.output)
        self._parse_entries("", None)
        self._skip_whitespace()
        if self.position != len(self.text):
            raise CliError("unexpected trailing parameter text")
        if len(self.output) == before:
            raise CliError("parameter block is empty")
        return self.next_order

    def _skip_whitespace(self):
        while (self.position < len(self.text)
               and self.text[self.position].isspace()):
            self.position += 1

    @staticmethod
    def _joined_path(prefix, name):
        return name if not prefix else prefix + "/" + name

    def _parse_entries(self, prefix, terminator):
        entry_count = 0
        while True:
            self._skip_whitespace()
            if self.position >= len(self.text):
                if terminator is not None:
                    raise CliError("missing closing '{}'".format(terminator))
                return
            if (terminator is not None
                    and self.text[self.position] == terminator):
                self.position += 1
                if entry_count == 0:
                    raise CliError("empty parameter object")
                return
            if self.text[self.position] == ";":
                self.position += 1
                continue
            if self.text[self.position] == "}":
                raise CliError("unexpected closing '}'")

            name_start = self.position
            while self.position < len(self.text):
                if self.text[self.position] in "={;}":
                    break
                self.position += 1
            name = self.text[name_start:self.position].strip()
            if not name:
                raise CliError("parameter name is empty")
            if self.position >= len(self.text):
                raise CliError(
                    "parameter '{}' is missing '=' or '{{'".format(name))

            operation = self.text[self.position]
            self.position += 1
            path = self._joined_path(prefix, name)
            if operation == "{":
                before = len(self.output)
                self._parse_entries(path, "}")
                if len(self.output) == before:
                    raise CliError(
                        "parameter object '{}' is empty".format(path))
            elif operation == "=":
                value = self._parse_value(terminator)
                self.output.append(RawParameterAssignment(
                    self.module_id, path, value,
                    self.next_order, self.block))
                self.next_order += 1
            else:
                raise CliError(
                    "parameter '{}' is missing '=' or '{{'".format(name))
            entry_count += 1

            self._skip_whitespace()
            if self.position >= len(self.text):
                if terminator is not None:
                    raise CliError("missing closing '{}'".format(terminator))
                return
            if self.text[self.position] == ";":
                self.position += 1
                continue
            if (terminator is not None
                    and self.text[self.position] == terminator):
                continue
            expected = "';'"
            if terminator is not None:
                expected += " or '{}'".format(terminator)
            raise CliError("expected {} after parameter '{}'".format(
                expected, path))

    def _parse_value(self, terminator):
        first = self.position
        while first < len(self.text) and self.text[first].isspace():
            first += 1

        quoted = first < len(self.text) and self.text[first] in "\"'"
        quote = self.text[first] if quoted else None
        if quoted:
            self.position = first + 1

        parsed = []
        quote_closed = not quoted
        while self.position < len(self.text):
            character = self.text[self.position]
            if character == "\\":
                if self.position + 1 >= len(self.text):
                    parsed.append(character)
                    self.position += 1
                    continue
                following = self.text[self.position + 1]
                if following in ";{}\\\"'":
                    parsed.append(following)
                    self.position += 2
                    continue
                parsed.append(character)
                self.position += 1
                continue
            if quoted and character == quote:
                quote_closed = True
                self.position += 1
                break
            if (not quoted
                    and (character == ";" or character == terminator)):
                break
            parsed.append(character)
            self.position += 1

        if not quote_closed:
            raise CliError("unterminated quoted parameter value")
        value = "".join(parsed)
        if quoted:
            self._skip_whitespace()
            if (self.position < len(self.text)
                    and self.text[self.position] != ";"
                    and self.text[self.position] != terminator):
                raise CliError(
                    "unexpected text after quoted parameter value")
            return value
        return value.strip()


@dataclass
class ModuleDeclaration:
    module_id: str
    type_name: str


@dataclass
class ConnectionSpec:
    producer: str
    consumer: str
    channels: list = field(default_factory=list)


@dataclass
class ShowTarget:
    module_id: str
    path: str = ""


@dataclass
class SynchronizeAssignment:
    module_id: str
    sync_type: object
    name: str = ""


@dataclass
class RunConfig:
    modules: list = field(default_factory=list)
    parameters: list = field(default_factory=list)
    connections: list = field(default_factory=list)
    show_targets: list = field(default_factory=list)
    synchronizers: list = field(default_factory=list)
    duration: float = 0.0


def parse_module_declaration(argument):
    if argument.count("=") != 1:
        raise CliError("module declaration must use ID=TYPE")
    module_id, type_name = (part.strip() for part in argument.split("=", 1))
    if not valid_identifier(module_id):
        raise CliError("invalid module id: '{}'".format(module_id))
    if not type_name:
        raise CliError("module type is empty")
    return ModuleDeclaration(module_id, type_name)


def parse_parameter_argument(argument, next_order, block_id, output):
    if ":" not in argument:
        raise CliError("parameter block must use ID:PARAMETERS")
    module_id, parameter_text = argument.split(":", 1)
    module_id = module_id.strip()
    if not valid_identifier(module_id):
        raise CliError(
            "invalid module id in parameter block: '{}'".format(module_id))
    return ParameterBlockParser(
        parameter_text, module_id, next_order, block_id, output).parse()


def parse_connection(argument):
    if argument.count("=") != 1:
        raise CliError(
            "connection must use PRODUCER[@CHANNELS]=CONSUMER")
    producer_text, consumer = argument.split("=", 1)
    producer_text = producer_text.strip()
    consumer = consumer.strip()
    if not valid_identifier(consumer):
        raise CliError("invalid consumer id: '{}'".format(consumer))
    if producer_text.count("@") > 1:
        raise CliError("connection contains more than one '@'")

    channels = []
    if "@" in producer_text:
        producer, channel_text = producer_text.split("@", 1)
        producer = producer.strip()
        if not channel_text:
            raise CliError("connection channel list is empty")
        unique = set()
        for item in channel_text.split(","):
            item = item.strip()
            if not re.fullmatch(r"[0-9]+", item or ""):
                raise CliError(
                    "invalid output channel id: '{}'".format(item))
            channel = int(item, 10)
            if channel > UINT32_MAX:
                raise CliError(
                    "invalid output channel id: '{}'".format(item),
                    -errno.ERANGE)
            if channel in unique:
                raise CliError(
                    "duplicate output channel id: '{}'".format(channel),
                    -errno.EEXIST)
            unique.add(channel)
            channels.append(channel)
    else:
        producer = producer_text
    if not valid_identifier(producer):
        raise CliError("invalid producer id: '{}'".format(producer))
    return ConnectionSpec(producer, consumer, channels)


def parse_show_target(argument):
    if ":" in argument:
        module_id, path = argument.split(":", 1)
        path = path.strip()
        if not path:
            raise CliError(
                "invalid --show-params target: '{}'".format(argument))
    else:
        module_id, path = argument, ""
    module_id = module_id.strip()
    if not valid_identifier(module_id):
        raise CliError(
            "invalid --show-params target: '{}'".format(argument))
    return ShowTarget(module_id, path)


def synchronize_type_name(sync_type):
    names = {
        ff.SynchronizeType.SYNCHRONIZETYPE_VIDEO: "video",
        ff.SynchronizeType.SYNCHRONIZETYPE_AUDIO: "audio",
        ff.SynchronizeType.SYNCHRONIZETYPE_ABSOLUTE: "absolute",
    }
    return names.get(sync_type, "unknown")


def parse_synchronize_type(value):
    names = {
        "video": ff.SynchronizeType.SYNCHRONIZETYPE_VIDEO,
        "audio": ff.SynchronizeType.SYNCHRONIZETYPE_AUDIO,
        "abs": ff.SynchronizeType.SYNCHRONIZETYPE_ABSOLUTE,
        "absolute": ff.SynchronizeType.SYNCHRONIZETYPE_ABSOLUTE,
    }
    sync_type = names.get(value)
    if sync_type is None:
        raise CliError("sync must use video, audio, or absolute")
    return sync_type


def parse_synchronize_assignment(argument):
    if argument.count("=") != 1:
        raise CliError("sync must use MODULE=MODE[:NAME]")
    module_id, setting = (part.strip() for part in argument.split("=", 1))
    if not valid_identifier(module_id):
        raise CliError(
            "invalid module id in sync assignment: '{}'".format(module_id))

    if setting.count(":") > 1:
        raise CliError(
            "sync must use video, audio, or absolute, optionally followed "
            "by :NAME")
    if ":" in setting:
        mode, name = setting.split(":", 1)
        name = name.strip()
        if not valid_identifier(name):
            raise CliError(
                "sync name must be a valid identifier: '{}'".format(name))
    else:
        mode, name = setting, ""
    return SynchronizeAssignment(
        module_id, parse_synchronize_type(mode.strip()), name)


def parse_duration(value):
    try:
        duration = float(value)
    except ValueError as error:
        raise argparse.ArgumentTypeError(
            "duration must be a non-negative number") from error
    if not math.isfinite(duration) or duration < 0.0:
        raise argparse.ArgumentTypeError(
            "duration must be a non-negative number")
    return duration


def make_argument_parser():
    parser = CliArgumentParser(
        prog=os.path.basename(sys.argv[0]),
        description="Build FFMedia module graphs through MediaParameter.")
    commands = parser.add_subparsers(dest="command", required=True)

    commands.add_parser("modules", help="list available module types")

    params = commands.add_parser(
        "params", help="show a module parameter schema")
    params.add_argument("type", help="module type from the modules command")
    params.add_argument("path", nargs="?", default="",
                        help="optional parameter subtree")

    run = commands.add_parser(
        "run", help="configure, connect, and run a module graph",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=(
            "Example:\n"
            "  %(prog)s -m src=ffmpeg-demux -m dec=mpp-dec "
            "-m out=file-writer \\\n"
            "    -p 'src:source{uri=/data/input.mp4;loop=1}' \\\n"
            "    -p 'out:path=/data/output.raw' \\\n"
            "    -c src@0=dec -c dec=out"))
    run.add_argument("-m", "--module", action="append", default=[],
                     metavar="ID=TYPE", help="declare a graph node")
    run.add_argument("-p", "--params", action="append", default=[],
                     metavar="ID:ENTRIES",
                     help="configure parameters; repeat as needed")
    run.add_argument("-c", "--connect", action="append", default=[],
                     metavar="PRODUCER[@CHANNELS]=CONSUMER",
                     help="connect two nodes")
    run.add_argument("--show-params", action="append", default=[],
                     metavar="ID[:PATH]",
                     help="print configured parameters and exit before init")
    run.add_argument("--sync", action="append", default=[],
                     metavar="MODULE=MODE[:NAME]",
                     help="configure per-module sync; matching NAME shares it")
    run.add_argument("-d", "--duration", type=parse_duration, default=0.0,
                     metavar="SECONDS",
                     help="stop after SECONDS; zero waits for EOS or signal")
    return parser


def make_run_config(arguments):
    config = RunConfig(duration=arguments.duration)
    config.modules = [parse_module_declaration(item)
                      for item in arguments.module]
    next_order = 0
    for block_id, item in enumerate(arguments.params):
        next_order = parse_parameter_argument(
            item, next_order, block_id, config.parameters)
    config.connections = [parse_connection(item)
                          for item in arguments.connect]
    config.show_targets = [parse_show_target(item)
                           for item in arguments.show_params]
    config.synchronizers = [parse_synchronize_assignment(item)
                            for item in arguments.sync]
    return config


@dataclass
class ParameterMeta:
    info: object
    atomic_root: str


def collect_parameter_metadata(info, path, parent_atomic, metadata):
    atomic_root = parent_atomic
    if (not atomic_root and info.type == ff.ParameterType.OBJECT
            and info.atomic):
        atomic_root = path
    metadata[path] = ParameterMeta(info, atomic_root)
    for child in info.object_members:
        collect_parameter_metadata(
            child, path + "/" + child.name, atomic_root, metadata)


def parameter_metadata(module):
    metadata = {}
    for root in module.queryParameters():
        collect_parameter_metadata(root, root.name, "", metadata)
    return metadata


def parameter_value_from_text(info, text):
    if info.type == ff.ParameterType.OBJECT:
        raise CliError(
            "Use child paths or OBJECT blocks instead of assigning a "
            "string to an OBJECT parameter.", -errno.ENOTSUP)
    if info.type == ff.ParameterType.INTEGER and info.enum_values:
        for item in info.enum_values:
            if item.name == text:
                return int(item.value)
    try:
        return ff.ParameterValue.fromString(info.type, text).toPython()
    except Exception as parse_error:
        if is_v4l2_format_parameter(info):
            image_format = int(ff.v4l2GetFmtByName(text))
            if image_format:
                return image_format
        raise CliError(
            "invalid {} value '{}'".format(
                parameter_type_name(info.type), text),
            -errno.EINVAL) from parse_error


def set_nested_object_value(target, parts, value):
    current = target
    for part in parts[:-1]:
        child = current.get(part)
        if not isinstance(child, dict):
            child = {}
            current[part] = child
        current = child
    current[parts[-1]] = value


@dataclass
class ParameterOperation:
    order: int
    path: str
    value: object = None
    object_patch: bool = False
    patch: dict = field(default_factory=dict)
    affected_paths: list = field(default_factory=list)


@dataclass
class ModuleInstance:
    module_id: str
    descriptor: ModuleDescriptor
    module: object
    assignments: list = field(default_factory=list)
    sync_configured: bool = False
    sync_type: object = None
    sync_name: str = ""
    synchronizer: object = None


def print_parameter_failure(instance, paths, result):
    plural = "s" if len(paths) > 1 else ""
    suffix = " parameter{}: {}".format(plural, " ".join(paths)) if paths else ""
    print(
        "Failed to configure module '{}' ({}){}, ret={} ({})".format(
            instance.module_id, instance.descriptor.type_name, suffix,
            result, error_text(result)),
        file=sys.stderr)
    print_module_parameters(
        instance.module,
        "{}={}".format(instance.module_id, instance.descriptor.type_name))


def configure_module(instance):
    if not instance.assignments:
        return
    metadata = parameter_metadata(instance.module)
    effective = []
    assigned_paths = set()

    for assignment in instance.assignments:
        meta = metadata.get(assignment.path)
        if meta is None:
            print_parameter_failure(instance, [assignment.path], -errno.ENOENT)
            raise CliError("unknown parameter path", -errno.ENOENT)
        repeatable_layout = (
            instance.descriptor.type_name == "video-stack"
            and meta.atomic_root == "input-layout")
        key = "{}:{}".format(assignment.block, assignment.path) \
            if repeatable_layout else assignment.path
        if key in assigned_paths:
            print_parameter_failure(instance, [assignment.path], -errno.EEXIST)
            raise CliError("duplicate parameter assignment", -errno.EEXIST)
        assigned_paths.add(key)
        effective.append(assignment)

    operations = []
    object_operations = {}
    for assignment in effective:
        meta = metadata[assignment.path]
        if not int(meta.info.flags) & WRITABLE:
            print_parameter_failure(instance, [assignment.path], -errno.EACCES)
            raise CliError("parameter is read-only", -errno.EACCES)
        if meta.info.type == ff.ParameterType.OBJECT:
            print_parameter_failure(instance, [assignment.path], -errno.ENOTSUP)
            raise CliError(
                "Use child paths or OBJECT blocks instead of assigning a "
                "string to an OBJECT parameter.", -errno.ENOTSUP)
        try:
            value = parameter_value_from_text(meta.info, assignment.value)
        except CliError as error:
            print_parameter_failure(instance, [assignment.path], error.code)
            raise

        if not meta.atomic_root:
            operations.append(ParameterOperation(
                assignment.order, assignment.path, value=value,
                affected_paths=[assignment.path]))
            continue

        repeatable_layout = (
            instance.descriptor.type_name == "video-stack"
            and meta.atomic_root == "input-layout")
        operation_key = meta.atomic_root
        if repeatable_layout:
            operation_key += ":{}".format(assignment.block)
        operation_index = object_operations.get(operation_key)
        if operation_index is None:
            operation_index = len(operations)
            object_operations[operation_key] = operation_index
            operations.append(ParameterOperation(
                assignment.order, meta.atomic_root, object_patch=True))

        relative = assignment.path[len(meta.atomic_root):].lstrip("/")
        parts = relative.split("/") if relative else []
        if not parts or any(not part for part in parts):
            print_parameter_failure(instance, [assignment.path], -errno.EINVAL)
            raise CliError("invalid atomic parameter path", -errno.EINVAL)
        operation = operations[operation_index]
        set_nested_object_value(operation.patch, parts, value)
        operation.affected_paths.append(assignment.path)

    operations.sort(key=lambda operation: operation.order)
    for operation in operations:
        try:
            value = operation.patch if operation.object_patch else operation.value
            result = instance.module.setParameter(operation.path, value)
        except Exception as error:
            print_parameter_failure(instance, operation.affected_paths,
                                    -errno.EINVAL)
            raise CliError(str(error), -errno.EINVAL) from error
        if result < 0:
            print_parameter_failure(instance, operation.affected_paths, result)
            raise CliError("parameter configuration failed", result)


def create_module_instances(config):
    if not config.modules:
        raise CliError(
            "At least one -m/--module declaration is required.",
            -errno.EINVAL)
    instances = []
    index_by_id = {}
    for declaration in config.modules:
        if declaration.module_id in index_by_id:
            raise CliError(
                "Duplicate module id: '{}'".format(declaration.module_id),
                -errno.EEXIST)
        descriptor = MODULE_BY_TYPE.get(declaration.type_name)
        if descriptor is None:
            raise CliError(
                "Unknown or disabled module type: '{}'".format(
                    declaration.type_name), -errno.ENOENT)
        try:
            module = descriptor.create()
        except Exception as error:
            raise CliError(
                "Failed to create module '{}' ({}): {}".format(
                    declaration.module_id, declaration.type_name, error),
                -errno.EINVAL) from error
        index_by_id[declaration.module_id] = len(instances)
        instances.append(ModuleInstance(
            declaration.module_id, descriptor, module))

    for assignment in config.parameters:
        index = index_by_id.get(assignment.module_id)
        if index is None:
            raise CliError(
                "Parameter block references unknown module id: '{}'".format(
                    assignment.module_id), -errno.ENOENT)
        instances[index].assignments.append(assignment)

    for assignment in config.synchronizers:
        index = index_by_id.get(assignment.module_id)
        if index is None:
            raise CliError(
                "Sync assignment references unknown module id: '{}'".format(
                    assignment.module_id), -errno.ENOENT)
        instance = instances[index]
        if instance.sync_configured:
            raise CliError(
                "Duplicate sync assignment for module '{}'".format(
                    instance.module_id), -errno.EEXIST)
        instance.sync_configured = True
        instance.sync_type = assignment.sync_type
        instance.sync_name = assignment.name

    for instance in instances:
        configure_module(instance)
    return instances, index_by_id


def configure_module_synchronizers(instances):
    synchronizers = {}
    synchronize_types = {}
    for instance in instances:
        if not instance.sync_configured:
            continue
        key = ("module:" + instance.module_id if not instance.sync_name
               else "name:" + instance.sync_name)
        synchronizer = synchronizers.get(key)
        if synchronizer is None:
            synchronizer = ff.Synchronize(instance.sync_type)
            synchronizer.setFirstFrameDuration(50000)
            synchronizers[key] = synchronizer
            synchronize_types[key] = instance.sync_type
        elif synchronize_types[key] != instance.sync_type:
            raise CliError(
                "Sync name '{}' uses conflicting modes: {} and {}".format(
                    instance.sync_name,
                    synchronize_type_name(synchronize_types[key]),
                    synchronize_type_name(instance.sync_type)))
        instance.synchronizer = synchronizer
        instance.module.setSynchronize(synchronizer)


def synchronize_parameter_value(instance):
    if not instance.sync_configured:
        return ""
    value = synchronize_type_name(instance.sync_type)
    return value if not instance.sync_name else value + ":" + instance.sync_name


@dataclass
class GraphPlan:
    topological_order: list
    roots: list
    incoming_connections: list


def module_is_source(instance):
    if not instance.descriptor.dynamic_source:
        return instance.descriptor.source
    try:
        return bool(instance.module.getParameter("source/publish"))
    except Exception as error:
        raise CliError(
            "Failed to determine graph role for module '{}': {}".format(
                instance.module_id, error), -errno.EINVAL) from error


def build_graph_plan(config, instances, index_by_id):
    count = len(instances)
    incoming = [[] for _ in range(count)]
    outgoing = [[] for _ in range(count)]
    indegree = [0] * count
    edges = set()

    for connection_index, connection in enumerate(config.connections):
        if (connection.producer not in index_by_id
                or connection.consumer not in index_by_id):
            raise CliError(
                "Connection references unknown module: {}={}".format(
                    connection.producer, connection.consumer), -errno.ENOENT)
        producer = index_by_id[connection.producer]
        consumer = index_by_id[connection.consumer]
        if producer == consumer:
            raise CliError(
                "Self connection is not allowed: {}".format(
                    connection.producer), -errno.EINVAL)
        edge = (producer, consumer)
        if edge in edges:
            raise CliError(
                "Duplicate connection: {}={}".format(
                    connection.producer, connection.consumer), -errno.EEXIST)
        edges.add(edge)
        outgoing[producer].append(consumer)
        incoming[consumer].append(connection_index)
        indegree[consumer] += 1

    roots = []
    for index, instance in enumerate(instances):
        if not instance.descriptor.graph_supported:
            raise CliError(
                "Module type '{}' requires a specialized application adapter "
                "and cannot be used by the generic graph runner.".format(
                    instance.descriptor.type_name), -errno.ENOTSUP)
        if instance.descriptor.type_name == "video-stack":
            requirements = instance.module.getInputMediaChannelRequirements()
            if not requirements:
                raise CliError(
                    "VideoStack module '{}' requires at least one input-layout "
                    "parameter.".format(instance.module_id), -errno.EINVAL)
            if len(incoming[index]) > len(requirements):
                raise CliError(
                    "VideoStack module '{}' has more incoming connections than "
                    "configured input-layout entries.".format(
                        instance.module_id), -errno.EINVAL)
        source = module_is_source(instance)
        if source and indegree[index] != 0:
            raise CliError(
                "Source module '{}' cannot have an incoming connection.".format(
                    instance.module_id), -errno.EINVAL)
        if not source and indegree[index] == 0:
            raise CliError(
                "Processing/output module '{}' has no producer.".format(
                    instance.module_id), -errno.EINVAL)
        if indegree[index] == 0:
            roots.append(index)

    remaining = list(indegree)
    ready = list(roots)
    order = []
    ready_index = 0
    while ready_index < len(ready):
        node = ready[ready_index]
        ready_index += 1
        order.append(node)
        for consumer in outgoing[node]:
            remaining[consumer] -= 1
            if remaining[consumer] == 0:
                ready.append(consumer)
    if len(order) != count:
        raise CliError("Module graph contains a cycle.", -errno.ELOOP)
    if not roots:
        raise CliError("Module graph has no source root.", -errno.EINVAL)
    return GraphPlan(order, roots, incoming)


def print_connection_details(producer, consumer):
    print("Producer output channels:", file=sys.stderr)
    for channel in producer.module.getOutputMediaChannels():
        print("  id={} name={} media={} codec={}".format(
            channel.id, channel.name, int(channel.media_type),
            int(channel.codec)), file=sys.stderr)
    print("Consumer input requirements:", file=sys.stderr)
    for requirement in consumer.module.getInputMediaChannelRequirements():
        print("  id={} name={} media={} allow-multiple={}".format(
            requirement.input_id, requirement.name,
            int(requirement.media_type),
            "true" if requirement.allow_multiple else "false"),
              file=sys.stderr)


def initialize_graph(config, instances, index_by_id, plan):
    for node in plan.topological_order:
        consumer = instances[node]
        for connection_index in plan.incoming_connections[node]:
            connection = config.connections[connection_index]
            producer = instances[index_by_id[connection.producer]]
            selection = ff.MediaChannelSelection(connection.channels)
            try:
                result = consumer.module.connectProducer(
                    producer.module, selection)
            except Exception as error:
                result = -errno.EINVAL
                print("Connection {}={} threw: {}".format(
                    connection.producer, connection.consumer, error),
                      file=sys.stderr)
            if result < 0:
                print("Failed to connect {}={}, ret={} ({})".format(
                    connection.producer, connection.consumer, result,
                    error_text(result)), file=sys.stderr)
                print_connection_details(producer, consumer)
                print_module_parameters(
                    consumer.module,
                    "{}={}".format(consumer.module_id,
                                   consumer.descriptor.type_name))
                raise CliError("module connection failed", result)

        try:
            result = consumer.module.init()
        except Exception as error:
            raise CliError(
                "Failed to initialize module '{}' ({}): {}".format(
                    consumer.module_id, consumer.descriptor.type_name, error),
                -errno.EINVAL) from error
        if result < 0:
            print("Failed to initialize module '{}' ({}), ret={} ({})".format(
                consumer.module_id, consumer.descriptor.type_name,
                result, error_text(result)), file=sys.stderr)
            print_module_parameters(
                consumer.module,
                "{}={}".format(consumer.module_id,
                               consumer.descriptor.type_name))
            raise CliError("module initialization failed", result)


def print_graph(config, instances):
    print("FFMedia pipeline:")
    for instance in instances:
        print("  {} = {}".format(
            instance.module_id, instance.descriptor.type_name))
    print("Connections:")
    for connection in config.connections:
        producer = connection.producer
        if connection.channels:
            producer += "@" + ",".join(str(item)
                                       for item in connection.channels)
        print("  {} -> {}".format(producer, connection.consumer))
    for instance in instances:
        if instance.sync_configured:
            print("  sync {}={}".format(
                instance.module_id, synchronize_parameter_value(instance)))


class PipelineRunner:
    def __init__(self, instances, plan):
        self.instances = instances
        self.roots = [instances[index].module for index in plan.roots]
        self.root_ids = {instances[index].module_id for index in plan.roots}
        self.eos_roots = set()
        self.eos_lock = threading.Lock()
        self.stop_event = threading.Event()
        self.failed = False
        self.all_roots_eos = False
        self.started = False
        self.callbacks = []

    def _on_status(self, module_id, is_root, status):
        print("[status] {} -> {}".format(module_id, status_name(status)))
        if status == ff.MediaStatus.ABNORMAL:
            self.failed = True
            self.stop_event.set()
            return
        if status != ff.MediaStatus.EOS or not is_root:
            return
        with self.eos_lock:
            self.eos_roots.add(module_id)
            if len(self.eos_roots) == len(self.roots):
                self.all_roots_eos = True
                self.stop_event.set()

    def start(self):
        for instance in self.instances:
            module_id = instance.module_id
            is_root = module_id in self.root_ids

            def callback(_name, status, module_id=module_id,
                         is_root=is_root):
                self._on_status(module_id, is_root, status)

            self.callbacks.append(callback)
            if not instance.module.setMediaStatusChangeHooker(callback):
                raise CliError(
                    "failed to install status hook for {}".format(module_id),
                    -errno.EBUSY)

        self.started = True
        try:
            for root in self.roots:
                root.start()
                root.dumpPipe()
        except Exception:
            self.stop()
            raise

    def wait(self, duration):
        started_at = time.monotonic()
        while not self.stop_event.is_set() and not _signal_stop.is_set():
            if duration > 0.0 and time.monotonic() - started_at >= duration:
                break
            self.stop_event.wait(0.1)
        if self.all_roots_eos:
            time.sleep(0.5)

    def _stop_root(self, root, failures):
        try:
            root.stop()
        except Exception as error:
            failures.append(error)

    def stop(self):
        self.stop_event.set()
        if not self.started:
            return
        failures = []
        if len(self.roots) == 1:
            self._stop_root(self.roots[0], failures)
        else:
            threads = [threading.Thread(
                target=self._stop_root, args=(root, failures),
                name="ffmedia-stop-{}".format(index))
                       for index, root in enumerate(self.roots)]
            for thread in threads:
                thread.start()
            for thread in threads:
                thread.join()
        for root in self.roots:
            try:
                root.dumpPipeSummary()
            except Exception as error:
                failures.append(error)
        if failures:
            self.failed = True
            for error in failures:
                print("Warning: pipeline stop failed: {}".format(error),
                      file=sys.stderr)
        self.started = False


def modules_command(_arguments):
    print("{:<20}{:<6}{:<10}{}".format(
        "TYPE", "CLASS", "GRAPH", "DESCRIPTION"))
    for descriptor in MODULE_DESCRIPTORS:
        print("{:<20}{:<6}{:<10}{}".format(
            descriptor.type_name, descriptor.category,
            "yes" if descriptor.graph_supported else "special",
            descriptor.description))
    return 0


def params_command(arguments):
    descriptor = MODULE_BY_TYPE.get(arguments.type)
    if descriptor is None:
        raise CliError(
            "Unknown or disabled module type: '{}'".format(arguments.type),
            -errno.ENOENT)
    module = descriptor.create()
    print_module_parameters(module, descriptor.type_name, arguments.path)
    return 0


def show_configured_parameters(config, instances, index_by_id):
    for target in config.show_targets:
        index = index_by_id.get(target.module_id)
        if index is None:
            raise CliError(
                "--show-params references unknown module id: '{}'".format(
                    target.module_id), -errno.ENOENT)
        instance = instances[index]
        print_module_parameters(
            instance.module,
            "{}={}".format(instance.module_id,
                           instance.descriptor.type_name),
            target.path)


def run_command(arguments):
    config = make_run_config(arguments)
    instances, index_by_id = create_module_instances(config)
    configure_module_synchronizers(instances)
    if config.show_targets:
        show_configured_parameters(config, instances, index_by_id)
        return 0

    plan = build_graph_plan(config, instances, index_by_id)
    initialize_graph(config, instances, index_by_id, plan)
    print_graph(config, instances)
    pipeline = PipelineRunner(instances, plan)
    try:
        pipeline.start()
        pipeline.wait(config.duration)
    finally:
        pipeline.stop()
    return 1 if pipeline.failed else 0


def signal_handler(_signum, _frame):
    _signal_stop.set()


def main(argv=None):
    parser = make_argument_parser()
    try:
        arguments = parser.parse_args(argv)
        if arguments.command == "modules":
            return modules_command(arguments)
        if arguments.command == "params":
            return params_command(arguments)
        if arguments.command == "run":
            return run_command(arguments)
        parser.print_help()
        return 1
    except CliError as error:
        print("ffmedia.py failed: {}, ret={} ({})".format(
            error, error.code, error_text(error.code)), file=sys.stderr)
        return 1
    except (OSError, RuntimeError, ValueError) as error:
        print("ffmedia.py failed: {}".format(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)
    raise SystemExit(main())
