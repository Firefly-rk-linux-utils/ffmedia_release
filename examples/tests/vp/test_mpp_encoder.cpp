#include "module/ff_media_parameter.hpp"
#include "module/vi/module_memReader.hpp"
#include "module/vp/module_mppenc.hpp"
#include "module/vp/module_rga.hpp"
#include "tests/module_test_utils.hpp"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using namespace FFMedia;
using namespace FFMediaTest;

namespace
{
constexpr uint32_t kSplitNone = 0;
constexpr uint32_t kSplitByByte = 1;
constexpr uint32_t kSplitByCtu = 2;
constexpr uint32_t kSplitOutLowDelay = 1U << 0;
constexpr uint32_t kSplitOutSegment = 1U << 1;
constexpr uint32_t kSplitOutMask = kSplitOutLowDelay | kSplitOutSegment;

constexpr int kNoFrame = -1;

struct RoiOptions {
    uint32_t x = 0;
    uint32_t y = 0;
    uint32_t width = 1;
    uint32_t height = 1;
    int64_t qp = 0;
    bool relative = false;
    bool intra = false;
};

struct EncoderOptions {
    media_codec_t codec = MEDIA_CODEC_VIDEO_H265;
    EncodeRcMode rc_mode = ENCODE_RC_MODE_CBR;
    EncodeProfile profile = ENCODE_PROFILE_HIGH;
    VideoBuffer::BUFFER_TYPE buffer_type = VideoBuffer::DRM_BUFFER_NONCACHEABLE;

    uint32_t width = 1920;
    uint32_t height = 1080;
    uint32_t hstride = 1920;
    uint32_t vstride = 1080;
    uint32_t format = 0;

    int fps = 30;
    int gop = 60;
    int bps = 2048;
    float quality = 0.8f;
    int64_t duration_us = 0;
    int output_timeout_ms = -1;
    int cache_frames = 0;
    int buffers = 8;
    int parallel_buffers = 0;

    uint64_t max_frames = 300;
    double duration_sec = 0.0;
    int wait_timeout_ms = 2000;
    int drain_timeout_ms = 2000;
    uint64_t report_every = 100;
    bool animate = false;
    bool verbose = false;
    bool external_consumer = false;
    bool dump_pipe = false;

    // Optional codec controls. --full-api enables every applicable control.
    bool full_api = false;
    bool use_intra_refresh = false;
    int refresh_mode = 0;
    int refresh_num = 10;

    bool use_split = false;
    uint32_t split_mode = kSplitNone;
    uint32_t split_arg = 0;
    uint32_t split_out = 0;

    bool use_sei_payload = false;
    std::vector<uint8_t> sei_payload;
    std::vector<uint8_t> sei_uuid;

    bool use_roi = false;
    RoiOptions roi;

