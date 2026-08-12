#include "module/vi/module_rtspClient.hpp"
#include "tests/module_test_utils.hpp"

#include <memory>

using namespace FFMedia;
using namespace FFMediaTest;

namespace
{
void usage(const char* program)
{
    ff_info("Usage: %s RTSP_URL [options]\n"
            "  --transport udp|tcp|multicast\n"
            "  --audio                Enable audio reception\n"
            "  --no-video             Disable video reception\n"
            "  --timeout-ms 5000 --buffers 10\n"
            "  --frames 0 --duration 10\n"
            "  --change-source URL    Exercise changeSource() before init\n"
            "  --external-consumer --verbose --report-every 100 --dump-pipe\n",
            program);
}

bool parseTransport(const std::string& name, RTSP_STREAM_TYPE& type)
{
    if (name == "udp")
        type = RTSP_STREAM_TYPE_UDP;
    else if (name == "tcp")
        type = RTSP_STREAM_TYPE_TCP;
    else if (name == "multicast")
        type = RTSP_STREAM_TYPE_MULTICAST;
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

    RTSP_STREAM_TYPE transport;
    if (!parseTransport(options.get("transport", "udp"), transport)) {
        ff_error("Invalid transport; use udp, tcp, or multicast\n");
        return 2;
    }
    auto client = std::make_shared<ModuleRtspClient>(
        options.positional()[0], transport, !options.has("no-video"),
        options.getBool("audio"));
    if (options.has("change-source")) {
        const int ret = client->changeSource(options.get("change-source"), transport);
        if (ret < 0) {
            ff_error("changeSource failed: %d\n", ret);
            return ret;
        }
    }
    client->setBufferCount(options.getLong("buffers", 10));
    const long timeout_ms = options.getLong("timeout-ms", 5000);
    client->setTimeOutSec(timeout_ms / 1000, (timeout_ms % 1000) * 1000);

    RunMonitor monitor(options.getLong("frames", 0),
                       options.getDouble("duration", 10.0),
                       options.getLong("report-every", 100),
                       options.getBool("verbose"));
    installSignalHandlers(monitor);
    client->setMediaBufferProduceHooker(
        [&monitor](const std::string& name, int queue_size,
                   std::shared_ptr<MediaBuffer> buffer) {
            monitor.onBuffer(name, queue_size, buffer);
        });
    client->setMediaStatusChangeHooker(
        [&monitor](const std::string& name, MediaStatus status) {
            monitor.onStatus(name, status);
        });

    int ret = client->init();
    if (ret < 0) {
        ff_error("Failed to init RTSP client: %d\n", ret);
        return ret;
    }

    dumpOutputMediaChannels(*client);
    const SampleInfo sample = client->getAudioSampleInfo();
    ff_info("Session=%d, video codec=%d fps=%u, audio codec=%d channels=%d rate=%d\n",
            static_cast<int>(client->getSessionStatus()),
            static_cast<int>(client->getVideoCodec()), client->videoFPS(),
            static_cast<int>(client->getAudioCodec()), sample.channels,
            sample.sample_rate);
    dumpExtraBuffer("video", client->getExtraBuffer(BUFFER_TYPE_VIDEO));
    dumpExtraBuffer("audio", client->getExtraBuffer(BUFFER_TYPE_AUDIO));

    std::shared_ptr<ModuleMedia> external_consumer;
    std::atomic<uint64_t> external_frames(0);
    if (options.has("external-consumer")) {
        external_consumer = client->addExternalConsumer(
            "rtsp-external", [&external_frames](const std::string&, int,
                                                std::shared_ptr<MediaBuffer>) {
                ++external_frames;
            });
    }
    if (options.has("dump-pipe"))
        client->dumpPipe();

    monitor.reset();
    client->start();
    monitor.wait();
    client->stop();
    if (options.has("dump-pipe"))
        client->dumpPipeSummary();
    monitor.printSummary("RTSP client");
    if (external_consumer)
        ff_info("External consumer received %lu buffers\n", external_frames.load());
    return monitor.abnormal() ? 1 : 0;
}
