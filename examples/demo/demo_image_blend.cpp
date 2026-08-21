#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "module/vp/module_imageProcessor.hpp"
#include "module/vi/module_fileReader.hpp"
#include "module/vp/module_mppdec.hpp"

#include "base/pixel_fmt.hpp"
#include "base/video_buffer.hpp"

#define TEST_DISPLAY
#ifdef TEST_DISPLAY
#include "module/vo/module_drmDisplay.hpp"
#endif

using namespace std;
using namespace FFMedia;

#define BLEND_WIDTH  1920
#define BLEND_HEIGHT 1080

#define PIP_WIDTH  480
#define PIP_HEIGHT 270
#define PIP_MARGIN 100


// Main video occupies input channel 0; the three video blend layers use
// channels 1..3 and the custom image layer uses channel 4.
constexpr MediaChannelId kVideoBlendIds[] = {1, 2, 3};
constexpr MediaChannelId kCustomImageId = 4;


// Configure one blend layer of the image processor.
int configureBlendLayer(shared_ptr<ModuleImageProcessor> processor,
                        MediaChannelId inputId, int zOrder,
                        uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                        int alphaMode, double opacity = 1.0)
{
    ParameterObject sourceCrop({
        {"x", static_cast<int64_t>(0)},
        {"y", static_cast<int64_t>(0)},
        {"width", static_cast<int64_t>(0)},
        {"height", static_cast<int64_t>(0)},
    });
    ParameterObject destRect({
        {"x", static_cast<int64_t>(x)},
        {"y", static_cast<int64_t>(y)},
        {"width", static_cast<int64_t>(w)},
        {"height", static_cast<int64_t>(h)},
    });
    ParameterObject value({
        {"input-id", static_cast<int64_t>(inputId)},
        {"enabled", true},
        {"z-order", static_cast<int64_t>(zOrder)},
        {"source-crop", sourceCrop},
        {"destination-rect", destRect},
        {"opacity", opacity},
        {"rotation", 0},
        {"mirror", false},
        {"flip", false},
        {"alpha-mode", alphaMode},
    });
    return processor->setParameter("blend", value);
}

