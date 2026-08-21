/*
 * Generic FFMedia command-line pipeline builder.
 *
 * Modules are created with their default constructors, configured only through
 * MediaParameter, connected as an explicit directed graph, and initialized in
 * topological order.
 */

#include <getopt.h>
#include <signal.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "base/ff_synchronize.hpp"
#include "base/pixel_fmt.hpp"
#include "module/base_config.h"
#include "module/module_media.hpp"
#include "module/vi/module_cam.hpp"
#include "module/vi/module_fileReader.hpp"
#include "module/vi/module_memReader.hpp"
#include "module/vi/module_rtmpClient.hpp"
#include "module/vi/module_rtspClient.hpp"
#include "module/vp/module_videoStack.hpp"
#include "module/vp/module_mppdec.hpp"
#include "module/vp/module_mppenc.hpp"
#include "module/vp/module_rga.hpp"
#include "module/vo/module_drmDisplay.hpp"
#include "module/vo/module_fileWriter.hpp"
#include "module/vo/module_gb28181Client.hpp"
#include "module/vo/module_rtmpServer.hpp"
#include "module/vo/module_rtspServer.hpp"

#if AUDIO_SUPPORT
#include "module/vi/module_alsaCapture.hpp"
#include "module/vo/module_alsaPlayBack.hpp"
#include "module/vp/module_aacdec.hpp"
#include "module/vp/module_aacenc.hpp"
#endif

#if FFMPEG_SUPPORT
#include "module/vi/module_ffmpegDemux.hpp"
#include "module/vo/module_ffmpegMux.hpp"
#endif

#if OPENGL_SUPPORT
#include "module/vo/module_rendererVideo.hpp"
#include "module/vp/module_imageProcessor.hpp"
#endif

#ifndef FFMEDIA_CLI_INFERENCE
#define FFMEDIA_CLI_INFERENCE 0
#endif

#if FFMEDIA_CLI_INFERENCE
#include "module/vp/module_inference.hpp"
#endif

using namespace FFMedia;

