#!/usr/bin/env python3
"""FFMedia module pipeline demo.

The demo intentionally builds every feature as a module graph.  Sources are
started once after all consumers have been connected and initialized.
"""

import argparse
import os
import re
import signal
import stat
import threading
import time
from functools import partial

import ff_pymedia as m

try:
    import cv2
    import numpy as np
except ImportError:
    cv2 = None
    np = None


class PipelineError(RuntimeError):
    pass


class Pipeline:
    """Own module references and manage the roots of a module graph."""

    def __init__(self):
        self.modules = []
        self.roots = []
        self.external_consumers = []
        self.files = []
        self.stop_event = threading.Event()
        self.failed = False
        self.started = False

    def initialize(self, module, label, producer=None, root=False,
                   additional_producers=()):
        # Keep the Python wrapper alive before init(), including on failures.
        self.modules.append(module)
        if producer is not None:
            ret = module.connectProducer(producer)
            if ret < 0:
                raise PipelineError(
                    "{} connectProducer({}) failed: {}".format(
                        label, producer.getName(), ret))
        for additional_producer in additional_producers:
            ret = module.connectProducer(additional_producer)
            if ret < 0:
                raise PipelineError(
                    "{} connectProducer({}) failed: {}".format(
                        label, additional_producer.getName(), ret))
        ret = module.init()
        if ret < 0:
            raise PipelineError("{} init failed: {}".format(label, ret))
        if root:
            self.roots.append(module)
        return module

    def add_external_consumer(self, producer, name, callback):
        consumer = producer.addExternalConsumer(name, callback)
        if consumer is None:
            raise PipelineError("{} addExternalConsumer failed".format(name))
        self.external_consumers.append(consumer)
        return consumer

    def add_file(self, file_obj):
        self.files.append(file_obj)
        return file_obj

    def on_status(self, name, status):
        print("{} status changed to {}".format(name, status))
        if status == m.MediaStatus.ABNORMAL:
            self.failed = True
            self.stop_event.set()
        elif status == m.MediaStatus.EOS:
            self.stop_event.set()

    def start(self):
        for root in self.roots:
            root.setMediaStatusChangeHooker(self.on_status)
        for root in self.roots:
            root.start()
            root.dumpPipe()
        self.started = True

    def wait(self, duration):
        if duration > 0:
            self.stop_event.wait(duration)
        else:
            while not self.stop_event.wait(1.0):
                pass

    def stop(self):
        if self.started:
            for root in reversed(self.roots):
                try:
                    root.stop()
                except Exception as error:
                    print("Warning: failed to stop {}: {}".format(
                        root.getName(), error))
            for root in self.roots:
                root.dumpPipeSummary()
            self.started = False
        for file_obj in self.files:
            file_obj.close()
        self.files.clear()


class Cv2Display:
    def __init__(self, name, sync, count):
        self.name = name
        self.sync = sync
        self.count = count


def align(value, alignment):
    return (value + alignment - 1) & ~(alignment - 1)


def parse_size(value):
    match = re.fullmatch(r"(\d+)[xX](\d+)", value)
    if not match:
        raise argparse.ArgumentTypeError("size must be WIDTHxHEIGHT")
    return tuple(map(int, match.groups()))


def rotation_from_degrees(value):
    rotations = {
        0: m.RgaRotate.RGA_ROTATE_NONE,
        1: m.RgaRotate.RGA_ROTATE_VFLIP,
        2: m.RgaRotate.RGA_ROTATE_HFLIP,
        90: m.RgaRotate.RGA_ROTATE_90,
        180: m.RgaRotate.RGA_ROTATE_180,
        270: m.RgaRotate.RGA_ROTATE_270,
    }
    if value not in rotations:
        raise PipelineError("unsupported rotation: {}".format(value))
    return rotations[value]


def cv2_callback(display, name, queue_size, media_buffer):
    del name, queue_size
    video_buffer = m.VideoBuffer.from_base(media_buffer)
    if display.sync is not None:
        delay = display.sync.updateVideo(video_buffer.getPUstimestamp(), 0)
        if delay > 0:
            time.sleep(delay / 1000000.0)

    para = video_buffer.getImagePara()
    image = np.ndarray(
        shape=(para.height, para.width, 3),
        buffer=video_buffer.getActiveData(),
        dtype=np.uint8,
        strides=(para.hstride * 3, 3, 1))
    for index in range(display.count):
        cv2.imshow(display.name + str(index), image)
    cv2.waitKey(1)


