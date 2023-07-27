#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "module/vi/module_rtspClient.hpp"
#include "module/vp/module_mppdec.hpp"
#include "module/vp/module_rga.hpp"
#include "module/vo/module_drmDisplay.hpp"

#include <opencv2/opencv.hpp>

struct External_ctx {
    shared_ptr<ModuleMedia> module;
    void *buf;
    uint32_t width;
    uint32_t height;
    size_t buf_size;
};

void callback_blend(void* _ctx, shared_ptr<MediaBuffer> buffer)
{
    External_ctx* ctx = static_cast<External_ctx*>(_ctx);
    shared_ptr<ModuleRga> rga = static_pointer_cast<ModuleRga>(ctx->module);

    if (ctx->buf == NULL) {
        ctx->width = rga->getOutputImagePara().hstride;
        ctx->height = rga->getOutputImagePara().vstride;
        ctx->buf_size = ctx->width * ctx->height * 4;
        ctx->buf = new char[ctx->buf_size];
        memset(ctx->buf, 0, ctx->buf_size);
        //cv::Scalar() BRGA => V4L2_PIX_FMT_ARGB32
        rga->setPatPara(V4L2_PIX_FMT_ARGB32, 0, 0, rga->getOutputImagePara().width, rga->getOutputImagePara().height, ctx->width, ctx->height);
    }

    cv::Mat image(cv::Size(ctx->width, ctx->height), CV_8UC4, ctx->buf);
    std::string timeText = "Current timestamp: " + std::to_string(buffer->getPUstimestamp() / 1000) + " ms";
    int baseline = 0;
    cv::Size textSize = cv::getTextSize(timeText, cv::FONT_HERSHEY_SIMPLEX, 1.0, 1, &baseline);
    cv::Point textPosition((ctx->width - textSize.width)/2, (ctx->height - textSize.height) / 2);
    cv::rectangle(image, textPosition, cv::Point(textPosition.x + textSize.width, textPosition.y - textSize.height), cv::Scalar(0, 0, 0, 0), cv::FILLED);
    cv::putText(image, timeText, textPosition, cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 0, 0, 255), 2);
    
    rga->setPatBuffer(ctx->buf, ModuleRga::BLEND_DST_OVER);
}


int main(int argc, char** argv)
{
    int ret;
    shared_ptr<ModuleRtspClient> rtsp_c = NULL;
    shared_ptr<ModuleMppDec> dec = NULL;
    shared_ptr<ModuleRga> rga = NULL;
    shared_ptr<ModuleDrmDisplay> drm_display = NULL;
    ImagePara input_para;
    External_ctx ctx1;
    ctx1.module = NULL;
    ctx1.buf = NULL;

    // 1. rtsp client module
    rtsp_c = make_shared<ModuleRtspClient>("rtsp://admin:firefly123@168.168.2.99:554/av_stream", RTSP_STREAM_TYPE_TCP);
    ret = rtsp_c->init();
    if (ret < 0) {
        ff_error("rtsp client init failed\n");
        return ret;
    }

    // 2. dec module
    input_para = rtsp_c->getOutputImagePara();
    dec = make_shared<ModuleMppDec>(input_para);
    dec->setProductor(rtsp_c);
    ret = dec->init();
    if (ret < 0) {
        ff_error("Dec init failed\n");
        return ret;
    }

    input_para = dec->getOutputImagePara();
    rga = make_shared<ModuleRga>(input_para, input_para, RGA_ROTATE_NONE);
    rga->setProductor(dec);
    ctx1.module = rga;
    rga->setBlendCallback(&ctx1, callback_blend);
    ret = rga->init();
    if (ret < 0) {
        ff_error("rga init failed\n");
        return ret;
    }

    // 4. drm display module
    input_para = rga->getOutputImagePara();
    drm_display = make_shared<ModuleDrmDisplay>(input_para);
    drm_display->setPlanePara(V4L2_PIX_FMT_NV12, 1);
    drm_display->setProductor(rga);
    ret = drm_display->init();
    if (ret < 0) {
        ff_error("drm display init failed\n");
        return ret;
    } else {
        uint32_t t_w, t_h;
        drm_display->getPlaneSize(&t_w, &t_h);
        uint32_t w = std::min(t_w / 2, input_para.width);
        uint32_t h = std::min(t_h / 2, input_para.height);
        uint32_t x = (t_w - w) / 2;
        uint32_t y = (t_h - h) / 2;

        ff_info("x y w h %d %d %d %d\n", x, y, w, h);
        drm_display->setWindowRect(x, y, w, h);
    }

    // 4. start origin producer
    rtsp_c->start();

    getchar();

    rtsp_c->stop();
    if (ctx1.buf)
        delete []static_cast<char*>(ctx1.buf);
}