namespace
{

volatile sig_atomic_t g_signal_stop = 0;

void signalHandler(int)
{
    g_signal_stop = 1;
}

std::string trim(const std::string& value)
{
    const std::string whitespace = " \t\r\n";
    const size_t first = value.find_first_not_of(whitespace);
    if (first == std::string::npos)
        return "";
    const size_t last = value.find_last_not_of(whitespace);
    return value.substr(first, last - first + 1);
}

bool validIdentifier(const std::string& value)
{
    if (value.empty())
        return false;
    const unsigned char first = static_cast<unsigned char>(value.front());
    if (!(std::isalpha(first) || value.front() == '_'))
        return false;
    for (char ch : value) {
        const unsigned char current = static_cast<unsigned char>(ch);
        if (!(std::isalnum(current) || ch == '_' || ch == '-'))
            return false;
    }
    return true;
}

std::string errorText(int ret)
{
    if (ret >= 0)
        return "success";
    const int code = -ret;
    if (code <= 0 || code > 4095)
        return "unknown error";
    const char* message = std::strerror(code);
    return message ? message : "unknown error";
}

const char* statusName(MediaStatus status)
{
    switch (status) {
        case MediaStatus::CREATED:
            return "created";
        case MediaStatus::STARTED:
            return "started";
        case MediaStatus::EOS:
            return "eos";
        case MediaStatus::STOPPED:
            return "stopped";
        case MediaStatus::ABNORMAL:
            return "abnormal";
    }
    return "unknown";
}

using ModuleFactory = std::shared_ptr<ModuleMedia> (*)();

template <typename T>
std::shared_ptr<ModuleMedia> createModule()
{
    return std::make_shared<T>();
}

struct ModuleDescriptor {
    std::string type;
    std::string category;
    std::string description;
    bool graph_supported;
    bool source;
    bool dynamic_source;
    ModuleFactory factory;
};

void addDescriptor(std::vector<ModuleDescriptor>& descriptors,
                   const char* type, const char* category,
                   const char* description, bool graph_supported,
                   ModuleFactory factory, bool source = false,
                   bool dynamic_source = false)
{
    ModuleDescriptor descriptor;
    descriptor.type = type;
    descriptor.category = category;
    descriptor.description = description;
    descriptor.graph_supported = graph_supported;
    descriptor.source = source;
    descriptor.dynamic_source = dynamic_source;
    descriptor.factory = factory;
    descriptors.push_back(descriptor);
}

std::vector<ModuleDescriptor> buildModuleDescriptors()
{
    std::vector<ModuleDescriptor> descriptors;

    addDescriptor(descriptors, "cam", "vi", "V4L2 camera source", true,
                  &createModule<ModuleCam>, true);
    addDescriptor(descriptors, "file-reader", "vi", "File/container source",
                  true, &createModule<ModuleFileReader>, true);
    addDescriptor(descriptors, "rtsp-client", "vi", "RTSP source", true,
                  &createModule<ModuleRtspClient>, true);
    addDescriptor(descriptors, "rtmp-client", "vi",
                  "RTMP source or publisher selected by source/publish", true,
                  &createModule<ModuleRtmpClient>, true, true);
    addDescriptor(descriptors, "mem-reader", "vi",
                  "Application-fed memory source", false,
                  &createModule<ModuleMemReader>, true);
    addDescriptor(descriptors, "video-stack", "vp",
                  "Composite processor with configured input layouts", true,
                  &createModule<ModuleVideoStack>);
#if FFMPEG_SUPPORT
    addDescriptor(descriptors, "ffmpeg-demux", "vi", "FFmpeg input source",
                  true, &createModule<ModuleFFmpegDemux>, true);
#endif
#if AUDIO_SUPPORT
    addDescriptor(descriptors, "alsa-capture", "vi", "ALSA capture source",
                  true, &createModule<ModuleAlsaCapture>, true);
#endif

    addDescriptor(descriptors, "mpp-dec", "vp", "Rockchip MPP decoder", true,
                  &createModule<ModuleMppDec>);
    addDescriptor(descriptors, "mpp-enc", "vp", "Rockchip MPP encoder", true,
                  &createModule<ModuleMppEnc>);
    addDescriptor(descriptors, "rga", "vp", "Rockchip RGA processor", true,
                  &createModule<ModuleRga>);
#if OPENGL_SUPPORT
    addDescriptor(descriptors, "image-processor", "vp",
                  "EGL/OpenGL image processor", true,
                  &createModule<ModuleImageProcessor>);
#endif
#if FFMEDIA_CLI_INFERENCE
    addDescriptor(descriptors, "inference", "vp", "RKNN inference processor",
                  true, &createModule<ModuleInference>);
#endif
#if AUDIO_SUPPORT
    addDescriptor(descriptors, "aac-dec", "vp", "FDK-AAC decoder", true,
                  &createModule<ModuleAacDec>);
    addDescriptor(descriptors, "aac-enc", "vp", "FDK-AAC encoder", true,
                  &createModule<ModuleAacEnc>);
#endif

    addDescriptor(descriptors, "drm-display", "vo", "DRM display sink", true,
                  &createModule<ModuleDrmDisplay>);
    addDescriptor(descriptors, "file-writer", "vo", "File output sink", true,
                  &createModule<ModuleFileWriter>);
    addDescriptor(descriptors, "rtsp-server", "vo", "RTSP server sink", true,
                  &createModule<ModuleRtspServer>);
    addDescriptor(descriptors, "rtmp-server", "vo", "RTMP server sink", true,
                  &createModule<ModuleRtmpServer>);
    addDescriptor(descriptors, "gb28181-client", "vo", "GB28181 client sink",
                  true, &createModule<ModuleGB28181Client>);
#if FFMPEG_SUPPORT
    addDescriptor(descriptors, "ffmpeg-mux", "vo", "FFmpeg output muxer", true,
                  &createModule<ModuleFFmpegMux>);
#endif
#if OPENGL_SUPPORT
    addDescriptor(descriptors, "renderer-video", "vo",
                  "Window video renderer", true,
                  &createModule<ModuleRendererVideo>);
#endif
#if AUDIO_SUPPORT
    addDescriptor(descriptors, "alsa-playback", "vo", "ALSA playback sink",
                  true, &createModule<ModuleAlsaPlayBack>);
#endif

    return descriptors;
}

const std::vector<ModuleDescriptor>& moduleDescriptors()
{
    static const std::vector<ModuleDescriptor> descriptors = buildModuleDescriptors();
    return descriptors;
}

const ModuleDescriptor* findModuleDescriptor(const std::string& type)
{
    const auto& descriptors = moduleDescriptors();
    auto found = std::find_if(
        descriptors.begin(), descriptors.end(),
        [&type](const ModuleDescriptor& item) { return item.type == type; });
    return found == descriptors.end() ? nullptr : &*found;
}

const char* parameterTypeName(ParameterType type)
{
    switch (type) {
        case ParameterType::BOOLEAN:
            return "boolean";
        case ParameterType::INTEGER:
            return "integer";
        case ParameterType::DOUBLE:
            return "double";
        case ParameterType::STRING:
            return "string";
        case ParameterType::OBJECT:
            return "object";
        default:
            return "invalid";
    }
}

const char* applyModeName(ParameterApplyMode mode)
{
    switch (mode) {
        case ParameterApplyMode::IMMEDIATE:
            return "immediate";
        case ParameterApplyMode::RECONFIGURE:
            return "reconfigure";
        case ParameterApplyMode::NEXT_START:
            return "next-start";
        case ParameterApplyMode::CONSTRUCT_ONLY:
            return "construct-only";
    }
    return "unknown";
}

std::string accessName(uint32_t flags)
{
    std::string result;
    if (flags & PARAMETER_FLAG_READABLE)
        result += "r";
    if (flags & PARAMETER_FLAG_WRITABLE)
        result += "w";
    return result.empty() ? "-" : result;
}

std::string writableStatesName(ParameterStateMask states)
{
    if (states == PARAMETER_STATE_ANY)
        return "any";

    struct StateName {
        MediaStatus status;
        const char* name;
    };
    static const StateName names[] = {
        {MediaStatus::CREATED, "created"},
        {MediaStatus::STARTED, "started"},
        {MediaStatus::EOS, "eos"},
        {MediaStatus::STOPPED, "stopped"},
        {MediaStatus::ABNORMAL, "abnormal"},
    };

    std::string result;
    for (const auto& item : names) {
        const ParameterStateMask bit = static_cast<ParameterStateMask>(1)
                                       << static_cast<int>(item.status);
        if (!(states & bit))
            continue;
        if (!result.empty())
            result += "|";
        result += item.name;
    }
    return result.empty() ? "none" : result;
}

bool isV4l2FormatParameter(const ParameterInfo& info)
{
    if (info.type != ParameterType::INTEGER
        || (info.name != "format" && info.name != "pixel-format")
        || !info.has_minimum || !info.has_maximum) {
        return false;
    }

    int64_t minimum = 0;
    int64_t maximum = 0;
    return info.minimum.getInteger(minimum)
           && info.maximum.getInteger(maximum)
           && minimum == 0
           && maximum == static_cast<int64_t>(std::numeric_limits<uint32_t>::max());
}

const char* v4l2FormatName(const ParameterValue& value)
{
    int64_t integer = 0;
    if (!value.getInteger(integer) || integer <= 0
        || integer > static_cast<int64_t>(
               std::numeric_limits<uint32_t>::max())) {
        return nullptr;
    }

    const char* name = v4l2GetFmtName(static_cast<uint32_t>(integer));
    return name && std::strcmp(name, "Unknow V4L2 Format") != 0
               ? name
               : nullptr;
}

std::string parameterValueText(const ParameterInfo& info,
                               const ParameterValue& value)
{
    if (info.type == ParameterType::INTEGER && !info.enum_values.empty()) {
        int64_t integer = 0;
        if (value.getInteger(integer)) {
            auto found = std::find_if(
                info.enum_values.begin(), info.enum_values.end(),
                [integer](const ParameterEnumValue& item) {
                    return item.value == integer;
                });
            if (found != info.enum_values.end())
                return found->name;
        }
    }
    if (isV4l2FormatParameter(info)) {
        const char* name = v4l2FormatName(value);
        if (name)
            return name;
    }
    return value.toString();
}

void printParameterInfo(const ModuleMedia& module, const ParameterInfo& info,
                        const std::string& path, size_t depth)
{
    const std::string indent(depth * 2, ' ');
    std::cout << indent << path << " [" << parameterTypeName(info.type)
              << ", " << accessName(info.flags);
    if (info.type == ParameterType::OBJECT && info.atomic)
        std::cout << ", atomic";
    if (info.flags & PARAMETER_FLAG_RUNTIME)
        std::cout << ", runtime";
    if (info.flags & PARAMETER_FLAG_DEPRECATED)
        std::cout << ", deprecated";
    std::cout << ", apply=" << applyModeName(info.apply_mode);
    if (info.flags & PARAMETER_FLAG_WRITABLE) {
        std::cout << ", states="
                  << writableStatesName(info.writable_states);
    }
    std::cout << "]";

    if (info.type != ParameterType::OBJECT) {
        if (info.flags & PARAMETER_FLAG_READABLE) {
            std::cout << " default="
                      << parameterValueText(info, info.default_value);
            ParameterValue current;
            const int ret = module.getParameter(path, current);
            if (ret == 0)
                std::cout << " current="
                          << parameterValueText(info, current);
            else
                std::cout << " current=<" << ret << " "
                          << errorText(ret) << ">";
        } else {
            std::cout << " default=<hidden> current=<write-only>";
        }
        if (info.has_minimum)
            std::cout << " min=" << info.minimum.toString();
        if (info.has_maximum)
            std::cout << " max=" << info.maximum.toString();
        if (!info.unit.empty())
            std::cout << " unit=" << info.unit;
    }
    std::cout << "\n";

    if (!info.enum_values.empty()) {
        std::cout << indent << "  enum:";
        for (const auto& item : info.enum_values)
            std::cout << " " << item.name << "=" << item.value;
        std::cout << "\n";
    }
    if (!info.description.empty())
        std::cout << indent << "  " << info.description << "\n";

    if (info.type != ParameterType::OBJECT)
        return;
    for (const auto& child : info.object_members) {
        const std::string child_path = path.empty()
                                           ? child.name
                                           : path + "/" + child.name;
        printParameterInfo(module, child, child_path, depth + 1);
    }
}

int printModuleParameters(const ModuleMedia& module,
                          const std::string& display_name,
                          const std::string& path = "")
{
    std::cout << "Parameters for " << display_name;
    if (!path.empty())
        std::cout << " (" << path << ")";
    std::cout << ":\n";

    if (!path.empty()) {
        ParameterInfo info;
        const int ret = module.queryParameter(path, info);
        if (ret < 0) {
            std::cerr << "Parameter path '" << path << "' not found: "
                      << ret << " (" << errorText(ret) << ")\n";
            return ret;
        }
        printParameterInfo(module, info, path, 1);
        return 0;
    }

    const std::vector<ParameterInfo> roots = module.queryParameters();
    for (const auto& info : roots)
        printParameterInfo(module, info, info.name, 1);
    return 0;
}

struct RawParameterAssignment {
    std::string module_id;
    std::string path;
    std::string value;
    size_t order;
    size_t block;
};

class ParameterBlockParser
{
public:
    ParameterBlockParser(const std::string& text,
                         const std::string& module_id,
                         size_t& next_order,
                         size_t block,
                         std::vector<RawParameterAssignment>& output)
        : text_(text), module_id_(module_id), next_order_(next_order),
          block_(block), output_(output)
    {
    }