    // Runtime API checks. A negative value disables an individual check.
    int idr_frame = kNoFrame;
    int reconfigure_frame = kNoFrame;
    int intra_refresh_frame = kNoFrame;
    int split_frame = kNoFrame;
    int sei_payload_frame = kNoFrame;
    int roi_frame = kNoFrame;
    int reconfigure_fps = 30;
    int reconfigure_gop = 60;
    int reconfigure_bps = 2048;
    EncodeRcMode reconfigure_rc_mode = ENCODE_RC_MODE_CBR;
    EncodeProfile reconfigure_profile = ENCODE_PROFILE_HIGH;
    float reconfigure_quality = 0.8f;
};

void usage(const char* program)
{
    ff_info(
        "Usage: %s OUTPUT [options]\n"
        "  --codec h264|h265|mjpeg --width 1920 --height 1080\n"
        "  --format nv12 [--hstride N --vstride N]\n"
        "  --fps 30 --gop 60 --bps 2048 --rc cbr|vbr|fixqp|avbr\n"
        "  --quality 0.8 --profile baseline|main|high\n"
        "  --duration-us N --output-timeout-ms -1 --cache-frames 0\n"
        "  --intra-refresh --refresh-mode 0 --refresh-num 10\n"
        "  --split-mode none|byte|ctu --split-arg N"
        " --split-out none|lowdelay|segment|lowdelay-segment\n"
        "  --sei-text TEXT\n"
        "  --sei-hex HEX --sei-uuid 32_HEX_DIGITS\n"
        "  --roi x,y,width,height,qp [--roi-relative] [--roi-intra]\n"
        "  --full-api   Exercise all applicable runtime APIs\n"
        "  --idr-frame N --reconfigure-frame N\n"
        "  --parallel-buffers 0   Insert RGA queue for parallel encoding\n"
        "  --buffer-type noncache|cache|malloc|dma32|cache-dma32\n"
        "  --frames 300 --wait-timeout-ms 2000 --drain-timeout-ms 2000 --animate\n"
        "  --report-every 100 --verbose --external-consumer --dump-pipe\n",
        program);
}

bool parseCodec(const std::string& name, media_codec_t& codec)
{
    if (name == "h264")
        codec = MEDIA_CODEC_VIDEO_H264;
    else if (name == "h265")
        codec = MEDIA_CODEC_VIDEO_H265;
    else if (name == "mjpeg")
        codec = MEDIA_CODEC_VIDEO_MJPEG;
    else
        return false;
    return true;
}

bool parseRcMode(const std::string& name, EncodeRcMode& mode)
{
    if (name == "cbr")
        mode = ENCODE_RC_MODE_CBR;
    else if (name == "vbr")
        mode = ENCODE_RC_MODE_VBR;
    else if (name == "fixqp")
        mode = ENCODE_RC_MODE_FIXQP;
    else if (name == "avbr")
        mode = ENCODE_RC_MODE_AVBR;
    else
        return false;
    return true;
}

bool parseProfile(const std::string& name, EncodeProfile& profile)
{
    if (name == "baseline")
        profile = ENCODE_PROFILE_BASELINE;
    else if (name == "main")
        profile = ENCODE_PROFILE_MAIN;
    else if (name == "high")
        profile = ENCODE_PROFILE_HIGH;
    else
        return false;
    return true;
}

bool parseBufferType(const std::string& name, VideoBuffer::BUFFER_TYPE& type)
{
    if (name == "noncache")
        type = VideoBuffer::DRM_BUFFER_NONCACHEABLE;
    else if (name == "cache")
        type = VideoBuffer::DRM_BUFFER_CACHEABLE;
    else if (name == "malloc")
        type = VideoBuffer::MALLOC_BUFFER;
    else if (name == "dma32")
        type = VideoBuffer::DRM_BUFFER_NONCACHEABLE_DMA32;
    else if (name == "cache-dma32")
        type = VideoBuffer::DRM_BUFFER_CACHEABLE_DMA32;
    else
        return false;
    return true;
}

bool parseSplitMode(const std::string& name, uint32_t& mode)
{
    if (name == "none")
        mode = kSplitNone;
    else if (name == "byte")
        mode = kSplitByByte;
    else if (name == "ctu")
        mode = kSplitByCtu;
    else
        return false;
    return true;
}

bool parseSplitOutput(const std::string& name, uint32_t& output)
{
    if (name == "none")
        output = 0;
    else if (name == "lowdelay")
        output = kSplitOutLowDelay;
    else if (name == "segment")
        output = kSplitOutSegment;
    else if (name == "lowdelay-segment")
        output = kSplitOutMask;
    else
        return false;
    return true;
}

int hexValue(char value)
{
    if (value >= '0' && value <= '9')
        return value - '0';
    if (value >= 'a' && value <= 'f')
        return value - 'a' + 10;
    if (value >= 'A' && value <= 'F')
        return value - 'A' + 10;
    return -1;
}

bool parseHexBytes(const std::string& text, std::vector<uint8_t>& bytes)
{
    if (text.size() % 2 != 0)
        return false;

    bytes.clear();
    bytes.reserve(text.size() / 2);
    for (size_t i = 0; i < text.size(); i += 2) {
        const int high = hexValue(text[i]);
        const int low = hexValue(text[i + 1]);
        if (high < 0 || low < 0)
            return false;
        bytes.push_back(static_cast<uint8_t>((high << 4) | low));
    }
    return true;
}

bool parseRoi(const std::string& text, RoiOptions& roi)
{
    std::stringstream stream(text);
    char comma = 0;
    int64_t x = 0;
    int64_t y = 0;
    int64_t width = 0;
    int64_t height = 0;
    if (!(stream >> x >> comma) || comma != ',')
        return false;
    if (!(stream >> y >> comma) || comma != ',')
        return false;
    if (!(stream >> width >> comma) || comma != ',')
        return false;
    if (!(stream >> height >> comma) || comma != ',')
        return false;
    if (!(stream >> roi.qp) || stream.peek() != std::char_traits<char>::eof())
        return false;
    if (x < 0 || y < 0 || width < 1 || height < 1
        || x > UINT16_MAX || y > UINT16_MAX || width > UINT16_MAX
        || height > UINT16_MAX || roi.qp < -51 || roi.qp > 51)
        return false;

    roi.x = static_cast<uint32_t>(x);
    roi.y = static_cast<uint32_t>(y);
    roi.width = static_cast<uint32_t>(width);
    roi.height = static_cast<uint32_t>(height);
    return true;
}

bool isH26x(media_codec_t codec)
{
    return codec == MEDIA_CODEC_VIDEO_H264 || codec == MEDIA_CODEC_VIDEO_H265;
}

bool fail(const std::string& message)
{
    ff_error("MPP encoder test failed: %s\n", message.c_str());
    return false;
}

bool expectSuccess(const char* action, int result)
{
    if (result == 0)
        return true;
    ff_error("MPP encoder test: %s returned %d\n", action, result);
    return false;
}

bool parseOptions(const CliOptions& cli, EncoderOptions& config)
{
    if (!parseCodec(cli.get("codec", "h265"), config.codec)
        || !parseRcMode(cli.get("rc", "cbr"), config.rc_mode)
        || !parseProfile(cli.get("profile", "high"), config.profile)
        || !parseBufferType(cli.get("buffer-type", "noncache"),
                            config.buffer_type)) {
        return fail("invalid codec, rate-control mode, profile, or buffer type");
    }

    config.width = cli.getLong("width", 1920);
    config.height = cli.getLong("height", 1080);
    config.format = v4l2GetFmtByName(cli.get("format", "nv12").c_str());
    config.hstride = cli.getLong("hstride", config.width);
    config.vstride = cli.getLong("vstride", config.height);
    config.fps = cli.getLong("fps", 30);
    config.gop = cli.getLong("gop", 60);
    config.bps = cli.getLong("bps", 2048);
    config.quality = cli.getDouble("quality", 0.8);
    config.duration_us = cli.getLong("duration-us", 0);
    config.output_timeout_ms = cli.getLong("output-timeout-ms", -1);
    config.cache_frames = cli.getLong("cache-frames", 0);
    config.buffers = cli.getLong("buffers", 8);
    config.parallel_buffers = cli.getLong("parallel-buffers", 0);
    config.max_frames = cli.getLong("frames", 300);
    config.duration_sec = cli.getDouble("duration", 0.0);
    config.wait_timeout_ms = cli.getLong("wait-timeout-ms", 2000);
    config.drain_timeout_ms = cli.getLong("drain-timeout-ms", 2000);
    config.report_every = cli.getLong("report-every", 100);
    config.animate = cli.getBool("animate");
    config.verbose = cli.getBool("verbose");
    config.external_consumer = cli.has("external-consumer");
    config.dump_pipe = cli.has("dump-pipe");
    config.full_api = cli.getBool("full-api");

    if (!config.width || !config.height || !config.format || config.hstride < config.width
        || config.vstride < config.height || config.fps <= 0 || config.gop < 0
        || config.bps < 0 || config.quality < 0.0f || config.quality > 1.0f
        || config.duration_us < 0 || config.output_timeout_ms < -1
        || config.output_timeout_ms > 3600000 || config.cache_frames < 0
        || config.cache_frames > 64 || config.buffers <= 0
        || config.parallel_buffers < 0 || config.duration_sec < 0.0
        || config.wait_timeout_ms < 0 || config.drain_timeout_ms < 0) {
        return fail("invalid dimensions or encoder options");
    }
    if (config.cache_frames > 1 && config.parallel_buffers <= config.cache_frames)
        return fail("cache-frames must be smaller than parallel-buffers when greater than 1");

    const bool codec_supports_h26x_controls = isH26x(config.codec);
    if (cli.has("intra-refresh")) {
        config.use_intra_refresh = cli.getBool("intra-refresh");
    } else {
        config.use_intra_refresh = config.full_api && codec_supports_h26x_controls;
    }
    config.refresh_mode = cli.getLong("refresh-mode", 0);
    config.refresh_num = cli.getLong("refresh-num", 10);
    if (config.refresh_mode < 0 || config.refresh_mode > 1 || config.refresh_num < 0
        || config.refresh_num > 8192)
        return fail("invalid intra-refresh options");

    if (cli.has("split-mode")) {
        if (!parseSplitMode(cli.get("split-mode"), config.split_mode))
            return fail("invalid split mode");
        config.use_split = true;
    } else if (config.full_api && codec_supports_h26x_controls) {
        config.use_split = true;
        config.split_mode = kSplitByCtu;
        config.split_arg = 16;
    }
    config.split_arg = cli.getLong("split-arg", config.split_arg);
    if (cli.has("split-out")
        && !parseSplitOutput(cli.get("split-out"), config.split_out))
        return fail("invalid split output mode");
    if (config.split_mode > kSplitByCtu || config.split_out > kSplitOutMask
        || (config.split_mode != kSplitNone && config.split_arg == 0))
        return fail("invalid split configuration");
    if (config.split_mode == kSplitNone) {
        config.split_arg = 0;
        config.split_out = 0;
    }

    if (cli.has("sei-text")) {
        const std::string text = cli.get("sei-text");
        config.sei_payload.assign(text.begin(), text.end());
        config.use_sei_payload = true;
    }
    if (cli.has("sei-hex")) {
        if (!parseHexBytes(cli.get("sei-hex"), config.sei_payload))
            return fail("invalid SEI payload hex");
        config.use_sei_payload = true;
    }
    if (cli.has("sei-uuid")) {
        if (!parseHexBytes(cli.get("sei-uuid"), config.sei_uuid)
            || config.sei_uuid.size() != 16)
            return fail("SEI UUID must contain exactly 16 bytes");
        if (!config.use_sei_payload)
            return fail("SEI UUID requires an SEI payload");
    }
    if (config.full_api && codec_supports_h26x_controls && !config.use_sei_payload) {
        const std::string payload = "ffmedia-module-mppenc-test";
        config.sei_payload.assign(payload.begin(), payload.end());
        config.sei_uuid = {
            0x00,
            0x11,
            0x22,
            0x33,
            0x44,
            0x55,
            0x66,
            0x77,
            0x88,
            0x99,
            0xaa,
            0xbb,
            0xcc,
            0xdd,
            0xee,
            0xff,
        };
        config.use_sei_payload = true;
    }
    if (config.use_sei_payload && !config.sei_uuid.empty()
        && config.sei_uuid.size() != 16)
        return fail("SEI UUID must contain exactly 16 bytes");
    if (cli.has("roi")) {
        if (!parseRoi(cli.get("roi"), config.roi))
            return fail("ROI must be x,y,width,height,qp");
        config.use_roi = true;
    } else if (config.full_api && codec_supports_h26x_controls) {
        config.use_roi = true;
        config.roi.x = config.width / 4;
        config.roi.y = config.height / 4;
        config.roi.width = std::max<uint32_t>(1, config.width / 2);
        config.roi.height = std::max<uint32_t>(1, config.height / 2);
        config.roi.qp = 24;
    }
    config.roi.relative = cli.getBool("roi-relative");
    config.roi.intra = cli.getBool("roi-intra");
    if (config.use_roi && (config.roi.x > UINT16_MAX || config.roi.y > UINT16_MAX || config.roi.width == 0 || config.roi.height == 0 || config.roi.width > UINT16_MAX || config.roi.height > UINT16_MAX || config.roi.qp < -51 || config.roi.qp > 51 || (!config.roi.relative && config.roi.qp < 0)))
        return fail("invalid ROI configuration");

    config.idr_frame = cli.has("idr-frame") ? cli.getLong("idr-frame", kNoFrame)
                                            : (config.full_api ? 1 : kNoFrame);
    config.reconfigure_frame = cli.has("reconfigure-frame")
                                   ? cli.getLong("reconfigure-frame", kNoFrame)
                                   : (config.full_api ? 2 : kNoFrame);
    if (config.idr_frame < kNoFrame || config.reconfigure_frame < kNoFrame)
        return fail("runtime frame numbers must be non-negative");

    config.intra_refresh_frame = config.full_api && codec_supports_h26x_controls ? 3 : kNoFrame;
    config.split_frame = config.full_api && codec_supports_h26x_controls ? 4 : kNoFrame;
    config.sei_payload_frame = config.full_api && codec_supports_h26x_controls ? 5 : kNoFrame;
    config.roi_frame = config.full_api && codec_supports_h26x_controls ? 6 : kNoFrame;

    config.reconfigure_fps = cli.getLong("reconfigure-fps", config.fps);
    config.reconfigure_gop = cli.getLong("reconfigure-gop", config.gop);
    config.reconfigure_bps = cli.getLong(
        "reconfigure-bps", config.bps < 199744 ? config.bps + 256 : config.bps - 256);
    config.reconfigure_quality = cli.getDouble("reconfigure-quality", config.quality);
    config.reconfigure_rc_mode = config.rc_mode;
    config.reconfigure_profile = config.profile;
    if (cli.has("reconfigure-rc")
        && !parseRcMode(cli.get("reconfigure-rc"), config.reconfigure_rc_mode))
        return fail("invalid reconfigure rate-control mode");
    if (cli.has("reconfigure-profile")
        && !parseProfile(cli.get("reconfigure-profile"), config.reconfigure_profile))
        return fail("invalid reconfigure profile");
    if (config.reconfigure_fps <= 0 || config.reconfigure_gop < 0
        || config.reconfigure_bps < 0 || config.reconfigure_bps > 200000
        || config.reconfigure_quality < 0.0f || config.reconfigure_quality > 1.0f)
        return fail("invalid runtime reconfigure options");

    return true;
}

ParameterObject encodeParameters(const EncoderOptions& config, int fps, int gop,
                                 int bps, EncodeRcMode rc_mode,
                                 float quality, EncodeProfile profile)
{
    return ParameterObject({
        {"codec", static_cast<int>(config.codec)},
        {"fps", fps},
        {"gop", gop},
        {"bitrate", bps},
        {"rc-mode", static_cast<int>(rc_mode)},
        {"quality", static_cast<double>(quality)},
        {"profile", static_cast<int>(profile)},
    });
}

ParameterObject intraRefreshParameters(const EncoderOptions& config, bool enabled)
{
    return ParameterObject({
        {"enabled", enabled},
        {"mode", config.refresh_mode},
        {"count", config.refresh_num},
    });
}

ParameterObject splitParameters(uint32_t mode, uint32_t arg, uint32_t output)
{
    return ParameterObject({
        {"mode", static_cast<int64_t>(mode)},
        {"arg", static_cast<int64_t>(arg)},
        {"out", static_cast<int64_t>(output)},
    });
}

ParameterObject roiParameters(const RoiOptions& roi, size_t index = 0)
{
    return ParameterObject({
        {"index", static_cast<int64_t>(index)},
        {"enabled", true},
        {"x", static_cast<int64_t>(roi.x)},
        {"y", static_cast<int64_t>(roi.y)},
        {"width", static_cast<int64_t>(roi.width)},
        {"height", static_cast<int64_t>(roi.height)},
        {"qp", roi.qp},
        {"relative", roi.relative},
        {"intra", roi.intra},
    });
}

bool checkParameterSchema(ModuleMppEnc& encoder, const ImagePara& input_para)
{
    const char* required_parameters[] = {
        "input",
        "encode",
        "encode/codec",
        "encode/fps",
        "encode/gop",
        "encode/bitrate",
        "encode/rc-mode",
        "encode/quality",
        "encode/profile",
        "duration-usec",
        "intra-refresh",
        "output-timeout-ms",
        "input-cache-pool-size",
        "roi",
        "split",
    };
    for (const char* path : required_parameters) {
        if (!encoder.hasParameter(path))
            return fail(std::string("missing parameter: ") + path);
    }
    if (encoder.hasParameter("sei-mode"))
        return fail("sei-mode parameter should not be exposed by ModuleMppEnc");

    ParameterInfo info;
    if (encoder.queryParameter("encode", info) < 0
        || info.type != ParameterType::OBJECT || !info.atomic
        || (info.flags & PARAMETER_FLAG_RUNTIME) == 0)
        return fail("encode parameter metadata is incomplete");
    if (encoder.queryParameters("encode").size() != 7)
        return fail("encode parameter member count is incorrect");

    ParameterObject input;
    if (encoder.getParameter("input", input) < 0)
        return fail("cannot read input parameter");
    int64_t width = 0;
    int64_t height = 0;
    if (input.getMember("width", width) < 0 || input.getMember("height", height) < 0
        || width != input_para.width || height != input_para.height)
        return fail("input parameter does not describe the connected image");

    int64_t fps = 0;
    if (encoder.getParameter("encode/fps", fps) < 0 || fps != 30)
        return fail("cannot read default encode/fps parameter");
    return true;
}

bool configureEncoder(ModuleMppEnc& encoder, const EncoderOptions& config)
{
    if (!expectSuccess(
            "changeEncodeParameter",
            encoder.changeEncodeParameter(config.codec, config.fps, config.gop,
                                          config.bps, config.rc_mode,
                                          config.quality, config.profile)))
        return false;

    // Exercise the atomic parameter binding as well as the direct API.
    if (!expectSuccess(
            "setParameter(encode)",
            encoder.setParameter("encode", encodeParameters(
                                               config, config.fps, config.gop,
                                               config.bps, config.rc_mode,
                                               config.quality, config.profile))))
        return false;

    encoder.setDuration(config.duration_us);
    encoder.setOutputTimeOut(config.output_timeout_ms);
    encoder.setInputCachePoolSize(config.cache_frames);
    if (!expectSuccess("setParameter(duration-usec)",
                       encoder.setParameter("duration-usec", config.duration_us))
        || !expectSuccess("setParameter(output-timeout-ms)",
                          encoder.setParameter("output-timeout-ms",
                                               config.output_timeout_ms))
        || !expectSuccess("setParameter(input-cache-pool-size)",
                          encoder.setParameter("input-cache-pool-size",
                                               config.cache_frames)))
        return false;

    if (!expectSuccess("setIntraRefresh",
                       encoder.setIntraRefresh(config.use_intra_refresh,
                                               config.refresh_mode,
                                               config.refresh_num))
        || !expectSuccess(
            "setParameter(intra-refresh)",
            encoder.setParameter("intra-refresh",
                                 intraRefreshParameters(config,
                                                        config.use_intra_refresh))))
        return false;

    const uint32_t split_mode = config.use_split ? config.split_mode : kSplitNone;
    const uint32_t split_arg = config.use_split ? config.split_arg : 0;
    const uint32_t split_out = config.use_split ? config.split_out : 0;
    if (!expectSuccess("setSplit", encoder.setSplit(split_mode, split_arg, split_out))
        || !expectSuccess("setParameter(split)",
                          encoder.setParameter("split",
                                               splitParameters(split_mode,
                                                               split_arg,
                                                               split_out)))
        || !expectSuccess("setSeiUserData",
                          encoder.setSeiUserData(config.use_sei_payload
                                                     ? config.sei_payload
                                                     : std::vector<uint8_t>(),
                                                 config.use_sei_payload
                                                     ? config.sei_uuid
                                                     : std::vector<uint8_t>())))
        return false;

    if (config.use_roi
        && !expectSuccess("setParameter(roi)",
                          encoder.setParameter("roi", roiParameters(config.roi))))
        return false;

    return true;
}

bool applyRuntimeChecks(ModuleMppEnc& encoder, const EncoderOptions& config,
                        uint64_t frame_index)
{
    if (static_cast<int>(frame_index) == config.idr_frame
        && !expectSuccess("runtime setIdrFrame", encoder.setIdrFrame()))
        return false;

    if (static_cast<int>(frame_index) == config.reconfigure_frame
        && !expectSuccess(
            "runtime changeEncodeParameter",
            encoder.changeEncodeParameter(
                config.codec, config.reconfigure_fps, config.reconfigure_gop,
                config.reconfigure_bps, config.reconfigure_rc_mode,
                config.reconfigure_quality, config.reconfigure_profile)))
        return false;

    if (static_cast<int>(frame_index) == config.intra_refresh_frame
        && !expectSuccess("runtime setIntraRefresh",
                          encoder.setIntraRefresh(false, config.refresh_mode,
                                                  config.refresh_num)))
        return false;

    if (static_cast<int>(frame_index) == config.split_frame
        && !expectSuccess("runtime setSplit", encoder.setSplit(kSplitNone, 0, 0)))
        return false;

    if (static_cast<int>(frame_index) == config.sei_payload_frame) {
        const std::vector<uint8_t> payload = {
            'r',
            'u',
            'n',
            't',
            'i',
            'm',
            'e',
            '-',
            's',
            'e',
            'i',
        };
        if (!expectSuccess("runtime setSeiUserData",
                           encoder.setSeiUserData(payload, config.sei_uuid)))
            return false;
    }

    if (static_cast<int>(frame_index) == config.roi_frame) {
        RoiOptions moved = config.roi;
        moved.x = 0;
        moved.y = 0;
        if (!expectSuccess("runtime setParameter(roi)",
                           encoder.setParameter("roi", roiParameters(moved, 1))))
            return false;
    }
    return true;
}

}  // namespace

