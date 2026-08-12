#include "module/vi/module_cam.hpp"
#include "module/ff_media_parameter_helpers.hpp"
#include "tests/module_test_utils.hpp"

#include <linux/videodev2.h>
#include <memory>

using namespace FFMedia;
using namespace FFMediaTest;

namespace
{
void usage(const char* program)
{
    ff_info("Usage: %s DEVICE [options]\n"
            "  --format nv12       Requested V4L2 pixel format\n"
            "  --width 1920 --height 1080 [--hstride N --vstride N]\n"
            "  --buffers 4         Camera buffer count\n"
            "  --timeout-ms 5000   Capture timeout\n"
            "  --frame-rate 30     Requested camera frame rate\n"
            "  --use-parameters    Configure the camera through MediaParameter\n"
            "  --frames 0          Stop after N buffers (0 disables)\n"
            "  --duration 10       Stop after N seconds (0 disables)\n"
            "  --change-source DEV Exercise changeSource() before init\n"
            "  --query-capabilities Exercise camIoctlOperation(VIDIOC_QUERYCAP)\n"
            "  --external-consumer Add a second output consumer\n"
            "  --verbose --report-every 100 --dump-pipe\n",
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

    const uint32_t width = options.getLong("width", 0);
    const uint32_t height = options.getLong("height", 0);
    const std::string format_name = options.get("format");
    const uint32_t format = format_name.empty() ? 0 : v4l2GetFmtByName(format_name.c_str());
    if (!format_name.empty() && !format) {
        ff_error("Unknown pixel format: %s\n", format_name.c_str());
        return 2;
    }

    const bool use_parameters = options.getBool("use-parameters");
    auto camera = std::make_shared<ModuleCam>(
        use_parameters ? std::string() : options.positional()[0]);
    if (use_parameters) {
        int ret = camera->setParameter("device", options.positional()[0]);
        if (ret < 0) {
            ff_error("Setting camera device parameter failed: %d\n", ret);
            return ret;
        }
    }
    if (options.has("change-source")) {
        const int ret = camera->changeSource(options.get("change-source"));
        if (ret < 0) {
            ff_error("changeSource failed: %d\n", ret);
            return ret;
        }
    }
    if (width || height || format) {
        const ImagePara capture(
            width, height, options.getLong("hstride", width),
            options.getLong("vstride", height), format);
        if (use_parameters) {
            const int ret = camera->setParameter(
                "capture", imageParaToParameterObject(capture));
            if (ret < 0) {
                ff_error("Setting camera capture parameter failed: %d\n", ret);
                return ret;
            }
        } else {
            camera->setOutputImagePara(capture);
        }
    }
    if (use_parameters) {
        const int ret = camera->setParameter(
            "buffer-count", options.getLong("buffers", 4));
        if (ret < 0)
            return ret;
    } else {
        camera->setBufferCount(options.getLong("buffers", 4));
    }
    const long timeout_ms = options.getLong("timeout-ms", 5000);
    if (use_parameters) {
        int ret = camera->setParameter("timeout-ms", timeout_ms);
        if (ret < 0)
            return ret;
        const double frame_rate = options.getDouble("frame-rate", 0.0);
        if (frame_rate > 0.0) {
            ret = camera->setParameter("frame-rate", frame_rate);
            if (ret < 0)
                return ret;
        }
    } else {
        camera->setTimeOutSec(timeout_ms / 1000,
                              (timeout_ms % 1000) * 1000);
    }

    RunMonitor monitor(options.getLong("frames", 0),
                       options.getDouble("duration", 10.0),
                       options.getLong("report-every", 100),
                       options.getBool("verbose"));
    installSignalHandlers(monitor);
    camera->setMediaBufferProduceHooker(
        [&monitor](const std::string& name, int queue_size,
                   std::shared_ptr<MediaBuffer> buffer) {
            monitor.onBuffer(name, queue_size, buffer);
        });
    camera->setMediaStatusChangeHooker(
        [&monitor](const std::string& name, MediaStatus status) {
            monitor.onStatus(name, status);
        });

    int ret = camera->init();
    if (ret < 0) {
        ff_error("Failed to init camera: %d\n", ret);
        return ret;
    }

    if (options.has("query-capabilities")) {
        v4l2_capability capability = {};
        ret = camera->camIoctlOperation(VIDIOC_QUERYCAP, &capability);
        if (ret < 0) {
            ff_error("VIDIOC_QUERYCAP failed: %d\n", ret);
            return ret;
        }
        ff_info("V4L2 driver=%s card=%s bus=%s capabilities=0x%x\n",
                capability.driver, capability.card, capability.bus_info,
                capability.capabilities);
    }

    dumpOutputMediaChannels(*camera);
    std::shared_ptr<ModuleMedia> external_consumer;
    std::atomic<uint64_t> external_frames(0);
    if (options.has("external-consumer")) {
        external_consumer = camera->addExternalConsumer(
            "camera-external", [&external_frames](const std::string&, int,
                                                  std::shared_ptr<MediaBuffer>) {
                ++external_frames;
            });
    }
    if (options.has("dump-pipe"))
        camera->dumpPipe();

    monitor.reset();
    camera->start();
    monitor.wait();
    camera->stop();
    if (options.has("dump-pipe"))
        camera->dumpPipeSummary();
    monitor.printSummary("camera");
    if (external_consumer)
        ff_info("External consumer received %lu buffers\n", external_frames.load());
    return monitor.abnormal() ? 1 : 0;
}
