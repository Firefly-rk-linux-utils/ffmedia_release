#include "module/vi/module_memReader.hpp"
#include "module/vp/module_mppenc.hpp"
#include "module/vp/module_rga.hpp"
#include "tests/module_test_utils.hpp"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <memory>

using namespace FFMedia;
using namespace FFMediaTest;

namespace
{
void usage(const char* program)
{
    ff_info("Usage: %s OUTPUT [options]\n"
            "  --codec h264|h265|mjpeg --width 1920 --height 1080\n"
            "  --format nv12 [--hstride N --vstride N]\n"
            "  --fps 30 --gop 60 --bps 2048 --rc cbr|vbr|fixqp|avbr\n"
            "  --quality 0.8 --profile baseline|main|high\n"
            "  --duration-us N --output-timeout-ms -1 --cache-frames 0\n"
            "  --intra-refresh --refresh-mode 0 --refresh-num 10\n"
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
}  // namespace

int main(int argc, char** argv)
{
    CliOptions options(argc, argv);
    if (options.has("help") || options.positional().empty()) {
        usage(argv[0]);
        return options.has("help") ? 0 : 2;
    }

    media_codec_t codec = MEDIA_CODEC_VIDEO_H265;
    EncodeRcMode rc_mode = ENCODE_RC_MODE_CBR;
    EncodeProfile profile = ENCODE_PROFILE_HIGH;
    VideoBuffer::BUFFER_TYPE buffer_type = VideoBuffer::DRM_BUFFER_NONCACHEABLE;
    if (!parseCodec(options.get("codec", "h265"), codec) || !parseRcMode(options.get("rc", "cbr"), rc_mode) || !parseProfile(options.get("profile", "high"), profile) || !parseBufferType(options.get("buffer-type", "noncache"), buffer_type)) {
        ff_error("Invalid codec, rate-control mode, profile, or buffer type\n");
        return 2;
    }

    const uint32_t width = options.getLong("width", 1920);
    const uint32_t height = options.getLong("height", 1080);
    const uint32_t format = v4l2GetFmtByName(options.get("format", "nv12").c_str());
    if (!width || !height || !format) {
        ff_error("Invalid input dimensions or format\n");
        return 2;
    }
    const ImagePara input_para(width, height,
                               options.getLong("hstride", width),
                               options.getLong("vstride", height), format);

    FILE* output_file = std::fopen(options.positional()[0].c_str(), "wb");
    if (!output_file) {
        ff_error("Failed to open output file: %s\n", options.positional()[0].c_str());
        return 1;
    }

    auto input_buffer = std::make_shared<VideoBuffer>(buffer_type);
    input_buffer->allocBuffer(input_para);
    if (!input_buffer->getActiveData()) {
        ff_error("Failed to allocate input buffer\n");
        std::fclose(output_file);
        return 1;
    }
    input_buffer->fillWithColor(32, 128, 224);
    if (buffer_type == VideoBuffer::DRM_BUFFER_CACHEABLE || buffer_type == VideoBuffer::DRM_BUFFER_CACHEABLE_DMA32)
        input_buffer->flushDrmBuf();

    auto reader = std::make_shared<ModuleMemReader>(input_para);
    int ret = reader->init();
    if (ret < 0) {
        ff_error("Failed to init memory reader: %d\n", ret);
        std::fclose(output_file);
        return ret;
    }
    dumpOutputMediaChannels(*reader);

    std::shared_ptr<ModuleMedia> producer = reader;
    std::shared_ptr<ModuleRga> rga;
    const int parallel_buffers = options.getLong("parallel-buffers", 0);
    const int cache_frames = options.getLong("cache-frames", 0);
    if (cache_frames > 1 && parallel_buffers <= cache_frames) {
        ff_error("cache-frames must be smaller than parallel-buffers when greater than 1\n");
        std::fclose(output_file);
        return 2;
    }
    if (parallel_buffers > 0) {
        rga = std::make_shared<ModuleRga>(input_para, RGA_ROTATE_NONE);
        ret = rga->connectProducer(reader);
        if (ret < 0) {
            ff_error("Failed to connect RGA: %d\n", ret);
            std::fclose(output_file);
            return ret;
        }
        rga->setBufferCount(parallel_buffers);
        ret = rga->init();
        if (ret < 0) {
            ff_error("Failed to init RGA: %d\n", ret);
            std::fclose(output_file);
            return ret;
        }
        dumpOutputMediaChannels(*rga);
        producer = rga;
    }

    const int fps = options.getLong("fps", 30);
    const int gop = options.getLong("gop", 60);
    const int bps = options.getLong("bps", 2048);
    const float quality = options.getDouble("quality", 0.8);
    auto encoder = std::make_shared<ModuleMppEnc>(codec, fps, gop, bps,
                                                  rc_mode, quality, profile);
    ret = encoder->connectProducer(producer);
    if (ret < 0) {
        ff_error("Failed to connect encoder: %d\n", ret);
        std::fclose(output_file);
        return ret;
    }
    encoder->setBufferCount(options.getLong("buffers", 8));
    encoder->setOutputTimeOut(options.getLong("output-timeout-ms", -1));
    encoder->setInputCachePoolSize(cache_frames);
    if (options.has("duration-us"))
        encoder->setDuration(options.getLong("duration-us", 0));
    if (options.has("intra-refresh")) {
        encoder->setIntraRefresh(true, options.getLong("refresh-mode", 0),
                                 options.getLong("refresh-num", 10));
    }

    const uint64_t max_frames = options.getLong("frames", 300);
    RunMonitor monitor(max_frames, options.getDouble("duration", 0.0),
                       options.getLong("report-every", 100),
                       options.getBool("verbose"));
    installSignalHandlers(monitor);
    std::atomic_bool write_failed(false);
    reader->setMediaStatusChangeHooker(
        [&monitor](const std::string& name, MediaStatus status) {
            monitor.onStatus(name, status);
        });
    encoder->setMediaStatusChangeHooker(
        [&monitor](const std::string& name, MediaStatus status) {
            monitor.onStatus(name, status);
        });
    encoder->setMediaBufferProduceHooker(
        [&monitor, output_file, &write_failed](const std::string& name,
                                               int queue_size,
                                               std::shared_ptr<MediaBuffer> buffer) {
            if (buffer && std::fwrite(buffer->getActiveData(), 1, buffer->getActiveSize(), output_file) != buffer->getActiveSize()) {
                write_failed = true;
                monitor.requestStop();
            }
            monitor.onBuffer(name, queue_size, buffer);
        });

    ret = encoder->init();
    if (ret < 0) {
        ff_error("Failed to init MPP encoder: %d\n", ret);
        std::fclose(output_file);
        return ret;
    }
    dumpOutputMediaChannels(*encoder);
    dumpExtraBuffer("encoder", encoder->getExtraBuffer());

    std::shared_ptr<ModuleMedia> external_consumer;
    std::atomic<uint64_t> external_frames(0);
    if (options.has("external-consumer")) {
        external_consumer = encoder->addExternalConsumer(
            "encoder-external", [&external_frames](const std::string&, int,
                                                   std::shared_ptr<MediaBuffer>) {
                ++external_frames;
            });
    }
    if (options.has("dump-pipe"))
        reader->dumpPipe();

    monitor.reset();
    reader->start();
    const int wait_timeout = options.getLong("wait-timeout-ms", 2000);
    uint64_t input_frames = 0;
    while (!monitor.stopped() && (!max_frames || input_frames < max_frames)) {
        if (options.getBool("animate")) {
            input_buffer->fillWithColor(input_frames & 0xff,
                                        (input_frames * 3) & 0xff,
                                        (input_frames * 7) & 0xff);
            if (buffer_type == VideoBuffer::DRM_BUFFER_CACHEABLE || buffer_type == VideoBuffer::DRM_BUFFER_CACHEABLE_DMA32)
                input_buffer->flushDrmBuf();
        }
        input_buffer->setPUstimestamp(fps > 0 ? input_frames * 1000000 / fps : 0);
        ret = reader->setInputBuffer(input_buffer);
        if (ret != 0) {
            ff_error("setInputBuffer failed: %d\n", ret);
            break;
        }
        ret = reader->waitProcess(wait_timeout);
        if (ret != 0) {
            ff_error("waitProcess failed or timed out: %d\n", ret);
            break;
        }
        ++input_frames;
    }

    const auto drain_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(options.getLong("drain-timeout-ms", 2000));
    while (ret == 0 && monitor.frames() < input_frames && std::chrono::steady_clock::now() < drain_deadline && !monitor.abnormal()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    const bool incomplete_output = monitor.frames() != input_frames;
    if (incomplete_output)
        ff_warn("Encoder drain incomplete: input %lu, output %lu\n",
                input_frames, monitor.frames());

    reader->setProcessStatus(ModuleMemReader::PROCESS_STATUS_EXIT);
    encoder->stop();
    if (rga)
        rga->stop();
    reader->stop();
    std::fclose(output_file);
    if (options.has("dump-pipe"))
        reader->dumpPipeSummary();
    monitor.printSummary("MPP encoder");
    ff_info("Input frames=%lu, output frames=%lu\n", input_frames,
            monitor.frames());
    if (external_consumer)
        ff_info("External consumer received %lu buffers\n", external_frames.load());
    return (ret != 0 || write_failed || monitor.abnormal() || incomplete_output) ? 1 : 0;
}