    int parse(std::string& error)
    {
        const size_t before = output_.size();
        const int ret = parseEntries("", '\0', error);
        if (ret < 0)
            return ret;
        skipWhitespace();
        if (position_ != text_.size()) {
            error = "unexpected trailing parameter text";
            return -EINVAL;
        }
        if (output_.size() == before) {
            error = "parameter block is empty";
            return -EINVAL;
        }
        return 0;
    }

private:
    void skipWhitespace()
    {
        while (position_ < text_.size()
               && std::isspace(static_cast<unsigned char>(text_[position_]))) {
            ++position_;
        }
    }

    std::string joinedPath(const std::string& prefix,
                           const std::string& name) const
    {
        return prefix.empty() ? name : prefix + "/" + name;
    }

    int parseEntries(const std::string& prefix, char terminator,
                     std::string& error)
    {
        size_t entry_count = 0;
        while (true) {
            skipWhitespace();
            if (position_ >= text_.size()) {
                if (terminator != '\0') {
                    error = std::string("missing closing '") + terminator
                            + "'";
                    return -EINVAL;
                }
                return 0;
            }
            if (terminator != '\0' && text_[position_] == terminator) {
                ++position_;
                if (entry_count == 0) {
                    error = "empty parameter object";
                    return -EINVAL;
                }
                return 0;
            }
            if (text_[position_] == ';') {
                ++position_;
                continue;
            }
            if (text_[position_] == '}') {
                error = "unexpected closing '}'";
                return -EINVAL;
            }

            const size_t name_start = position_;
            while (position_ < text_.size()) {
                const char ch = text_[position_];
                if (ch == '=' || ch == '{' || ch == ';' || ch == '}')
                    break;
                ++position_;
            }
            const std::string name = trim(text_.substr(name_start, position_ - name_start));
            if (name.empty()) {
                error = "parameter name is empty";
                return -EINVAL;
            }
            if (position_ >= text_.size()) {
                error = "parameter '" + name + "' is missing '=' or '{'";
                return -EINVAL;
            }

            const char operation = text_[position_++];
            const std::string path = joinedPath(prefix, name);
            if (operation == '{') {
                const size_t before = output_.size();
                const int ret = parseEntries(path, '}', error);
                if (ret < 0)
                    return ret;
                if (output_.size() == before) {
                    error = "parameter object '" + path + "' is empty";
                    return -EINVAL;
                }
            } else if (operation == '=') {
                std::string value;
                const int ret = parseValue(terminator, value, error);
                if (ret < 0)
                    return ret;
                RawParameterAssignment assignment;
                assignment.module_id = module_id_;
                assignment.path = path;
                assignment.value = value;
                assignment.order = next_order_++;
                assignment.block = block_;
                output_.push_back(assignment);
            } else {
                error = "parameter '" + name + "' is missing '=' or '{'";
                return -EINVAL;
            }
            ++entry_count;

            skipWhitespace();
            if (position_ >= text_.size()) {
                if (terminator != '\0') {
                    error = std::string("missing closing '") + terminator
                            + "'";
                    return -EINVAL;
                }
                return 0;
            }
            if (text_[position_] == ';') {
                ++position_;
                continue;
            }
            if (terminator != '\0' && text_[position_] == terminator)
                continue;
            error = std::string("expected ';'")
                    + (terminator == '\0'
                           ? ""
                           : std::string(" or '") + terminator + "'")
                    + " after parameter '" + path + "'";
            return -EINVAL;
        }
    }

    int parseValue(char terminator, std::string& value, std::string& error)
    {
        size_t first = position_;
        while (first < text_.size()
               && std::isspace(static_cast<unsigned char>(text_[first]))) {
            ++first;
        }

        const bool quoted = first < text_.size()
                            && (text_[first] == '"' || text_[first] == '\'');
        const char quote = quoted ? text_[first] : '\0';
        if (quoted)
            position_ = first + 1;

        std::string parsed;
        bool quote_closed = !quoted;
        while (position_ < text_.size()) {
            const char ch = text_[position_];
            if (ch == '\\') {
                if (position_ + 1 >= text_.size()) {
                    parsed.push_back(ch);
                    ++position_;
                    continue;
                }
                const char next = text_[position_ + 1];
                if (next == ';' || next == '{' || next == '}'
                    || next == '\\' || next == '"' || next == '\'') {
                    parsed.push_back(next);
                    position_ += 2;
                    continue;
                }
                parsed.push_back(ch);
                ++position_;
                continue;
            }
            if (quoted && ch == quote) {
                quote_closed = true;
                ++position_;
                break;
            }
            if (!quoted
                && (ch == ';'
                    || (terminator != '\0' && ch == terminator))) {
                break;
            }
            parsed.push_back(ch);
            ++position_;
        }

        if (!quote_closed) {
            error = "unterminated quoted parameter value";
            return -EINVAL;
        }
        if (quoted) {
            while (position_ < text_.size()
                   && std::isspace(
                       static_cast<unsigned char>(text_[position_]))) {
                ++position_;
            }
            if (position_ < text_.size()
                && text_[position_] != ';'
                && !(terminator != '\0'
                     && text_[position_] == terminator)) {
                error = "unexpected text after quoted parameter value";
                return -EINVAL;
            }
            value = parsed;
        } else {
            value = trim(parsed);
        }
        return 0;
    }