def file_callback(file_obj, name, queue_size, media_buffer):
    del name, queue_size
    file_obj.write(media_buffer.getActiveData())


def get_parameters():
    parser = argparse.ArgumentParser(
        description="Build a media pipeline from FFMedia module components")
    parser.add_argument("-i", "--input-source", "--input_source",
                        required=True,
                        help="file, /dev/video*, RTSP/RTMP URL or FFmpeg input")
    parser.add_argument("--input-size", type=parse_size,
                        help="raw/camera input size, WIDTHxHEIGHT")
    parser.add_argument("--input-format", default=None,
                        help="raw/camera input pixel format")
    parser.add_argument("-f", "--save-file", "--save_file",
                        help="dump source output through an external consumer")
    parser.add_argument("-o", "--output", type=parse_size,
                        help="processed image size, WIDTHxHEIGHT")
    parser.add_argument("-b", "--outputfmt", default="NV12",
                        help="processed image format (default: NV12)")
    parser.add_argument("--dec-disabled", "--dec_disabled",
                        action="store_true",
                        help="do not insert ModuleMppDec for compressed input")
    parser.add_argument("-e", "--encodetype", type=int, default=-1,
                        help="ModuleMppEnc EncodeType value; -1 disables encoding")
    parser.add_argument("-m", "--enmux",
                        help="encoded/raw output file or FFmpeg mux URI")
    parser.add_argument("-p", "--port", type=int, default=0,
                        help="enable RTSP/RTMP server on this port")
    parser.add_argument("--push-type", "--push_type", type=int,
                        choices=(0, 1), default=0,
                        help="0: RTSP server, 1: RTMP server")
    parser.add_argument("--push-path", "--push_path", default="/live/0",
                        help="RTSP/RTMP server path (default: /live/0)")
    parser.add_argument("--rtmp-url", "--rtmp_url",
                        help="publish encoded output with ModuleRtmpClient")
    parser.add_argument("--rtsp-transport", "--rtsp_transport", type=int,
                        choices=(0, 1, 2),
                        default=0, help="0: UDP, 1: TCP, 2: multicast")
    parser.add_argument("-s", "--sync", type=int, choices=(-1, 0, 1, 2),
                        default=-1, help="synchronizer type; -1 disables it")
    parser.add_argument("--audio", action="store_true",
                        help="enable source audio for playback/mux/push")
    parser.add_argument("--aplay", help="ALSA playback device")
    parser.add_argument("--arecord", help="ALSA capture device")
    parser.add_argument("-r", "--rotate", type=int, default=0,
                        choices=(0, 1, 2, 90, 180, 270))
    parser.add_argument("-d", "--drmdisplay", type=int, default=-1,
                        help="DRM plane id; 0 selects automatically")
    parser.add_argument("--connector", type=int, default=0)
    parser.add_argument("-z", "--zpos", type=int, default=0xFF)
    parser.add_argument("-c", "--cvdisplay", type=int, default=0,
                        help="number of OpenCV windows")
    parser.add_argument("-x", "--x11display", action="store_true",
                        help="display using ModuleRendererVideo")
    parser.add_argument("-l", "--loop", action="store_true")
    parser.add_argument("--duration", type=float, default=0,
                        help="stop after N seconds; 0 waits for EOS or signal")
    parser.add_argument("--gb28181-user-id", "--gb28181_user_id")
    parser.add_argument("--gb28181-server-id", "--gb28181_server_id")
    parser.add_argument("--gb28181-server-realm", "--gb28181_server_realm",
                        default="ffmedia")
    parser.add_argument("--gb28181-server-ip", "--gb28181_server_ip")
    parser.add_argument("--gb28181-server-port", "--gb28181_server_port",
                        type=int, default=5060)
    parser.add_argument("--use-ffmpeg-demux", "--use_ffmpeg_demux",
                        nargs="?", const="", default=None,
                        metavar="FORMAT",
                        help="use ModuleFFmpegDemux; optional input format")
    parser.add_argument("--use-ffmpeg-mux", "--use_ffmpeg_mux",
                        nargs="?", const="", default=None,
                        metavar="FORMAT",
                        help="use ModuleFFmpegMux; optional output format")
    return parser.parse_args()


def configure_input_para(module, args):
    if args.input_size is None and args.input_format is None:
        return
    para = m.ImagePara()
    if args.input_size is not None:
        para.width, para.height = args.input_size
        para.hstride, para.vstride = args.input_size
    if args.input_format is not None:
        para.v4l2Fmt = m.v4l2GetFmtByName(args.input_format)
        if para.v4l2Fmt == 0:
            raise PipelineError(
                "unsupported input format: {}".format(args.input_format))
    module.setOutputImagePara(para)


