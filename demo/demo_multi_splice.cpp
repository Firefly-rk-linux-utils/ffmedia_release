#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <math.h>

#include "module/vi/module_memReader.hpp"
#include "module/vi/module_rtspClient.hpp"
#include "module/vp/module_rga.hpp"
#include "module/vp/module_mppdec.hpp"
#include "module/vp/module_mppenc.hpp"
#include "module/vo/module_rtspServer.hpp"

#define DISPLAY
#ifdef DISPLAY
#include "module/vo/module_drmDisplay.hpp"
#endif

#define GALIGN(x, a) (((x) + (a)-1) & ~((a)-1))

#define WIDTH  1920
#define HEIGHT 1080
const char* rtspPath = "/live/1";
const int rtspPort = 8554;

const EncodeType encodeType = ENCODE_TYPE_H264;
const RTSP_STREAM_TYPE rtspTransport = RTSP_STREAM_TYPE_UDP;
const char* rtspUrl[] = {
    "rtsp://admin:firefly123@172.16.2.1:554/av_stream",
    "rtsp://admin:firefly123@172.16.2.2:554/av_stream",
    "rtsp://admin:firefly123@172.16.2.3:554/av_stream",
    "rtsp://admin:firefly123@172.16.2.4:554/av_stream",
};

struct MemReaderContext {
    shared_ptr<ModuleMemReader> source;
    shared_ptr<VideoBuffer> buffer;
    std::atomic_uint32_t bufferStatus;
    uint32_t bufferCompleteStatus;
    std::shared_timed_mutex mtx;
};

struct InputLinkContext {
    shared_ptr<ModuleMedia> source;
    shared_ptr<ModuleRga> rga;
    ImageCrop outputCrop;
    MemReaderContext* memRCtx;
    uint16_t index;
};

void inputHanderCallback(void* _ctx, shared_ptr<MediaBuffer> buffer)
{
    InputLinkContext* ctx = static_cast<InputLinkContext*>(_ctx);
    MemReaderContext* memRCtx = ctx->memRCtx;
    {
        std::shared_lock<std::shared_timed_mutex> lck(memRCtx->mtx);

        shared_ptr<MediaBuffer> output_buffer = memRCtx->buffer;
        // copy buffer to memRCtx->buffer
        ctx->rga->doConsume(buffer, output_buffer);
        memRCtx->bufferStatus |= 1 << ctx->index;
    }

    {
        std::lock_guard<std::shared_timed_mutex> lck(memRCtx->mtx);
        if (memRCtx->bufferStatus == memRCtx->bufferCompleteStatus) {
            memRCtx->bufferStatus = 0;
            memRCtx->source->setInputBuffer(memRCtx->buffer->getData(), memRCtx->buffer->getSize(),
                                            memRCtx->buffer->getBufFd());
            if (memRCtx->source->waitProcess(2000) < 0) {
                ff_info("Failed to wait mem_r process\n");
            }
        }
    }
}

MemReaderContext* memReaderModuleLinkCreate()
{
    int ret;
    MemReaderContext* memRCtx = nullptr;

    do {
        auto inputPara = ImagePara(WIDTH, HEIGHT, ALIGN(WIDTH, 8), ALIGN(HEIGHT, 8), V4L2_PIX_FMT_NV12);
        auto buffer = make_shared<VideoBuffer>(VideoBuffer::DRM_BUFFER_CACHEABLE);
        buffer->allocBuffer(inputPara);
        if (buffer->getSize() <= 0) {
            ff_error("Failed to alloc buf\n");
            break;
        }

        auto memR = make_shared<ModuleMemReader>(inputPara);
        ret = memR->init();
        if (ret < 0) {
            ff_error("Failed to init memreader\n");
            break;
        }

#ifdef DISPLAY
        auto display = make_shared<ModuleDrmDisplay>();
        display->setPlanePara(V4L2_PIX_FMT_NV12);
        display->setProductor(memR);
        ret = display->init();
        if (ret < 0) {
            ff_error("Failed to init display\n");
            break;
        }
#endif

        // copy buffer
        auto rga = make_shared<ModuleRga>(inputPara, RGA_ROTATE_NONE);
        rga->setProductor(memR);
        rga->setBufferCount(2);
        ret = rga->init();
        if (ret < 0) {
            ff_error("Failed to init rga\n");
            break;
        }

        auto enc = make_shared<ModuleMppEnc>(encodeType);
        enc->setProductor(rga);
        enc->setBufferCount(8);
        ret = enc->init();
        if (ret < 0) {
            ff_error("Failed to init mppenc\n");
            break;
        }

        auto rtspS = make_shared<ModuleRtspServer>(rtspPath, rtspPort);
        rtspS->setProductor(enc);
        ret = rtspS->init();
        if (ret < 0) {
            ff_error("Failed to init rtsp server\n");
            break;
        }
        ff_info("\n Start push stream: rtsp://LocalIpAddr:%d%s\n\n", rtspPort, rtspPath);

        memRCtx = new MemReaderContext();
        memRCtx->buffer = buffer;
        memRCtx->bufferStatus = 0;
        memRCtx->bufferCompleteStatus = 0;
        memRCtx->source = memR;

    } while (0);

    return memRCtx;
}