int main(int argc, char** argv)
{
    CliOptions cli(argc, argv);
    if (cli.has("help") || cli.positional().empty()) {
        usage(argv[0]);
        return cli.has("help") ? 0 : 2;
    }

    EncoderOptions config;
    if (!parseOptions(cli, config))
        return 2;

    const ImagePara input_para(config.width, config.height, config.hstride,
                               config.vstride, config.format);
    std::unique_ptr<FILE, int (*)(FILE*)> output_file(
        std::fopen(cli.positional()[0].c_str(), "wb"), &std::fclose);
    if (!output_file) {
        ff_error("Failed to open output file: %s\n", cli.positional()[0].c_str());
        return 1;
    }

    auto input_buffer = std::make_shared<VideoBuffer>(config.buffer_type);
    input_buffer->allocBuffer(input_para);
    if (!input_buffer->getActiveData()) {
        ff_error("Failed to allocate input buffer\n");
        return 1;
    }
    input_buffer->fillWithColor(32, 128, 224);
    if (config.buffer_type == VideoBuffer::DRM_BUFFER_CACHEABLE
        || config.buffer_type == VideoBuffer::DRM_BUFFER_CACHEABLE_DMA32)
        input_buffer->flushDrmBuf();

    // Also cover the EncodeType constructor, which is used by the public API.
    ModuleMppEnc constructor_probe(ENCODE_TYPE_H265, input_para);
    if (constructor_probe.getInputImagePara().width != input_para.width
        || constructor_probe.getOutputImagePara().v4l2Fmt != V4L2_PIX_FMT_HEVC) {
        fail("EncodeType constructor did not initialize image parameters");
        return 1;
    }

    auto reader = std::make_shared<ModuleMemReader>(input_para);
    int ret = reader->init();
    if (ret < 0) {
        ff_error("Failed to init memory reader: %d\n", ret);
        return ret;
    }
    dumpOutputMediaChannels(*reader);

    std::shared_ptr<ModuleMedia> producer = reader;
    std::shared_ptr<ModuleRga> rga;
    if (config.parallel_buffers > 0) {
        rga = std::make_shared<ModuleRga>(input_para, RGA_ROTATE_NONE);
        ret = rga->connectProducer(reader);
        if (ret < 0) {
            ff_error("Failed to connect RGA: %d\n", ret);
            return ret;
        }
        rga->setBufferCount(config.parallel_buffers);
        ret = rga->init();
        if (ret < 0) {
            ff_error("Failed to init RGA: %d\n", ret);
            return ret;
        }
        dumpOutputMediaChannels(*rga);
        producer = rga;
    }

    auto encoder = std::make_shared<ModuleMppEnc>(
        config.codec, config.fps, config.gop, config.bps, config.rc_mode,
        config.quality, config.profile);
    ret = encoder->connectProducer(producer);
    if (ret < 0) {
        ff_error("Failed to connect encoder: %d\n", ret);
        return ret;
    }
    encoder->setBufferCount(config.buffers);
    if (!checkParameterSchema(*encoder, input_para)
        || !configureEncoder(*encoder, config))
        return 1;

    const uint64_t max_frames = config.max_frames;
    RunMonitor monitor(max_frames, config.duration_sec, config.report_every,
                       config.verbose);
    installSignalHandlers(monitor);
    std::atomic_bool write_failed(false);
    std::atomic<uint64_t> output_buffers(0);
    std::atomic<uint64_t> output_bytes(0);
    std::atomic_bool output_with_extra(false);

    reader->setMediaStatusChangeHooker(
        [&monitor](const std::string& name, MediaStatus status) {
            monitor.onStatus(name, status);
        });
    encoder->setMediaStatusChangeHooker(
        [&monitor](const std::string& name, MediaStatus status) {
            monitor.onStatus(name, status);
        });
    encoder->setMediaBufferProduceHooker(
        [&monitor, output = output_file.get(), &write_failed, &output_buffers,
         &output_bytes, &output_with_extra](const std::string& name,
                                            int queue_size,
                                            std::shared_ptr<MediaBuffer> buffer) {
            if (buffer) {
                const size_t size = buffer->getActiveSize();
                if (std::fwrite(buffer->getActiveData(), 1, size, output) != size) {
                    write_failed = true;
                    monitor.requestStop();
                } else {
                    ++output_buffers;
                    output_bytes += size;
                    if (buffer->getExtraData())
                        output_with_extra = true;
                }
            }
            monitor.onBuffer(name, queue_size, buffer);
        });

    ret = encoder->init();
    if (ret < 0) {
        ff_error("Failed to init MPP encoder: %d\n", ret);
        return ret;
    }
    dumpOutputMediaChannels(*encoder);
    dumpExtraBuffer("encoder", encoder->getExtraBuffer());

    std::shared_ptr<ModuleMedia> external_consumer;
    std::atomic<uint64_t> external_frames(0);
    if (config.external_consumer) {
        external_consumer = encoder->addExternalConsumer(
            "encoder-external", [&external_frames](const std::string&, int,
                                                   std::shared_ptr<MediaBuffer>) {
                ++external_frames;
            });
    }
    if (config.dump_pipe)
        reader->dumpPipe();

    monitor.reset();
    reader->start();
    uint64_t input_frames = 0;
    while (!monitor.stopped() && (!max_frames || input_frames < max_frames)) {
        if (!applyRuntimeChecks(*encoder, config, input_frames)) {
            ret = -EIO;
            break;
        }
        if (config.animate) {
            input_buffer->fillWithColor(input_frames & 0xff,
                                        (input_frames * 3) & 0xff,
                                        (input_frames * 7) & 0xff);
            if (config.buffer_type == VideoBuffer::DRM_BUFFER_CACHEABLE
                || config.buffer_type == VideoBuffer::DRM_BUFFER_CACHEABLE_DMA32)
                input_buffer->flushDrmBuf();
        }
        input_buffer->setPUstimestamp(
            config.fps > 0 ? input_frames * 1000000 / config.fps : 0);
        ret = reader->setInputBuffer(input_buffer);
        if (ret != 0) {
            ff_error("setInputBuffer failed: %d\n", ret);
            break;
        }
        ret = reader->waitProcess(config.wait_timeout_ms);
        if (ret != 0) {
            ff_error("waitProcess failed or timed out: %d\n", ret);
            break;
        }
        ++input_frames;
    }

    const auto drain_deadline = std::chrono::steady_clock::now()
                                + std::chrono::milliseconds(config.drain_timeout_ms);
    while (ret == 0 && monitor.frames() < input_frames
           && std::chrono::steady_clock::now() < drain_deadline
           && !monitor.abnormal()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    const bool incomplete_output = monitor.frames() != input_frames;
    if (incomplete_output)
        ff_warn("Encoder drain incomplete: input %lu, output %lu\n", input_frames,
                monitor.frames());

    reader->setProcessStatus(ModuleMemReader::PROCESS_STATUS_EXIT);
    encoder->stop();
    if (rga)
        rga->stop();
    reader->stop();
    if (config.dump_pipe)
        reader->dumpPipeSummary();

    monitor.printSummary("MPP encoder");
    ff_info("Input frames=%lu, output frames=%lu, output bytes=%lu, extra %d\n",
            input_frames, monitor.frames(), output_bytes.load(),
            output_with_extra.load());
    if (external_consumer)
        ff_info("External consumer received %lu buffers\n", external_frames.load());
    return (ret != 0 || write_failed || monitor.abnormal() || incomplete_output
            || output_buffers.load() != monitor.frames())
               ? 1
               : 0;
}