    const std::string& text_;
    std::string module_id_;
    size_t& next_order_;
    size_t block_;
    std::vector<RawParameterAssignment>& output_;
    size_t position_ = 0;
};

int parseParameterArgument(const std::string& argument,
                           size_t& next_order,
                           size_t block_id,
                           std::vector<RawParameterAssignment>& output,
                           std::string& error)
{
    const size_t separator = argument.find(':');
    if (separator == std::string::npos) {
        error = "parameter block must use ID:PARAMETERS";
        return -EINVAL;
    }
    const std::string module_id = trim(argument.substr(0, separator));
    if (!validIdentifier(module_id)) {
        error = "invalid module id in parameter block: '" + module_id + "'";
        return -EINVAL;
    }
    const std::string parameter_text = argument.substr(separator + 1);
    ParameterBlockParser parser(
        parameter_text, module_id, next_order, block_id, output);
    return parser.parse(error);
}

struct ModuleDeclaration {
    std::string id;
    std::string type;
};

int parseModuleDeclaration(const std::string& argument,
                           ModuleDeclaration& declaration,
                           std::string& error)
{
    const size_t separator = argument.find('=');
    if (separator == std::string::npos
        || separator != argument.rfind('=')) {
        error = "module declaration must use ID=TYPE";
        return -EINVAL;
    }
    declaration.id = trim(argument.substr(0, separator));
    declaration.type = trim(argument.substr(separator + 1));
    if (!validIdentifier(declaration.id)) {
        error = "invalid module id: '" + declaration.id + "'";
        return -EINVAL;
    }
    if (declaration.type.empty()) {
        error = "module type is empty";
        return -EINVAL;
    }
    return 0;
}

struct ConnectionSpec {
    std::string producer;
    std::string consumer;
    std::vector<MediaChannelId> channels;
};

int parseChannelId(const std::string& text, MediaChannelId& channel)
{
    if (text.empty())
        return -EINVAL;
    errno = 0;
    char* end = nullptr;
    const unsigned long long value = std::strtoull(text.c_str(), &end, 10);
    if (errno == ERANGE
        || value > std::numeric_limits<MediaChannelId>::max()) {
        return -ERANGE;
    }
    if (!end || *end != '\0')
        return -EINVAL;
    channel = static_cast<MediaChannelId>(value);
    return 0;
}

int parseConnection(const std::string& argument,
                    ConnectionSpec& connection,
                    std::string& error)
{
    const size_t separator = argument.find('=');
    if (separator == std::string::npos
        || separator != argument.rfind('=')) {
        error = "connection must use PRODUCER[@CHANNELS]=CONSUMER";
        return -EINVAL;
    }
    std::string producer = trim(argument.substr(0, separator));
    connection.consumer = trim(argument.substr(separator + 1));
    if (!validIdentifier(connection.consumer)) {
        error = "invalid consumer id: '" + connection.consumer + "'";
        return -EINVAL;
    }

    const size_t channel_separator = producer.find('@');
    if (channel_separator != std::string::npos
        && channel_separator != producer.rfind('@')) {
        error = "connection contains more than one '@'";
        return -EINVAL;
    }
    if (channel_separator == std::string::npos) {
        connection.producer = producer;
    } else {
        connection.producer = trim(producer.substr(0, channel_separator));
        const std::string channels = producer.substr(channel_separator + 1);
        if (channels.empty()) {
            error = "connection channel list is empty";
            return -EINVAL;
        }
        size_t start = 0;
        std::set<MediaChannelId> unique;
        while (start <= channels.size()) {
            const size_t comma = channels.find(',', start);
            const std::string item = trim(channels.substr(
                start, comma == std::string::npos
                           ? std::string::npos
                           : comma - start));
            MediaChannelId channel = 0;
            const int ret = parseChannelId(item, channel);
            if (ret < 0) {
                error = "invalid output channel id: '" + item + "'";
                return ret;
            }
            if (!unique.insert(channel).second) {
                error = "duplicate output channel id: '"
                        + std::to_string(channel) + "'";
                return -EEXIST;
            }
            connection.channels.push_back(channel);
            if (comma == std::string::npos)
                break;
            start = comma + 1;
        }
    }
    if (!validIdentifier(connection.producer)) {
        error = "invalid producer id: '" + connection.producer + "'";
        return -EINVAL;
    }
    return 0;
}

int parseDuration(const std::string& text, double& duration)
{
    if (text.empty())
        return -EINVAL;
    errno = 0;
    char* end = nullptr;
    const double value = std::strtod(text.c_str(), &end);
    if (errno == ERANGE)
        return -ERANGE;
    if (!end || *end != '\0' || !std::isfinite(value) || value < 0.0)
        return -EINVAL;
    duration = value;
    return 0;
}

const char* synchronizeTypeName(SynchronizeType type)
{
    switch (type) {
        case SYNCHRONIZETYPE_VIDEO:
            return "video";
        case SYNCHRONIZETYPE_AUDIO:
            return "audio";
        case SYNCHRONIZETYPE_ABSOLUTE:
            return "absolute";
    }
    return "unknown";
}

int parseSynchronizeType(const std::string& text, SynchronizeType& type)
{
    if (text == "video") {
        type = SYNCHRONIZETYPE_VIDEO;
        return 0;
    }
    if (text == "audio") {
        type = SYNCHRONIZETYPE_AUDIO;
        return 0;
    }
    if (text == "abs" || text == "absolute") {
        type = SYNCHRONIZETYPE_ABSOLUTE;
        return 0;
    }
    return -EINVAL;
}

int parseSynchronizeParameter(const std::string& text, SynchronizeType& type,
                              std::string& name)
{
    const size_t separator = text.find(':');
    const std::string mode = text.substr(0, separator);
    const int ret = parseSynchronizeType(mode, type);
    if (ret < 0)
        return ret;

    name.clear();
    if (separator == std::string::npos)
        return 0;

    name = text.substr(separator + 1);
    if (name.empty() || name.find(':') != std::string::npos
        || !validIdentifier(name)) {
        return -EINVAL;
    }
    return 0;
}

struct ShowTarget {
    std::string module_id;
    std::string path;
};

struct SynchronizeAssignment {
    std::string module_id;
    SynchronizeType type = SYNCHRONIZETYPE_AUDIO;
    std::string name;
};

int parseShowTarget(const std::string& argument, ShowTarget& target)
{
    const size_t separator = argument.find(':');
    target.module_id = trim(argument.substr(
        0, separator == std::string::npos ? argument.size() : separator));
    if (!validIdentifier(target.module_id))
        return -EINVAL;
    if (separator != std::string::npos) {
        target.path = trim(argument.substr(separator + 1));
        if (target.path.empty())
            return -EINVAL;
    }
    return 0;
}

int parseSynchronizeAssignment(const std::string& argument,
                               SynchronizeAssignment& assignment,
                               std::string& error)
{
    const size_t separator = argument.find('=');
    if (separator == std::string::npos
        || separator != argument.rfind('=')) {
        error = "sync must use MODULE=MODE[:NAME]";
        return -EINVAL;
    }

    assignment.module_id = trim(argument.substr(0, separator));
    if (!validIdentifier(assignment.module_id)) {
        error = "invalid module id in sync assignment: '"
                + assignment.module_id + "'";
        return -EINVAL;
    }

    const std::string setting = trim(argument.substr(separator + 1));
    const int ret = parseSynchronizeParameter(
        setting, assignment.type, assignment.name);
    if (ret < 0) {
        error = "sync must use video, audio, or absolute, optionally followed "
                "by :NAME";
    }
    return ret;
}

struct RunConfig {
    std::vector<ModuleDeclaration> modules;
    std::vector<RawParameterAssignment> parameters;
    std::vector<ConnectionSpec> connections;
    std::vector<ShowTarget> show_targets;
    std::vector<SynchronizeAssignment> synchronizers;
    double duration = 0.0;
};

void printGeneralUsage(const char* program)
{
    std::cout
        << "Usage:\n"
        << "  " << program << " modules\n"
        << "  " << program << " params TYPE [PATH]\n"
        << "  " << program << " run [options]\n\n"
        << "Use '" << program << " run --help' for pipeline syntax.\n";
}

void printRunUsage(const char* program)
{
    std::cout
        << "Usage: " << program << " run [options]\n\n"
        << "  -m, --module ID=TYPE\n"
        << "      Declare a module instance. Repeat for every graph node.\n"
        << "  -p, --params 'ID:ENTRIES'\n"
        << "      Configure one or more parameters. Entries use ';'.\n"
        << "      Flat:   'dec:fast=true;buffer-count=10'\n"
        << "      Object: 'gpu:output{width=1280;height=720;format=NV12;"
           "compression=linear}'\n"
        << "      V4L2 format fields accept names such as NV12 and RGB24.\n"
        << "  -c, --connect PRODUCER[@CHANNELS]=CONSUMER\n"
        << "      Connect modules; omitted channel list selects compatible outputs.\n"
        << "      Example: demux@0=decoder\n"
        << "      VideoStack: repeat one input-layout -p block per input;\n"
        << "      layouts and incoming connections are matched in declaration order.\n"
        << "      --show-params ID[:PATH]\n"
        << "      Print configured parameters and exit before init().\n"
        << "      --sync MODULE=MODE[:NAME]\n"
        << "      Configure module synchronization separately. Without NAME,\n"
        << "      the module gets a private synchronizer; matching NAME shares one.\n"
        << "  -d, --duration SECONDS\n"
        << "      Stop after the duration; zero waits for EOS or a signal.\n"
        << "  -h, --help\n\n"
        << "Example:\n"
        << "  " << program << " run \\\n"
        << "    -m src=ffmpeg-demux -m dec=mpp-dec -m out=file-writer \\\n"
        << "    -p 'src:source{uri=/data/input.mp4;loop=1}' \\\n"
        << "    -p 'out:path=/data/output.raw' \\\n"
        << "    -c src@0=dec -c dec=out\n";
}

int parseRunArguments(int argc, char** argv, RunConfig& config,
                      std::string& error)
{
    enum {
        OPT_SHOW_PARAMS = 1000,
        OPT_SYNC = 1001,
    };
    static const option options[] = {
        {"module", required_argument, nullptr, 'm'},
        {"params", required_argument, nullptr, 'p'},
        {"connect", required_argument, nullptr, 'c'},
        {"show-params", required_argument, nullptr, OPT_SHOW_PARAMS},
        {"sync", required_argument, nullptr, OPT_SYNC},
        {"duration", required_argument, nullptr, 'd'},
        {"help", no_argument, nullptr, 'h'},
        {nullptr, 0, nullptr, 0},
    };

    optind = 2;
    opterr = 0;
    size_t next_parameter_order = 0;
    size_t next_parameter_block = 0;
    int value = 0;
    while ((value = getopt_long(
                argc, argv, "+m:p:c:d:h", options, nullptr))
           != -1) {
        switch (value) {
            case 'm': {
                ModuleDeclaration declaration;
                const int ret = parseModuleDeclaration(optarg, declaration, error);
                if (ret < 0)
                    return ret;
                config.modules.push_back(declaration);
                break;
            }
            case 'p': {
                const int ret = parseParameterArgument(
                    optarg, next_parameter_order, next_parameter_block++,
                    config.parameters, error);
                if (ret < 0)
                    return ret;
                break;
            }
            case 'c': {
                ConnectionSpec connection;
                const int ret = parseConnection(optarg, connection, error);
                if (ret < 0)
                    return ret;
                config.connections.push_back(connection);
                break;
            }
            case 'd': {
                const int ret = parseDuration(optarg, config.duration);
                if (ret < 0) {
                    error = "invalid duration: '" + std::string(optarg) + "'";
                    return ret;
                }
                break;
            }
            case OPT_SHOW_PARAMS: {
                ShowTarget target;
                const int ret = parseShowTarget(optarg, target);
                if (ret < 0) {
                    error = "invalid --show-params target: '"
                            + std::string(optarg) + "'";
                    return ret;
                }
                config.show_targets.push_back(target);
                break;
            }
            case OPT_SYNC: {
                SynchronizeAssignment assignment;
                const int ret = parseSynchronizeAssignment(
                    optarg, assignment, error);
                if (ret < 0)
                    return ret;
                config.synchronizers.push_back(std::move(assignment));
                break;
            }
            case 'h':
                return 1;
            case '?':
            default:
                error = "unknown or incomplete command-line option";
                return -EINVAL;
        }
    }
    if (optind != argc) {
        error = "unexpected positional argument: '"
                + std::string(argv[optind]) + "'";
        return -EINVAL;
    }
    return 0;
}

struct ParameterMeta {
    ParameterInfo info;
    std::string atomic_root;
};

void collectParameterMetadata(const ParameterInfo& info,
                              const std::string& path,
                              const std::string& parent_atomic,
                              std::map<std::string, ParameterMeta>& metadata)
{
    std::string atomic_root = parent_atomic;
    if (atomic_root.empty()
        && info.type == ParameterType::OBJECT && info.atomic) {
        atomic_root = path;
    }

    ParameterMeta meta;
    meta.info = info;
    meta.atomic_root = atomic_root;
    metadata[path] = meta;

    for (const auto& child : info.object_members) {
        collectParameterMetadata(
            child, path + "/" + child.name, atomic_root, metadata);
    }
}

std::map<std::string, ParameterMeta> parameterMetadata(
    const ModuleMedia& module)
{
    std::map<std::string, ParameterMeta> metadata;
    for (const auto& root : module.queryParameters())
        collectParameterMetadata(root, root.name, "", metadata);
    return metadata;
}

int parameterValueFromText(const ParameterInfo& info,
                           const std::string& text,
                           ParameterValue& value)
{
    if (info.type == ParameterType::OBJECT)
        return -ENOTSUP;
    if (info.type == ParameterType::INTEGER && !info.enum_values.empty()) {
        auto found = std::find_if(
            info.enum_values.begin(), info.enum_values.end(),
            [&text](const ParameterEnumValue& item) {
                return item.name == text;
            });
        if (found != info.enum_values.end()) {
            value = ParameterValue(found->value);
            return 0;
        }
    }

    const int ret = ParameterValue::fromString(info.type, text, value);
    if (ret != -EINVAL || !isV4l2FormatParameter(info))
        return ret;

    const uint32_t format = v4l2GetFmtByName(text.c_str());
    if (!format)
        return -EINVAL;
    value = ParameterValue(static_cast<int64_t>(format));
    return 0;
}

int splitParameterPath(const std::string& path,
                       std::vector<std::string>& parts)
{
    if (path.empty())
        return -EINVAL;
    size_t start = 0;
    while (start <= path.size()) {
        const size_t separator = path.find('/', start);
        const std::string part = path.substr(
            start, separator == std::string::npos
                       ? std::string::npos
                       : separator - start);
        if (part.empty())
            return -EINVAL;
        parts.push_back(part);
        if (separator == std::string::npos)
            break;
        start = separator + 1;
    }
    return 0;
}

int setNestedObjectValue(ParameterObject& object,
                         const std::vector<std::string>& parts,
                         size_t index, const ParameterValue& value)
{
    if (index >= parts.size())
        return -EINVAL;
    if (index + 1 == parts.size()) {
        object.setMember(parts[index], value);
        return 0;
    }

    ParameterObject child;
    ParameterValue current;
    if (object.getMember(parts[index], current) == 0)
        current.getObject(child);
    const int ret = setNestedObjectValue(child, parts, index + 1, value);
    if (ret < 0)
        return ret;
    object.setMember(parts[index], child);
    return 0;
}

struct ParameterOperation {
    size_t order = 0;
    bool object_patch = false;
    std::string path;
    ParameterValue value;
    ParameterObject patch;
    std::vector<std::string> affected_paths;
};

struct ModuleInstance {
    std::string id;
    const ModuleDescriptor* descriptor = nullptr;
    std::shared_ptr<ModuleMedia> module;
    std::vector<RawParameterAssignment> assignments;
    bool sync_configured = false;
    SynchronizeType sync_type = SYNCHRONIZETYPE_AUDIO;
    std::string sync_name;
    std::shared_ptr<Synchronize> synchronizer;
};

void printParameterFailure(const ModuleInstance& instance,
                           const std::vector<std::string>& paths,
                           int ret)
{
    std::cerr << "Failed to configure module '" << instance.id
              << "' (" << instance.descriptor->type << ")";
    if (!paths.empty()) {
        std::cerr << " parameter";
        if (paths.size() > 1)
            std::cerr << "s";
        std::cerr << ":";
        for (const auto& path : paths)
            std::cerr << " " << path;
    }
    std::cerr << ", ret=" << ret << " (" << errorText(ret) << ")\n";
    printModuleParameters(
        *instance.module,
        instance.id + "=" + instance.descriptor->type);
}

int configureModule(ModuleInstance& instance)
{
    if (instance.assignments.empty())
        return 0;

    const std::map<std::string, ParameterMeta> metadata = parameterMetadata(*instance.module);

    std::vector<const RawParameterAssignment*> effective;
    std::set<std::string> assigned_paths;
    for (const auto& assignment : instance.assignments) {
        auto found = metadata.find(assignment.path);
        if (found == metadata.end()) {
            const int ret = -ENOENT;
            printParameterFailure(
                instance, std::vector<std::string>{assignment.path}, ret);
            return ret;
        }
        const bool repeatable_input_layout = instance.descriptor->type == "video-stack"
                                             && found->second.atomic_root == "input-layout";
        std::string assignment_key = assignment.path;
        if (repeatable_input_layout) {
            assignment_key = std::to_string(assignment.block)
                             + ":" + assignment.path;
        }
        if (!assigned_paths.insert(assignment_key).second) {
            const int ret = -EEXIST;
            printParameterFailure(
                instance, std::vector<std::string>{assignment.path}, ret);
            return ret;
        }
        effective.push_back(&assignment);
    }

    std::vector<ParameterOperation> operations;
    std::map<std::string, size_t> object_operations;
    for (const auto* assignment : effective) {
        auto found = metadata.find(assignment->path);
        if (found == metadata.end()) {
            const int ret = -ENOENT;
            printParameterFailure(
                instance, std::vector<std::string>{assignment->path}, ret);
            return ret;
        }
        const ParameterMeta& meta = found->second;
        if (!(meta.info.flags & PARAMETER_FLAG_WRITABLE)) {
            const int ret = -EACCES;
            printParameterFailure(
                instance, std::vector<std::string>{assignment->path}, ret);
            return ret;
        }
        if (meta.info.type == ParameterType::OBJECT) {
            const int ret = -ENOTSUP;
            printParameterFailure(
                instance, std::vector<std::string>{assignment->path}, ret);
            std::cerr << "Use child paths or OBJECT blocks instead of assigning "
                         "a string to an OBJECT parameter.\n";
            return ret;
        }

        ParameterValue value;
        const int parse_ret = parameterValueFromText(meta.info, assignment->value, value);
        if (parse_ret < 0) {
            printParameterFailure(
                instance, std::vector<std::string>{assignment->path},
                parse_ret);
            return parse_ret;
        }

        if (meta.atomic_root.empty()) {
            ParameterOperation operation;
            operation.order = assignment->order;
            operation.path = assignment->path;
            operation.value = value;
            operation.affected_paths.push_back(assignment->path);
            operations.push_back(operation);
            continue;
        }

        size_t operation_index = 0;
        const bool repeatable_input_layout = instance.descriptor->type == "video-stack"
                                             && meta.atomic_root == "input-layout";
        std::string operation_key = meta.atomic_root;
        if (repeatable_input_layout) {
            operation_key += ":" + std::to_string(assignment->block);
        }
        auto operation_found = object_operations.find(operation_key);
        if (operation_found == object_operations.end()) {
            ParameterOperation operation;
            operation.order = assignment->order;
            operation.object_patch = true;
            operation.path = meta.atomic_root;
            operations.push_back(operation);
            operation_index = operations.size() - 1;
            object_operations[operation_key] = operation_index;
        } else {
            operation_index = operation_found->second;
        }

        std::string relative = assignment->path.substr(
            meta.atomic_root.size());
        if (!relative.empty() && relative.front() == '/')
            relative.erase(relative.begin());
        std::vector<std::string> parts;
        int ret = splitParameterPath(relative, parts);
        if (ret == 0) {
            ret = setNestedObjectValue(
                operations[operation_index].patch, parts, 0, value);
        }
        if (ret < 0) {
            printParameterFailure(
                instance, std::vector<std::string>{assignment->path}, ret);
            return ret;
        }
        operations[operation_index].affected_paths.push_back(
            assignment->path);
    }

    std::sort(operations.begin(), operations.end(),
              [](const ParameterOperation& lhs,
                 const ParameterOperation& rhs) {
                  return lhs.order < rhs.order;
              });

    for (const auto& operation : operations) {
        const int ret = operation.object_patch
                            ? instance.module->setParameter(
                                operation.path, operation.patch)
                            : instance.module->setParameter(
                                operation.path, operation.value);
        if (ret < 0) {
            printParameterFailure(
                instance, operation.affected_paths, ret);
            return ret;
        }
    }
    return 0;
}

int createModuleInstances(
    const RunConfig& config, std::vector<ModuleInstance>& instances,
    std::map<std::string, size_t>& index_by_id)
{
    if (config.modules.empty()) {
        std::cerr << "At least one -m/--module declaration is required.\n";
        return -EINVAL;
    }

    for (const auto& declaration : config.modules) {
        if (index_by_id.count(declaration.id)) {
            std::cerr << "Duplicate module id: '" << declaration.id << "'\n";
            return -EEXIST;
        }
        const ModuleDescriptor* descriptor = findModuleDescriptor(declaration.type);
        if (!descriptor) {
            std::cerr << "Unknown or disabled module type: '"
                      << declaration.type << "'\n";
            return -ENOENT;
        }
        ModuleInstance instance;
        instance.id = declaration.id;
        instance.descriptor = descriptor;
        instance.module = descriptor->factory();
        index_by_id[instance.id] = instances.size();
        instances.push_back(instance);
    }

    for (const auto& assignment : config.parameters) {
        auto found = index_by_id.find(assignment.module_id);
        if (found == index_by_id.end()) {
            std::cerr << "Parameter block references unknown module id: '"
                      << assignment.module_id << "'\n";
            return -ENOENT;
        }
        instances[found->second].assignments.push_back(assignment);
    }

    for (const auto& assignment : config.synchronizers) {
        auto found = index_by_id.find(assignment.module_id);
        if (found == index_by_id.end()) {
            std::cerr << "Sync assignment references unknown module id: '"
                      << assignment.module_id << "'\n";
            return -ENOENT;
        }
        ModuleInstance& instance = instances[found->second];
        if (instance.sync_configured) {
            std::cerr << "Duplicate sync assignment for module '"
                      << instance.id << "'\n";
            return -EEXIST;
        }
        instance.sync_configured = true;
        instance.sync_type = assignment.type;
        instance.sync_name = assignment.name;
    }

    for (auto& instance : instances) {
        const int ret = configureModule(instance);
        if (ret < 0)
            return ret;
    }
    return 0;
}

int configureModuleSynchronizers(std::vector<ModuleInstance>& instances)
{
    std::map<std::string, std::shared_ptr<Synchronize>> synchronizers;
    std::map<std::string, SynchronizeType> synchronize_types;

    for (auto& instance : instances) {
        if (!instance.sync_configured)
            continue;

        const std::string key = instance.sync_name.empty()
                                    ? "module:" + instance.id
                                    : "name:" + instance.sync_name;
        auto found = synchronizers.find(key);
        if (found == synchronizers.end()) {
            instance.synchronizer = std::make_shared<Synchronize>(instance.sync_type);
            instance.synchronizer->setFirstFrameDuration(50000);
            synchronizers.emplace(key, instance.synchronizer);
            synchronize_types.emplace(key, instance.sync_type);
        } else {
            if (synchronize_types.at(key) != instance.sync_type) {
                std::cerr << "Sync name '" << key
                          << "' uses conflicting modes: "
                          << synchronizeTypeName(synchronize_types.at(key))
                          << " and "
                          << synchronizeTypeName(instance.sync_type) << "\n";
                return -EINVAL;
            }
            instance.synchronizer = found->second;
        }
        instance.module->setSynchronize(instance.synchronizer);
    }
    return 0;
}

std::string synchronizeParameterValue(const ModuleInstance& instance)
{
    if (!instance.sync_configured)
        return "";
    std::string value = synchronizeTypeName(instance.sync_type);
    if (!instance.sync_name.empty())
        value += ":" + instance.sync_name;
    return value;
}

struct GraphPlan {
    std::vector<size_t> topological_order;
    std::vector<size_t> roots;
    std::vector<std::vector<size_t>> incoming_connections;
};

int moduleIsSource(const ModuleInstance& instance, bool& source)
{
    source = instance.descriptor->source;
    if (!instance.descriptor->dynamic_source)
        return 0;

    bool configured_source = false;
    const int ret = instance.module->getParameter(
        "source/publish", configured_source);
    if (ret < 0)
        return ret;
    source = configured_source;
    return 0;
}

int buildGraphPlan(const RunConfig& config,
                   const std::vector<ModuleInstance>& instances,
                   const std::map<std::string, size_t>& index_by_id,
                   GraphPlan& plan)
{
    const size_t count = instances.size();
    plan.incoming_connections.assign(count, std::vector<size_t>());
    std::vector<std::vector<size_t>> outgoing(count);
    std::vector<size_t> indegree(count, 0);
    std::set<std::pair<size_t, size_t>> edges;

    for (size_t edge_index = 0;
         edge_index < config.connections.size(); ++edge_index) {
        const ConnectionSpec& connection = config.connections[edge_index];
        auto producer_found = index_by_id.find(connection.producer);
        auto consumer_found = index_by_id.find(connection.consumer);
        if (producer_found == index_by_id.end()
            || consumer_found == index_by_id.end()) {
            std::cerr << "Connection references unknown module: "
                      << connection.producer << "="
                      << connection.consumer << "\n";
            return -ENOENT;
        }
        const size_t producer = producer_found->second;
        const size_t consumer = consumer_found->second;
        if (producer == consumer) {
            std::cerr << "Self connection is not allowed: "
                      << connection.producer << "\n";
            return -EINVAL;
        }
        if (!edges.insert(std::make_pair(producer, consumer)).second) {
            std::cerr << "Duplicate connection: "
                      << connection.producer << "="
                      << connection.consumer << "\n";
            return -EEXIST;
        }
        outgoing[producer].push_back(consumer);
        plan.incoming_connections[consumer].push_back(edge_index);
        ++indegree[consumer];
    }

    for (size_t i = 0; i < count; ++i) {
        if (!instances[i].descriptor->graph_supported) {
            std::cerr << "Module type '" << instances[i].descriptor->type
                      << "' requires a specialized application adapter and "
                         "cannot be used by the generic graph runner.\n";
            return -ENOTSUP;
        }
        if (instances[i].descriptor->type == "video-stack") {
            const auto& requirements = instances[i].module->getInputMediaChannelRequirements();
            if (requirements.empty()) {
                std::cerr << "VideoStack module '" << instances[i].id
                          << "' requires at least one input-layout parameter.\n";
                return -EINVAL;
            }
            if (plan.incoming_connections[i].size() > requirements.size()) {
                std::cerr << "VideoStack module '" << instances[i].id
                          << "' has more incoming connections than configured "
                             "input-layout entries.\n";
                return -EINVAL;
            }
        }
        bool source = false;
        const int source_ret = moduleIsSource(instances[i], source);
        if (source_ret < 0) {
            std::cerr << "Failed to determine graph role for module '"
                      << instances[i].id << "', ret=" << source_ret
                      << " (" << errorText(source_ret) << ")\n";
            return source_ret;
        }
        if (source && indegree[i] != 0) {
            std::cerr << "Source module '" << instances[i].id
                      << "' cannot have an incoming connection.\n";
            return -EINVAL;
        }
        if (!source && indegree[i] == 0) {
            std::cerr << "Processing/output module '" << instances[i].id
                      << "' has no producer.\n";
            return -EINVAL;
        }
        if (indegree[i] == 0)
            plan.roots.push_back(i);
    }

    std::vector<size_t> remaining = indegree;
    std::vector<size_t> ready = plan.roots;
    size_t ready_index = 0;
    while (ready_index < ready.size()) {
        const size_t node = ready[ready_index++];
        plan.topological_order.push_back(node);
        for (size_t consumer : outgoing[node]) {
            if (--remaining[consumer] == 0)
                ready.push_back(consumer);
        }
    }
    if (plan.topological_order.size() != count) {
        std::cerr << "Module graph contains a cycle.\n";
        return -ELOOP;
    }
    if (plan.roots.empty()) {
        std::cerr << "Module graph has no source root.\n";
        return -EINVAL;
    }
    return 0;
}

void printConnectionDetails(const ModuleInstance& producer,
                            const ModuleInstance& consumer)
{
    std::cerr << "Producer output channels:\n";
    for (const auto& channel : producer.module->getOutputMediaChannels()) {
        std::cerr << "  id=" << channel.id << " name=" << channel.name
                  << " media=" << static_cast<int>(channel.media_type)
                  << " codec=" << static_cast<int>(channel.codec) << "\n";
    }
    std::cerr << "Consumer input requirements:\n";
    for (const auto& requirement :
         consumer.module->getInputMediaChannelRequirements()) {
        std::cerr << "  id=" << requirement.input_id
                  << " name=" << requirement.name
                  << " media=" << static_cast<int>(requirement.media_type)
                  << " allow-multiple="
                  << (requirement.allow_multiple ? "true" : "false")
                  << "\n";
    }
}

int initializeGraph(const RunConfig& config,
                    std::vector<ModuleInstance>& instances,
                    const std::map<std::string, size_t>& index_by_id,
                    const GraphPlan& plan)
{
    for (size_t node : plan.topological_order) {
        ModuleInstance& consumer = instances[node];
        for (size_t connection_index :
             plan.incoming_connections[node]) {
            const ConnectionSpec& connection = config.connections[connection_index];
            const size_t producer_index = index_by_id.at(connection.producer);
            ModuleInstance& producer = instances[producer_index];
            MediaChannelSelection selection;
            selection.output_ids = connection.channels;

            int ret = 0;
            try {
                ret = consumer.module->connectProducer(
                    producer.module, selection);
            } catch (const std::exception& exception) {
                std::cerr << "Connection " << connection.producer << "="
                          << connection.consumer << " threw: "
                          << exception.what() << "\n";
                ret = -EINVAL;
            }
            if (ret < 0) {
                std::cerr << "Failed to connect " << connection.producer
                          << "=" << connection.consumer << ", ret="
                          << ret << " (" << errorText(ret) << ")\n";
                printConnectionDetails(producer, consumer);
                printModuleParameters(
                    *consumer.module,
                    consumer.id + "=" + consumer.descriptor->type);
                return ret;
            }
        }

        const int ret = consumer.module->init();
        if (ret < 0) {
            std::cerr << "Failed to initialize module '" << consumer.id
                      << "' (" << consumer.descriptor->type << "), ret="
                      << ret << " (" << errorText(ret) << ")\n";
            printModuleParameters(
                *consumer.module,
                consumer.id + "=" + consumer.descriptor->type);
            return ret;
        }
    }
    return 0;
}

void printGraph(const RunConfig& config,
                const std::vector<ModuleInstance>& instances)
{
    std::cout << "FFMedia pipeline:\n";
    for (const auto& instance : instances) {
        std::cout << "  " << instance.id << " = "
                  << instance.descriptor->type << "\n";
    }
    std::cout << "Connections:\n";
    for (const auto& connection : config.connections) {
        std::cout << "  " << connection.producer;
        if (!connection.channels.empty()) {
            std::cout << "@";
            for (size_t i = 0; i < connection.channels.size(); ++i) {
                if (i)
                    std::cout << ",";
                std::cout << connection.channels[i];
            }
        }
        std::cout << " -> " << connection.consumer << "\n";
    }
    for (const auto& instance : instances) {
        if (instance.sync_configured) {
            std::cout << "  sync " << instance.id << "="
                      << synchronizeParameterValue(instance) << "\n";
        }
    }
}

class PipelineRunner
{
public:
    PipelineRunner(std::vector<ModuleInstance>& instances,
                   const GraphPlan& plan)
        : instances_(instances)
    {
        for (size_t root : plan.roots) {
            roots_.push_back(instances_[root].module);
            root_ids_.insert(instances_[root].id);
        }
    }

