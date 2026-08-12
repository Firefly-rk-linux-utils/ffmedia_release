#include "module/vi/module_fileReader.hpp"
#include "tests/module_test_utils.hpp"

#include <memory>

using namespace FFMedia;
using namespace FFMediaTest;

namespace
{
void usage(const char* program)
{
    ff_info("Usage: %s FILE [options]\n"
            "  --loop                  Loop at end of input\n"
            "  --seek MS               Seek before start\n"
            "  --runtime-seek MS       Seek from the first produce callback\n"
            "  --use-parameters        Configure source and seek through MediaParameter\n"
            "  --buffers 4             Output buffer count\n"
            "  --frames 0 --duration 0 Stop limits (0 disables)\n"
            "  --change-source FILE    Exercise changeSource() before init\n"
            "  --external-consumer --verbose --report-every 100 --dump-pipe\n",
            program);
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
    const bool use_parameters = options.getBool("use-parameters");
    auto reader = std::make_shared<ModuleFileReader>(
        use_parameters ? std::string() : options.positional()[0],
        use_parameters ? false : loop);
    if (use_parameters) {
        int ret = reader->setParameter("source/path", options.positional()[0]);
        if (ret == 0)
            ret = reader->setParameter("source/loop", loop);
        if (ret < 0) {
            ff_error("Setting file reader source parameters failed: %d\n", ret);
            return ret;
        }
    }
    if (options.has("change-source")) {
        const int ret = reader->changeSource(options.get("change-source"), loop);
        if (ret < 0) {
            ff_error("changeSource failed: %d\n", ret);
            return ret;
        }
    }
    if (use_parameters) {
        const int ret = reader->setParameter(
            "buffer-count", options.getLong("buffers", 4));
        if (ret < 0)
            return ret;
    } else {
        reader->setBufferCount(options.getLong("buffers", 4));
    }

    RunMonitor monitor(options.getLong("frames", 0),
                       options.getDouble("duration", 0.0),
                       options.getLong("report-every", 100),
                       options.getBool("verbose"));
    installSignalHandlers(monitor);
    std::atomic<bool> runtime_seek_attempted(false);
    std::atomic<int> runtime_seek_result(0);
    ModuleFileReader* reader_ptr = reader.get();
    reader->setMediaBufferProduceHooker(
        [&monitor, &options, use_parameters, reader_ptr,
         &runtime_seek_attempted, &runtime_seek_result](
            const std::string& name, int queue_size,
            std::shared_ptr<MediaBuffer> buffer) {
            if (options.has("runtime-seek")
                && !runtime_seek_attempted.exchange(true)) {
                const int ret = use_parameters
                                    ? reader_ptr->setParameter(
                                        "seek", options.getLong(
                                                    "runtime-seek", 0))
                                    : reader_ptr->setFileReaderSeek(
                                        options.getLong("runtime-seek", 0));
                runtime_seek_result.store(ret);
                if (ret < 0)
                    monitor.requestStop();
            }
            monitor.onBuffer(name, queue_size, buffer);
        });
    reader->setMediaStatusChangeHooker(
        [&monitor](const std::string& name, MediaStatus status) {
            monitor.onStatus(name, status);
        });

    int ret = reader->init();
    if (ret < 0) {
        ff_error("Failed to init file reader: %d\n", ret);
        return ret;
    }
    if (options.has("seek")) {
        ret = use_parameters
                  ? reader->setParameter("seek", options.getLong("seek", 0))
                  : reader->setFileReaderSeek(options.getLong("seek", 0));
        if (ret < 0) {
            ff_error("File reader seek failed: %d\n", ret);
            return ret;
        }
    }

    dumpOutputMediaChannels(*reader);
    int64_t max_seek = reader->getFileReaderMaxSeek();
    if (use_parameters) {
        ret = reader->getParameter("seek-max", max_seek);
        if (ret < 0)
            return ret;
    }
    ff_info("Video codec=%d, audio codec=%d, max seek=%ld\n",
            static_cast<int>(reader->getVideoCodec()),
            static_cast<int>(reader->getAudioCodec()),
            max_seek);
    dumpExtraBuffer("video", reader->getExtraBuffer(BUFFER_TYPE_VIDEO));
    dumpExtraBuffer("audio", reader->getExtraBuffer(BUFFER_TYPE_AUDIO));

    std::shared_ptr<ModuleMedia> external_consumer;
    std::atomic<uint64_t> external_frames(0);
    if (options.has("external-consumer")) {
        external_consumer = reader->addExternalConsumer(
            "file-reader-external", [&external_frames](const std::string&, int,
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
    if (options.has("dump-pipe"))
        reader->dumpPipeSummary();
    monitor.printSummary("file reader");
    if (external_consumer)
        ff_info("External consumer received %lu buffers\n", external_frames.load());
    if (options.has("runtime-seek")
        && (!runtime_seek_attempted.load()
            || runtime_seek_result.load() < 0)) {
        return 1;
    }
    return monitor.abnormal() ? 1 : 0;
}