// Generate a semi-transparent RGBA32 custom image (diagonal gradient).
shared_ptr<VideoBuffer> createCustomImage(uint32_t width, uint32_t height)
{
    ImagePara para(width, height, ALIGN(width, 64), ALIGN(height, 16),
                   V4L2_PIX_FMT_RGB24);
    auto buffer = make_shared<VideoBuffer>(VideoBuffer::DRM_BUFFER_CACHEABLE);
    buffer->allocBuffer(para);
    if (buffer->getBufFd() <= 0 || buffer->getSize() == 0
        || !buffer->getActiveData()) {
        return nullptr;
    }

    ff_info("Custom image: size(%d x %d), stride(%d x %d), format(%s), size %d\n",
            width, height, para.hstride, para.vstride,
            v4l2GetFmtName(para.v4l2Fmt), buffer->getSize());

    auto* data = static_cast<uint8_t*>(buffer->getActiveData());
    memset(data, 0, buffer->getSize());
    const uint32_t xRange = width > 1 ? width - 1 : 1;
    const uint32_t yRange = height > 1 ? height - 1 : 1;
    for (uint32_t y = 0; y < height; y++) {
        uint8_t* row = data + static_cast<size_t>(y) * para.hstride * 3;
        for (uint32_t x = 0; x < width; x++) {
            row[x * 3 + 0] = static_cast<uint8_t>(220 * x / xRange);
            row[x * 3 + 1] = static_cast<uint8_t>(80 + 120 * y / yRange);
            row[x * 3 + 2] = static_cast<uint8_t>(255 - 200 * x / xRange);
        }
    }
    buffer->flushDrmBuf();
    return buffer;
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        printf("Usage: %s <video-file>\n", argv[0]);
        return -1;
    }
    const char* videoPath = argv[1];

    // 1. Read the video file and fan it out to four decoders: main + three
    //    blend streams, all playing the same file.
    auto fileReader = make_shared<ModuleFileReader>(videoPath, true);
    fileReader->setBufferCount(20);
    int ret = fileReader->init();
    if (ret < 0) {
        ff_error("Failed to init file reader: %s\n", videoPath);
        return ret;
    }

    shared_ptr<ModuleMppDec> decoders[4];
    for (int i = 0; i < 4; i++) {
        auto dec = make_shared<ModuleMppDec>();
        ret = dec->connectProducer(fileReader);
        if (ret < 0) {
            ff_error("Failed to connect decoder %d, ret: %d\n", i, ret);
            return ret;
        }
        dec->setBufferCount(10);
        ret = dec->init();
        if (ret < 0) {
            ff_error("Failed to init decoder %d, ret: %d\n", i, ret);
            return ret;
        }
        decoders[i] = dec;
    }

    // 2. Create the image processor with the output canvas.
    auto processor = make_shared<ModuleImageProcessor>();
    processor->setParameter("output", {{"width", static_cast<int64_t>(BLEND_WIDTH)},
                                       {"height", static_cast<int64_t>(BLEND_HEIGHT)},
                                       {"format", static_cast<int64_t>(V4L2_PIX_FMT_RGB24)}});

    // 3. Configure the three video blend layers.
    ret = configureBlendLayer(processor, kVideoBlendIds[0], 1,
                              PIP_MARGIN, PIP_MARGIN, PIP_WIDTH, PIP_HEIGHT,
                              0, 1.0);
    if (ret == 0)
        ret = configureBlendLayer(processor, kVideoBlendIds[1], 2,
                                  BLEND_WIDTH - PIP_WIDTH - PIP_MARGIN,
                                  PIP_MARGIN, PIP_WIDTH, PIP_HEIGHT, 1, 0.7);
    if (ret == 0)
        ret = configureBlendLayer(processor, kVideoBlendIds[2], 3,
                                  PIP_MARGIN,
                                  BLEND_HEIGHT - PIP_HEIGHT - PIP_MARGIN,
                                  PIP_WIDTH, PIP_HEIGHT, 2, 0.6);
    if (ret < 0) {
        ff_error("Configure image blend failed, ret: %d\n", ret);
        return ret;
    }

    // 4. Connect producers. The first raw-video producer becomes the main
    //    input (channel 0), the following three become video blend layers
    //    (channels 1..3).
    ret = processor->connectProducer(decoders[0]);
    if (ret < 0) {
        ff_error("Failed to connect main video, ret: %d\n", ret);
        return ret;
    }
    for (int i = 1; i < 4; i++) {
        ret = processor->connectProducer(decoders[i]);
        if (ret < 0) {
            ff_error("Failed to connect blend video %d, ret: %d\n", i - 1, ret);
            return ret;
        }
    }

    ret = processor->init();
    if (ret < 0) {
        ff_error("Failed to init image processor, ret: %d\n", ret);
        return -1;
    }

#ifdef TEST_DISPLAY
    auto display = make_shared<ModuleDrmDisplay>();
    display->setProductor(processor);
    display->setPlaneDisplayMode(DrmDisplayPlane::SINGLE_WINDOW_DISPLAY);
    // display->setSynchronize(std::make_shared<Synchronize>(SYNCHRONIZETYPE_VIDEO));
    ret = display->init();
    if (ret < 0) {
        ff_error("Failed to init display\n");
        return -1;
    }
#endif

    {
        // Use a Synchronize object to pace the main video stream. The three blend
        // streams are not synchronized and will be dropped if they are late.
        auto sync = std::make_shared<Synchronize>(SYNCHRONIZETYPE_VIDEO);
        decoders[0]->setMediaBufferProduceHooker([sync](const std::string&, int, const std::shared_ptr<MediaBuffer>& buffer) {
            auto diff = sync->updateVideo(buffer->getPUstimestamp(), 0);
            if (diff > 0) {
                std::this_thread::sleep_for(std::chrono::microseconds(diff));
            }
        });
    }

    // 5. Start producers then submit the custom image frame.
    fileReader->start();

    {
        // custom image source becomes channel 4
        ret = configureBlendLayer(processor, kCustomImageId, 4,
                                  BLEND_WIDTH - PIP_WIDTH - PIP_MARGIN,
                                  BLEND_HEIGHT - PIP_HEIGHT - PIP_MARGIN,
                                  PIP_WIDTH, PIP_HEIGHT, 0, 0.5);
        if (ret == 0) {
            shared_ptr<VideoBuffer> customImage = createCustomImage(PIP_WIDTH, PIP_HEIGHT);
            if (customImage) {
                processor->receiveMediaBuffer(MediaBufferContext{customImage, kCustomImageId});
            }
        } else
            ff_error("Configure custom image layer failed, ret: %d\n", ret);
    }

    getchar();

    fileReader->stop();
    return 0;
}