std::vector<InputLinkContext*> inputModuleLinkCreate(MemReaderContext* memRCtx, const char* rtspUrl[], int urlCount)
{
    std::vector<InputLinkContext*> inputs;
    int count = 0;
    int ret;

    auto outputImagePara = memRCtx->buffer->getImagePara();
    int hc, vc;
    int s = sqrt(urlCount);
    if ((s * s) < urlCount) {
        if ((s * (s + 1)) < urlCount)
            vc = s + 1;
        else
            vc = s;
        hc = s + 1;
    } else {
        hc = vc = s;
    }

    while (count < urlCount) {
        int index = count++;
        auto rtspC = make_shared<ModuleRtspClient>(rtspUrl[index], rtspTransport);
        rtspC->setBufferCount(20);
        ret = rtspC->init();
        if (ret < 0) {
            ff_error("Failed to init rtsp client, index: %d\n", index);
            continue;
        }

        auto dec = make_shared<ModuleMppDec>();
        dec->setProductor(rtspC);
        dec->setBufferCount(10);
        ret = dec->init();
        if (ret < 0) {
            ff_error("Failed to init dex, index: %d\n", index);
            continue;
        }

        auto inputLCtx = new InputLinkContext();
        inputLCtx->index = index;
        inputLCtx->source = rtspC;
        // Use rga alone to copy the decoded data to the same memory without adding it to the module link.
        inputLCtx->rga = make_shared<ModuleRga>();
        inputLCtx->memRCtx = memRCtx;
        memRCtx->bufferCompleteStatus |= 1 << index;
        inputs.push_back(inputLCtx);

        dec->setOutputDataCallback(inputLCtx, inputHanderCallback);
        auto inputImagePara = dec->getOutputImagePara();

        int ho = inputLCtx->index % hc;
        int vo = inputLCtx->index / hc;
        uint32_t dw = outputImagePara.width / hc;
        uint32_t dh = outputImagePara.height / vc;
        inputLCtx->outputCrop.w = std::min(dw, inputImagePara.width);
        inputLCtx->outputCrop.h = std::min(dh, inputImagePara.height);
        inputLCtx->outputCrop.x = (dw - inputLCtx->outputCrop.w) / 2 + ho * dw;
        inputLCtx->outputCrop.y = (dh - inputLCtx->outputCrop.h) / 2 + vo * dh;

        ff_info("input link (%d) output corp: x %d, y %d, w %d, h %d\n\n", index, inputLCtx->outputCrop.x,
                inputLCtx->outputCrop.y, inputLCtx->outputCrop.w, inputLCtx->outputCrop.h);

        inputLCtx->rga->setSrcPara(inputImagePara.v4l2Fmt, 0, 0, inputImagePara.width, inputImagePara.height,
                                   inputImagePara.hstride, inputImagePara.vstride);
        inputLCtx->rga->setDstPara(outputImagePara.v4l2Fmt, inputLCtx->outputCrop.x,
                                   inputLCtx->outputCrop.y, inputLCtx->outputCrop.w, inputLCtx->outputCrop.h,
                                   outputImagePara.hstride, outputImagePara.vstride);
    }

    return inputs;
}


int main(int argc, char** argv)
{
    MemReaderContext* memRCtx = memReaderModuleLinkCreate();
    if (memRCtx == nullptr) {
        ff_error("Failed to create memory read module link\n");
        return 1;
    }

    auto inputLinks = inputModuleLinkCreate(memRCtx, rtspUrl, sizeof(rtspUrl) / sizeof(rtspUrl[0]));
    if (inputLinks.size() == 0) {
        ff_error("Failed to create input module links\n");
        delete memRCtx;
        return -1;
    }

    memRCtx->source->start();

    for (auto it : inputLinks) {
        it->source->start();
    }

    getchar();

    for (auto it : inputLinks) {
        it->source->stop();
        delete it;
        it = nullptr;
    }

    memRCtx->source->setProcessStatus(ModuleMemReader::PROCESS_STATUS_EXIT);
    memRCtx->source->stop();
    delete memRCtx;
    return 0;
}
