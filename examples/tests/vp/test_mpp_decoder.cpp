#include "module/vi/module_fileReader.hpp"
#include "module/vp/module_mppdec.hpp"
#include "tests/module_test_utils.hpp"

#include <cstdio>
#include <memory>

using namespace FFMedia;
using namespace FFMediaTest;

namespace
{
void usage(const char* program)
{
    ff_info("Usage: %s INPUT [options]\n"
            "  --loop --seek MS --change-source FILE\n"
            "  --split 0 --fast 1 --deinterlace 1\n"
            "  --output-timeout-ms 0 --buffers 20\n"
            "  --buffer-type noncache|cache|malloc|dma32|cache-dma32\n"
            "  --output-format nv12 --output decoded.yuv\n"
            "  --frames 0 --duration 0 --report-every 100 --verbose\n"
            "  --external-consumer --dump-pipe\n",
            program);
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

DecodeType decodeTypeFromFormat(uint32_t format)
{
    switch (format) {
        case V4L2_PIX_FMT_H264:
            return DECODE_TYPE_H264;
        case V4L2_PIX_FMT_HEVC:
            return DECODE_TYPE_H265;
        case V4L2_PIX_FMT_MJPEG:
            return DECODE_TYPE_MJPEG;
        case V4L2_PIX_FMT_VP8:
            return DECODE_TYPE_VP8;
        case V4L2_PIX_FMT_VP9:
            return DECODE_TYPE_VP9;
        case V4L2_PIX_FMT_MPEG1:
            return DECODE_TYPE_MPEG1;
        case V4L2_PIX_FMT_MPEG2:
            return DECODE_TYPE_MPEG2;
        case V4L2_PIX_FMT_MPEG4:
            return DECODE_TYPE_MPEG4;
        default:
            return DECODE_TYPE_MAX;
    }
}
}  // namespace

int main(int argc, char** argv)
{
    CliOptions options(argc, argv);
    if (options.has("help") || options.positional().empty()) {
        usage(argv[0]);
        return options.has("help") ? 0 : 2;
    }

    const bool loop = options.getBool("loop");
    auto reader = std::make_shared<ModuleFileReader>(options.positional()[0], loop);
    if (options.has("change-source")) {
        const int ret = reader->changeSource(options.get("change-source"), loop);
        if (ret < 0) {
            ff_error("changeSource failed: %d\n", ret);
            return ret;
        }
    }
    int ret = reader->init();
    if (ret < 0) {
        ff_error("Failed to init file reader: %d\n", ret);
        return ret;
    }
    if (options.has("seek")) {
        ret = reader->setFileReaderSeek(options.getLong("seek", 0));
        if (ret < 0) {
            ff_error("setFileReaderSeek failed: %d\n", ret);
            return ret;
        }
    }
    dumpOutputMediaChannels(*reader);

    ImagePara input = reader->getOutputImagePara();
    const DecodeType decode_type = decodeTypeFromFormat(input.v4l2Fmt);
    if (decode_type == DECODE_TYPE_MAX) {
        ff_error("Unsupported compressed input format: %s\n",
                 v4l2GetFmtName(input.v4l2Fmt));
        return 2;
    }
    auto decoder = std::make_shared<ModuleMppDec>(input, decode_type);
    ret = decoder->connectProducer(reader);
    if (ret < 0) {
        ff_error("Failed to connect file reader to decoder: %d\n", ret);
        return ret;
    }
    decoder->setNeedSplit(options.getLong("split", 0));
    decoder->setFastMode(options.getLong("fast", 1));
    decoder->setDeinterlace(options.getLong("deinterlace", 1));
    decoder->setOutputTimeOut(options.getLong("output-timeout-ms", 0));
    decoder->setBufferCount(options.getLong("buffers", 20));
    VideoBuffer::BUFFER_TYPE buffer_type = VideoBuffer::DRM_BUFFER_NONCACHEABLE;
    if (!parseBufferType(options.get("buffer-type", "noncache"), buffer_type)) {
        ff_error("Invalid decoder buffer type\n");
        return 2;
    }
    decoder->setBufferType(buffer_type);
    if (options.has("output-format")) {
        const uint32_t format = v4l2GetFmtByName(options.get("output-format").c_str());
        if (!format) {
            ff_error("Unknown output format\n");
            return 2;
        }
        ImagePara output = input;
        output.v4l2Fmt = format;
        decoder->setOutputImagePara(output);
    }

    FILE* output_file = nullptr;
    if (options.has("output")) {
        output_file = std::fopen(options.get("output").c_str(), "wb");
        if (!output_file) {
            ff_error("Failed to open output file: %s\n", options.get("output").c_str());
            return 1;
        }
    }

    RunMonitor monitor(options.getLong("frames", 0),
                       options.getDouble("duration", 0.0),
                       options.getLong("report-every", 100),
                       options.getBool("verbose"));
    installSignalHandlers(monitor);
    reader->setMediaStatusChangeHooker(
        [&monitor](const std::string& name, MediaStatus status) {
            monitor.onStatus(name, status);
        });
    decoder->setMediaStatusChangeHooker(
        [&monitor](const std::string& name, MediaStatus status) {
            monitor.onStatus(name, status);
        });
    decoder->setMediaBufferProduceHooker(
        [&monitor, output_file](const std::string& name, int queue_size,
                                std::shared_ptr<MediaBuffer> buffer) {
            if (output_file && buffer)
                std::fwrite(buffer->getActiveData(), 1, buffer->getActiveSize(), output_file);
            monitor.onBuffer(name, queue_size, buffer);
        });

    ret = decoder->init();
    if (ret < 0) {
        if (output_file)
            std::fclose(output_file);
        ff_error("Failed to init MPP decoder: %d\n", ret);
        return ret;
    }
    dumpOutputMediaChannels(*decoder);

    std::shared_ptr<ModuleMedia> external_consumer;
    std::atomic<uint64_t> external_frames(0);
    if (options.has("external-consumer")) {
        external_consumer = decoder->addExternalConsumer(
            "decoder-external", [&external_frames](const std::string&, int,
                                                   std::shared_ptr<MediaBuffer>) {
                ++external_frames;
            });
    }
    if (options.has("dump-pipe"))
        reader->dumpPipe();

    monitor.reset();
    reader->start();
    monitor.wait();
    reader->stop();
    if (output_file)
        std::fclose(output_file);
    if (options.has("dump-pipe"))
        reader->dumpPipeSummary();
    monitor.printSummary("MPP decoder");
    if (external_consumer)
        ff_info("External consumer received %lu buffers\n", external_frames.load());
    return monitor.abnormal() ? 1 : 0;
}
