#include "module/vi/module_ffmpegDemux.hpp"
#include "tests/module_test_utils.hpp"

#include <memory>
#include <sstream>

using namespace FFMedia;
using namespace FFMediaTest;

namespace
{
void usage(const char* program)
{
    ff_info("Usage: %s INPUT [options]\n"
            "  --loop 1                FFmpeg loop count (-1 forever)\n"
            "  --input-format NAME     Force AVInputFormat\n"
            "  --format-options K=V,K=V (default: probesize=200K)\n"
            "  --use-parameters        Configure through MediaParameter\n"
            "  --timeout-us 5000000 --seek TS --seek-flags N\n"
            "  --runtime-seek TS       Seek from the first produce callback\n"
            "  --buffers 20 --frames 0 --duration 0\n"
            "  --change-source INPUT   Exercise changeSource() before init\n"
            "  --external-consumer --verbose --report-every 100 --dump-pipe\n",
            program);
}

int setFormatOptions(ModuleFFmpegDemux& demuxer, const std::string& values)
{
    std::stringstream stream(values);
    std::string item;
    while (std::getline(stream, item, ',')) {
        const std::string::size_type equal = item.find('=');
        if (equal == std::string::npos || equal == 0) {
            ff_error("Invalid format option: %s\n", item.c_str());
            return -1;
        }
        const std::string key = item.substr(0, equal);
        const int ret = demuxer.setFormatOption(key, item.substr(equal + 1), 0);
        if (ret < 0)
            return ret;
        ff_info("Format option %s=%s\n", key.c_str(),
                demuxer.getFormatOption(key, 0).c_str());
    }
    return 0;
}
}  // namespace