def create_source(pipeline, args):
    source_path = args.input_source
    if args.use_ffmpeg_demux is not None:
        source = m.ModuleFFmpegDemux(source_path, -1 if args.loop else 1)
        if args.use_ffmpeg_demux:
            source.setInputFormat(args.use_ffmpeg_demux)
    elif source_path.startswith("rtsp://"):
        source = m.ModuleRtspClient(
            source_path, m.RTSP_STREAM_TYPE(args.rtsp_transport), True,
            args.audio)
    elif source_path.startswith("rtmp://"):
        source = m.ModuleRtmpClient(source_path)
    else:
        try:
            mode = os.stat(source_path).st_mode
        except OSError:
            # FFmpeg supports non-file protocols and virtual inputs.
            source = m.ModuleFFmpegDemux(source_path, -1 if args.loop else 1)
        else:
            if stat.S_ISCHR(mode):
                source = m.ModuleCam(source_path)
            elif stat.S_ISREG(mode):
                source = m.ModuleFileReader(source_path, args.loop)
            else:
                source = m.ModuleFFmpegDemux(
                    source_path, -1 if args.loop else 1)

    configure_input_para(source, args)
    return pipeline.initialize(source, "input source", root=True)


def build_pipeline(pipeline, args):
    source = create_source(pipeline, args)
    last_video = source
    last_audio = None
    sync = None if args.sync == -1 else m.Synchronize(
        m.SynchronizeType(args.sync))

    if args.arecord:
        sample_info = m.SampleInfo()
        sample_info.channels = 2
        sample_info.fmt = m.SAMPLE_FMT_S16
        sample_info.nb_samples = 1024
        sample_info.sample_rate = 48000
        capture = pipeline.initialize(
            m.ModuleAlsaCapture(args.arecord, sample_info), "ALSA capture",
            root=True)
        last_audio = pipeline.initialize(
            m.ModuleAacEnc(sample_info), "AAC encoder", capture)

    if args.aplay:
        audio_producer = last_audio if last_audio is not None else source
        aac_decoder = pipeline.initialize(
            m.ModuleAacDec(), "AAC decoder", audio_producer)
        playback = m.ModuleAlsaPlayBack(args.aplay)
        playback.setSynchronize(sync)
        pipeline.initialize(playback, "ALSA playback", aac_decoder)

    input_para = last_video.getOutputImagePara()
    if m.v4l2fmtIsCompressed(input_para.v4l2Fmt) and not args.dec_disabled:
        decoder = m.ModuleMppDec()
        decoder.setBufferCount(10)
        last_video = pipeline.initialize(
            decoder, "MPP decoder", last_video)

    input_para = last_video.getOutputImagePara()
    output_width = input_para.width
    output_height = input_para.height
    if args.output is not None:
        output_width, output_height = args.output
    output_format = m.v4l2GetFmtByName(args.outputfmt)
    if output_format == 0:
        raise PipelineError("unsupported output format: {}".format(
            args.outputfmt))
    output_para = m.ImagePara(
        output_width, output_height, input_para.hstride, input_para.vstride,
        output_format)
    rotation = rotation_from_degrees(args.rotate)
    if rotation in (m.RgaRotate.RGA_ROTATE_90,
                    m.RgaRotate.RGA_ROTATE_270):
        output_para.width, output_para.height = (
            output_para.height, output_para.width)
    output_para.hstride = align(output_para.width, 16)
    output_para.vstride = align(output_para.height, 16)

    needs_processing = (
        args.rotate != 0 or input_para.width != output_para.width or
        input_para.height != output_para.height or
        input_para.v4l2Fmt != output_para.v4l2Fmt)
    if needs_processing:
        processor = m.ModuleRga(output_para, rotation)
        processor.setBufferCount(2)
        last_video = pipeline.initialize(
            processor, "RGA image processor", last_video)

    if args.drmdisplay != -1:
        display = m.ModuleDrmDisplay()
        display.setPlanePara(
            m.v4l2GetFmtByName("NV12"), args.drmdisplay,
            m.PLANE_TYPE.PLANE_TYPE_OVERLAY_OR_PRIMARY, args.zpos, 1,
            args.connector)
        display.setSynchronize(sync)
        pipeline.initialize(display, "DRM display", last_video)
        plane_width = display.getDisplayPlaneW()
        plane_height = display.getDisplayPlaneH()
        image_para = last_video.getOutputImagePara()
        width = min(plane_width, image_para.width)
        height = min(plane_height, image_para.height)
        display.setWindowSize(
            (plane_width - width) // 2, (plane_height - height) // 2,
            width, height)

    if args.x11display:
        renderer = m.ModuleRendererVideo("FFMedia pipeline")
        renderer.setSynchronize(sync)
        pipeline.initialize(renderer, "video renderer", last_video)

    if args.cvdisplay > 0:
        if cv2 is None or np is None:
            raise PipelineError("OpenCV/NumPy are required for --cvdisplay")
        if last_video.getOutputImagePara().v4l2Fmt != m.v4l2GetFmtByName(
                "BGR24"):
            raise PipelineError("--cvdisplay requires -b BGR24")
        cv_display = Cv2Display("FFMedia OpenCV ", sync, args.cvdisplay)
        pipeline.add_external_consumer(
            last_video, "opencv-display",
            partial(cv2_callback, cv_display))

    if args.encodetype != -1:
        encoder = m.ModuleMppEnc(
            m.EncodeType(args.encodetype), last_video.getOutputImagePara())
        encoder.setBufferCount(8)
        encoder.setDuration(0)
        last_video = pipeline.initialize(
            encoder, "MPP encoder", last_video)

        if args.port:
            if args.push_type == 0:
                server = m.ModuleRtspServer(args.push_path, args.port)
                label = "RTSP server"
            else:
                server = m.ModuleRtmpServer(args.push_path, args.port)
                label = "RTMP server"
            server.setBufferCount(0)
            if sync is not None:
                server.setSynchronize(sync)
            audio_producer = last_audio or source
            audio_producers = (
                (audio_producer,)
                if args.audio and audio_producer is not last_video else ())
            pipeline.initialize(
                server, label, last_video,
                additional_producers=audio_producers)

        if args.rtmp_url:
            publisher = m.ModuleRtmpClient(args.rtmp_url, m.ImagePara(), 0)
            if sync is not None:
                publisher.setSynchronize(sync)
            pipeline.initialize(
                publisher, "RTMP publisher", last_video)

        if args.gb28181_user_id:
            if not args.gb28181_server_id or not args.gb28181_server_ip:
                raise PipelineError(
                    "GB28181 requires --gb28181-server-id and "
                    "--gb28181-server-ip")
            gb28181 = m.ModuleGB28181Client(
                args.gb28181_user_id, "ffmedia")
            if sync is not None:
                gb28181.setSynchronize(sync)
            gb28181.setServerConfig(
                args.gb28181_server_id, args.gb28181_server_realm,
                args.gb28181_server_ip, args.gb28181_server_port, 3600)
            pipeline.initialize(gb28181, "GB28181 client", last_video)

    if args.enmux:
        if args.use_ffmpeg_mux is not None:
            muxer = m.ModuleFFmpegMux(args.enmux, args.use_ffmpeg_mux)
            label = "FFmpeg muxer"
        else:
            muxer = m.ModuleFileWriter(args.enmux)
            label = "file writer"
        if sync is not None:
            muxer.setSynchronize(sync)
        audio_producer = last_audio or source
        audio_producers = (
            (audio_producer,)
            if args.audio and audio_producer is not last_video else ())
        pipeline.initialize(
            muxer, label, last_video,
            additional_producers=audio_producers)

    if args.save_file:
        dump_file = pipeline.add_file(open(args.save_file, "wb"))
        pipeline.add_external_consumer(
            source, "source-dump", partial(file_callback, dump_file))


def main():
    args = get_parameters()
    if args.duration < 0:
        raise PipelineError("--duration must be non-negative")
    # Playback and capture are explicit requests to construct an audio branch.
    if args.aplay or args.arecord:
        args.audio = True

    pipeline = Pipeline()

    def request_stop(signum, frame):
        del signum, frame
        pipeline.stop_event.set()

    signal.signal(signal.SIGINT, request_stop)
    signal.signal(signal.SIGTERM, request_stop)

    try:
        build_pipeline(pipeline, args)
        pipeline.start()
        pipeline.wait(args.duration)
        # Give downstream consumers a short chance to drain EOS buffers.
        time.sleep(0.5)
    except (OSError, PipelineError, RuntimeError, ValueError) as error:
        print("FFMedia demo failed: {}".format(error))
        pipeline.failed = True
    finally:
        pipeline.stop()
        if cv2 is not None:
            cv2.destroyAllWindows()
    return 1 if pipeline.failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