    void start()
    {
        for (auto& instance : instances_) {
            const std::string id = instance.id;
            const bool is_root = root_ids_.count(id) != 0;
            if (!instance.module->setMediaStatusChangeHooker(
                    [this, id, is_root](const std::string&, MediaStatus status) {
                        onStatus(id, is_root, status);
                    })) {
                throw std::runtime_error(
                    "failed to install status hook for " + id);
            }
        }

        started_ = true;
        try {
            for (const auto& root : roots_) {
                root->start();
                root->dumpPipe();
            }
        } catch (...) {
            stop();
            throw;
        }
    }

    void wait(double duration)
    {
        const auto start_time = std::chrono::steady_clock::now();
        while (!stop_requested_.load() && !g_signal_stop) {
            if (duration > 0.0) {
                const double elapsed = std::chrono::duration<double>(
                                           std::chrono::steady_clock::now() - start_time)
                                           .count();
                if (elapsed >= duration)
                    break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if (all_roots_eos_.load())
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    void stop()
    {
        stop_requested_ = true;
        if (!started_)
            return;
        if (roots_.size() == 1) {
            roots_.front()->stop();
        } else {
            std::vector<std::thread> stop_threads;
            stop_threads.reserve(roots_.size());
            for (const auto& root : roots_) {
                stop_threads.emplace_back([root]() { root->stop(); });
            }
            for (auto& thread : stop_threads) {
                if (thread.joinable())
                    thread.join();
            }
        }
        for (const auto& root : roots_)
            root->dumpPipeSummary();
        started_ = false;
    }

    bool failed() const
    {
        return failed_.load();
    }

    ~PipelineRunner()
    {
        stop();
    }

private:
    void onStatus(const std::string& id, bool is_root,
                  MediaStatus status)
    {
        std::cout << "[status] " << id << " -> "
                  << statusName(status) << "\n";
        if (status == MediaStatus::ABNORMAL) {
            failed_ = true;
            stop_requested_ = true;
            return;
        }
        if (status != MediaStatus::EOS || !is_root)
            return;

        std::lock_guard<std::mutex> lock(eos_mutex_);
        eos_roots_.insert(id);
        if (eos_roots_.size() == roots_.size()) {
            all_roots_eos_ = true;
            stop_requested_ = true;
        }
    }

    std::vector<ModuleInstance>& instances_;
    std::vector<std::shared_ptr<ModuleMedia>> roots_;
    std::set<std::string> root_ids_;
    std::set<std::string> eos_roots_;
    std::mutex eos_mutex_;
    std::atomic_bool stop_requested_{false};
    std::atomic_bool failed_{false};
    std::atomic_bool all_roots_eos_{false};
    bool started_ = false;
};

int modulesCommand(int argc, char** argv)
{
    if (argc == 3
        && (std::string(argv[2]) == "-h"
            || std::string(argv[2]) == "--help")) {
        std::cout << "Usage: " << argv[0] << " modules\n";
        return 0;
    }
    if (argc != 2) {
        std::cerr << "The modules command does not accept arguments.\n";
        std::cout << "Usage: " << argv[0] << " modules\n";
        return 1;
    }
    std::cout << std::left
              << std::setw(20) << "TYPE"
              << std::setw(6) << "CLASS"
              << std::setw(10) << "GRAPH"
              << "DESCRIPTION\n";
    for (const auto& descriptor : moduleDescriptors()) {
        std::cout << std::left
                  << std::setw(20) << descriptor.type
                  << std::setw(6) << descriptor.category
                  << std::setw(10)
                  << (descriptor.graph_supported ? "yes" : "special")
                  << descriptor.description << "\n";
    }
    return 0;
}

int paramsCommand(int argc, char** argv)
{
    const bool help = (argc == 3
                       && (std::string(argv[2]) == "-h"
                           || std::string(argv[2]) == "--help"))
                      || (argc == 4
                          && (std::string(argv[3]) == "-h"
                              || std::string(argv[3]) == "--help"));
    if (argc < 3 || argc > 4 || help) {
        std::cout << "Usage: " << argv[0] << " params TYPE [PATH]\n";
        return help ? 0 : 1;
    }
    const std::string type = argv[2];
    const ModuleDescriptor* descriptor = findModuleDescriptor(type);
    if (!descriptor) {
        std::cerr << "Unknown or disabled module type: '" << type << "'\n";
        return 1;
    }
    const std::shared_ptr<ModuleMedia> module = descriptor->factory();
    const std::string path = argc == 4 ? argv[3] : "";
    return printModuleParameters(*module, type, path) < 0 ? 1 : 0;
}

int showConfiguredParameters(
    const RunConfig& config,
    const std::vector<ModuleInstance>& instances,
    const std::map<std::string, size_t>& index_by_id)
{
    for (const auto& target : config.show_targets) {
        auto found = index_by_id.find(target.module_id);
        if (found == index_by_id.end()) {
            std::cerr << "--show-params references unknown module id: '"
                      << target.module_id << "'\n";
            return -ENOENT;
        }
        const ModuleInstance& instance = instances[found->second];
        const int ret = printModuleParameters(
            *instance.module,
            instance.id + "=" + instance.descriptor->type,
            target.path);
        if (ret < 0)
            return ret;
    }
    return 0;
}

int runCommand(int argc, char** argv)
{
    RunConfig config;
    std::string error;
    const int parse_ret = parseRunArguments(argc, argv, config, error);
    if (parse_ret == 1) {
        printRunUsage(argv[0]);
        return 0;
    }
    if (parse_ret < 0) {
        std::cerr << "Command-line error: " << error << ", ret="
                  << parse_ret << " (" << errorText(parse_ret) << ")\n";
        printRunUsage(argv[0]);
        return 1;
    }

    std::vector<ModuleInstance> instances;
    std::map<std::string, size_t> index_by_id;
    int ret = createModuleInstances(
        config, instances, index_by_id);
    if (ret < 0)
        return 1;
    ret = configureModuleSynchronizers(instances);
    if (ret < 0)
        return 1;

    if (!config.show_targets.empty()) {
        return showConfiguredParameters(
                   config, instances, index_by_id)
                       < 0
                   ? 1
                   : 0;
    }

    GraphPlan plan;
    ret = buildGraphPlan(config, instances, index_by_id, plan);
    if (ret < 0)
        return 1;
    ret = initializeGraph(config, instances, index_by_id, plan);
    if (ret < 0)
        return 1;

    printGraph(config, instances);
    PipelineRunner pipeline(instances, plan);
    pipeline.start();
    pipeline.wait(config.duration);
    pipeline.stop();
    return pipeline.failed() ? 1 : 0;
}

}  // namespace

int main(int argc, char** argv)
{
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    if (argc < 2) {
        printGeneralUsage(argv[0]);
        return 1;
    }

    const std::string command = argv[1];
    try {
        if (command == "modules")
            return modulesCommand(argc, argv);
        if (command == "params")
            return paramsCommand(argc, argv);
        if (command == "run")
            return runCommand(argc, argv);
        if (command == "-h" || command == "--help") {
            printGeneralUsage(argv[0]);
            return 0;
        }
        std::cerr << "Unknown command: '" << command << "'\n";
        printGeneralUsage(argv[0]);
        return 1;
    } catch (const std::exception& exception) {
        std::cerr << "ffmedia failed: " << exception.what() << "\n";
        return 1;
    }
}
