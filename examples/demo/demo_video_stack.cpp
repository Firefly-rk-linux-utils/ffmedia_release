#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <math.h>

#include "module/vp/module_videoStack.hpp"
#include "module/vi/module_rtspClient.hpp"
#include "module/vp/module_mppdec.hpp"

#define TEST_PUSH_STREAM
#ifdef TEST_PUSH_STREAM
#include "module/vp/module_mppenc.hpp"
#include "module/vo/module_rtspServer.hpp"
#endif

#define TEST_DISPLAY
#ifdef TEST_DISPLAY
#include "module/vo/module_drmDisplay.hpp"
#endif

using namespace std;
using namespace FFMedia;

#define STACK_WIDTH  1920
#define STACK_HEIGHT 1080
#define STACK_FPS    30

const char* rtspPushPath = "/live/1";
const int rtspPushPort = 8554;

const EncodeType encodeType = ENCODE_TYPE_H264;
const RTSP_STREAM_TYPE rtspTransport = RTSP_STREAM_TYPE_UDP;
const char* rtspUrl[] = {
    "rtsp://172.16.10.201:8554/test/0",
    "rtsp://172.16.10.201:8554/test/0",
    "rtsp://172.16.10.201:8554/test/0",
    "rtsp://172.16.10.201:8554/test/0",
    //    "rtsp://admin:firefly123@172.16.2.1:554/av_stream",
    //    "rtsp://admin:firefly123@172.16.2.2:554/av_stream",
    //    "rtsp://admin:firefly123@172.16.2.3:554/av_stream",
    //    "rtsp://admin:firefly123@172.16.2.4:554/av_stream",
};


struct InputLinkContext {
    shared_ptr<ModuleMedia> source;
    ImageCrop stackCrop;
    shared_ptr<ModuleVideoStack> videoStack;
};

shared_ptr<ModuleVideoStack> videoStackModuleLinkCreate(int width, int height, int fps)
{
    int ret;
    auto videoStack = std::make_shared<ModuleVideoStack>("video stack", width, height, fps);
    ret = videoStack->init();
    if (ret < 0) {
        ff_error("Failed to init video stack\n");
        return nullptr;
    }

#ifdef TEST_DISPLAY
    auto display = make_shared<ModuleDrmDisplay>(videoStack->getOutputImagePara());
    display->setPlanePara(V4L2_PIX_FMT_NV12);
    display->setProductor(videoStack);
    display->setSynchronize(std::make_shared<Synchronize>(SYNCHRONIZETYPE_VIDEO));
    ret = display->init();
    if (ret < 0) {
        ff_error("Failed to init display\n");
        return nullptr;
    }
#endif

#ifdef TEST_PUSH_STREAM
    auto enc = make_shared<ModuleMppEnc>(encodeType, videoStack->getOutputImagePara());
    enc->setProductor(videoStack);
    enc->setInputCachePoolSize(0);
    ret = enc->init();
    if (ret < 0) {
        ff_error("Failed to init mppenc\n");
        return nullptr;
    }

    auto rtspS = make_shared<ModuleRtspServer>(rtspPushPath, rtspPushPort);
    rtspS->setProductor(enc);
    ret = rtspS->init();
    if (ret < 0) {
        ff_error("Failed to init rtsp server\n");
        return nullptr;
    }
    ff_info("\n Start push stream: rtsp://LocalIpAddr:%d%s\n\n", rtspPushPort, rtspPushPath);
#endif
    return videoStack;
}

std::vector<std::shared_ptr<InputLinkContext>> inputModuleLinkCreate(std::shared_ptr<ModuleVideoStack> videoStack, const char* rtspUrl[], int urlCount)
{
    std::vector<std::shared_ptr<InputLinkContext>> inputs;
    int ret;

    int rows, cols;
    int s = sqrt(urlCount);
    if ((s * s) < urlCount) {
        if ((s * (s + 1)) < urlCount)
            cols = s + 1;
        else
            cols = s;
        rows = s + 1;
    } else {
        rows = cols = s;
    }

    auto outputImagePara = videoStack->getOutputImagePara();
    int gridWidth = outputImagePara.width / cols;
    int gridHeight = outputImagePara.height / rows;

    for (int row = 0; row < rows; row++) {
        for (int col = 0; col < cols; col++) {
            int index = row * cols + col;
            if (index >= urlCount) {
                break;
            }

            auto rtspC = make_shared<ModuleRtspClient>(rtspUrl[index], rtspTransport);
            ret = rtspC->init();
            if (ret < 0) {
                ff_error("Failed to init rtsp client, index: %d\n", index);
                continue;
            }

            auto dec = make_shared<ModuleMppDec>(rtspC->getOutputImagePara());
            dec->setProductor(rtspC);
            ret = dec->init();
            if (ret < 0) {
                ff_error("Failed to init dex, index: %d\n", index);
                continue;
            }

            auto inputLCtx = std::make_shared<InputLinkContext>();
            inputLCtx->source = rtspC;
            inputLCtx->videoStack = videoStack;
            inputLCtx->stackCrop.x = gridWidth * col;
            inputLCtx->stackCrop.y = gridHeight * row;
            inputLCtx->stackCrop.h = gridHeight;
            inputLCtx->stackCrop.w = gridWidth;

            const MediaChannelId inputId = inputs.size();
            videoStack->setModuleStackParams(inputId, inputLCtx->stackCrop);
            ret = videoStack->connectProducer(dec);
            if (ret < 0) {
                ff_error("Failed to connect video stack input, index: %d, ret: %d\n",
                         index, ret);
                continue;
            }

            ff_info("input link (%d) stack corp: x %d, y %d, w %d, h %d\n\n", index, inputLCtx->stackCrop.x,
                    inputLCtx->stackCrop.y, inputLCtx->stackCrop.w, inputLCtx->stackCrop.h);

            inputs.push_back(inputLCtx);
        }
    }

    return inputs;
}


int main(int argc, char** argv)
{
    auto videoStack = videoStackModuleLinkCreate(STACK_WIDTH, STACK_HEIGHT, STACK_FPS);
    if (!videoStack) {
        ff_error("Failed to create video stack module link\n");
        return 1;
    }

    auto inputLinks = inputModuleLinkCreate(videoStack, rtspUrl, sizeof(rtspUrl) / sizeof(rtspUrl[0]));
    if (inputLinks.empty()) {
        ff_error("Failed to create input module links\n");
        return -1;
    }

    videoStack->start();
    for (auto& it : inputLinks) {
        it->source->start();
    }

    getchar();

    for (auto& it : inputLinks) {
        it->source->stop();
    }
    videoStack->stop();
    return 0;
}
