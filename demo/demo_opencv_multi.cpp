#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "module/vi/module_rtspClient.hpp"
#include "module/vp/module_mppdec.hpp"
#include "module/vp/module_rga.hpp"

#define ENABLE_OPENCV

//=============================================
#ifdef ENABLE_OPENCV
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#endif
//=============================================

#define UNUSED(x) [&x] {}()
using namespace std;

struct External_ctx {
    shared_ptr<ModuleMedia> module;
    uint16_t test;
};

void callback_external(void* _ctx, shared_ptr<MediaBuffer> buffer)
{
    External_ctx* ctx = static_cast<External_ctx*>(_ctx);
    shared_ptr<ModuleMedia> module = ctx->module;

    if (buffer == NULL || buffer->getMediaBufferType() != BUFFER_TYPE_VIDEO)
        return;
    shared_ptr<VideoBuffer> buf = static_pointer_cast<VideoBuffer>(buffer);

    void* ptr = buf->getActiveData();
    size_t size = buf->getActiveSize();
    uint32_t width = buf->getImagePara().hstride;
    uint32_t height = buf->getImagePara().vstride;
    // flush dma buf to cpu
    buf->invalidateDrmBuf();

    UNUSED(size);
    //=================================================
#ifdef ENABLE_OPENCV
    cv::Mat mat(cv::Size(width, height), CV_8UC3, ptr);
    cv::imshow(module->getName(), mat);
    cv::waitKey(1);
#endif
    //=================================================
}

int main(int argc, char** argv)
{
    int ret;
    shared_ptr<ModuleRtspClient> rtsp_c = NULL;
    shared_ptr<ModuleMppDec> dec = NULL;
    shared_ptr<ModuleRga> rga = NULL;
    ImagePara input_para;
    ImagePara output_para;
    External_ctx* ctx1 = NULL;
    External_ctx* ctx2 = NULL;

    rtsp_c = make_shared<ModuleRtspClient>("rtsp://admin:firefly123@168.168.2.96:554/av_stream");
    ret = rtsp_c->init();
    if (ret < 0) {
        ff_error("rtsp client init failed\n");
        goto FAILED;
    }

    input_para = rtsp_c->getOutputImagePara();
    dec = make_shared<ModuleMppDec>(input_para);
    dec->setProductor(rtsp_c);
    ret = dec->init();
    if (ret < 0) {
        ff_error("Dec init failed\n");
        goto FAILED;
    }

    input_para = dec->getOutputImagePara();
    output_para = input_para;
    output_para.width = input_para.width / 2;
    output_para.height = input_para.height / 2;
    output_para.hstride = output_para.width;
    output_para.vstride = output_para.height;
    output_para.v4l2Fmt = V4L2_PIX_FMT_BGR24;
    rga = make_shared<ModuleRga>(input_para, output_para, RGA_ROTATE_NONE);
    rga->setProductor(dec);
    ret = rga->init();
    if (ret < 0) {
        ff_error("rga init failed\n");
        goto FAILED;
    }

    ctx1 = new External_ctx();
    ctx1->module = rga->addExternalConsumer("external_test1", ctx1, callback_external);

    ctx2 = new External_ctx();
    ctx2->module = rga->addExternalConsumer("external_test2", ctx2, callback_external);

    rtsp_c->start();

    getchar();

    rtsp_c->stop();

FAILED:
    if (ctx1)
        delete ctx1;
    if (ctx2)
        delete ctx2;
}