int main(int argc, char** argv)
{
    CliOptions options(argc, argv);
    if (options.has("help") || options.positional().empty()) {
        usage(argv[0]);
        return options.has("help") ? 0 : 2;
    }

    const int loop = options.getLong("loop", 1);
    const bool use_parameters = options.getBool("use-parameters");
    auto demuxer = std::make_shared<ModuleFFmpegDemux>(
        use_parameters ? std::string() : options.positional()[0],
        use_parameters ? 1 : loop);
    if (use_parameters) {
        int ret = demuxer->setParameter("source/uri", options.positional()[0]);
        if (ret == 0)
            ret = demuxer->setParameter("source/loop", loop);
        if (ret < 0) {
            ff_error("Setting FFmpeg source parameters failed: %d\n", ret);
            return ret;
        }
    }
    if (options.has("change-source")) {
        const int ret = demuxer->changeSource(options.get("change-source"), loop);
        if (ret < 0) {
            ff_error("changeSource failed: %d\n", ret);
            return ret;
        }
    }
    if (options.has("input-format")) {
        const int ret = use_parameters
                            ? demuxer->setParameter(
                                "input-format", options.get("input-format"))
                            : demuxer->setInputFormat(
                                options.get("input-format"));
        if (ret < 0) {
            ff_error("setInputFormat failed: %d\n", ret);
            return ret;
        }
    }
    const std::string format_options = options.get("format-options", "probesize=200K");
    int ret = use_parameters
                  ? demuxer->setParameter("ffmpeg/options", format_options)
                  : setFormatOptions(*demuxer, format_options);
    if (ret < 0)
        return ret;
    if (use_parameters) {
        ret = demuxer->setParameter(
            "timeout-usec", options.getLong("timeout-us", 5000000));
        if (ret == 0)
            ret = demuxer->setParameter(
                "buffer-count", options.getLong("buffers", 20));
        if (ret < 0)
            return ret;
    } else {
        demuxer->setTimeOut(options.getLong("timeout-us", 5000000));
        demuxer->setBufferCount(options.getLong("buffers", 20));
    }

    RunMonitor monitor(options.getLong("frames", 0),
                       options.getDouble("duration", 0.0),
                       options.getLong("report-every", 100),
                       options.getBool("verbose"));
    installSignalHandlers(monitor);
    std::atomic<bool> runtime_seek_attempted(false);
    std::atomic<int> runtime_seek_result(0);
    ModuleFFmpegDemux* demuxer_ptr = demuxer.get();
    demuxer->setMediaBufferProduceHooker(
        [&monitor, &options, use_parameters, demuxer_ptr,
         &runtime_seek_attempted, &runtime_seek_result](
            const std::string& name, int queue_size,
            std::shared_ptr<MediaBuffer> buffer) {
            if (options.has("runtime-seek")
                && !runtime_seek_attempted.exchange(true)) {
                int ret = 0;
                if (use_parameters) {
                    ret = demuxer_ptr->setParameter(
                        "seek-flags", options.getLong("seek-flags", 0));
                    if (ret == 0) {
                        ret = demuxer_ptr->setParameter(
                            "seek", options.getLong("runtime-seek", 0));
                    }
                } else {
                    ret = demuxer_ptr->setFileSeek(
                        options.getLong("runtime-seek", 0),
                        options.getLong("seek-flags", 0));
                }
                runtime_seek_result.store(ret);
                if (ret < 0)
                    monitor.requestStop();
            }
            monitor.onBuffer(name, queue_size, buffer);
        });
    demuxer->setMediaStatusChangeHooker(
        [&monitor](const std::string& name, MediaStatus status) {
            monitor.onStatus(name, status);
        });

    ret = demuxer->init();
    if (ret < 0) {
        ff_error("Failed to init FFmpeg demuxer: %d\n", ret);
        return ret;
    }
    if (use_parameters) {
        int64_t seek_max = 0;
        bool seekable = false;
        ret = demuxer->getParameter("seek-max", seek_max);
        if (ret == 0)
            ret = demuxer->getParameter("seekable", seekable);
        if (ret < 0) {
            ff_error("Querying FFmpeg seek parameters failed: %d\n", ret);
            return ret;
        }
        ff_info("FFmpeg seek max=%ld us, seekable=%d\n",
                seek_max, seekable);
    }
    if (options.has("seek")) {
        if (use_parameters) {
            ret = demuxer->setParameter(
                "seek-flags", options.getLong("seek-flags", 0));
            if (ret == 0)
                ret = demuxer->setParameter(
                    "seek", options.getLong("seek", 0));
        } else {
            ret = demuxer->setFileSeek(options.getLong("seek", 0),
                                       options.getLong("seek-flags", 0));
        }
        if (ret < 0) {
            ff_error("FFmpeg seek failed: %d\n", ret);
            return ret;
        }
    }

    dumpOutputMediaChannels(*demuxer);
    ff_info("Video codec=%d, audio codec=%d\n",
            static_cast<int>(demuxer->getVideoCodec()),
            static_cast<int>(demuxer->getAudioCodec()));
    dumpExtraBuffer("video", demuxer->getExtraBuffer(BUFFER_TYPE_VIDEO));
    dumpExtraBuffer("audio", demuxer->getExtraBuffer(BUFFER_TYPE_AUDIO));

    std::shared_ptr<ModuleMedia> external_consumer;
    std::atomic<uint64_t> external_frames(0);
    if (options.has("external-consumer")) {
        external_consumer = demuxer->addExternalConsumer(
            "ffmpeg-demux-external", [&external_frames](const std::string&, int,
                                                        std::shared_ptr<MediaBuffer>) {
                ++external_frames;
            });
    }
    if (options.has("dump-pipe"))
        demuxer->dumpPipe();

    monitor.reset();
    demuxer->start();
    monitor.wait();
    demuxer->stop();
    if (options.has("dump-pipe"))
        demuxer->dumpPipeSummary();
    monitor.printSummary("FFmpeg demuxer");
    if (external_consumer)
        ff_info("External consumer received %lu buffers\n", external_frames.load());
    if (options.has("runtime-seek")
        && (!runtime_seek_attempted.load()
            || runtime_seek_result.load() < 0)) {
        return 1;
    }
    return monitor.abnormal() ? 1 : 0;
}
