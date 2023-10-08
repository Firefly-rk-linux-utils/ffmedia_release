#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "module/vi/module_rtspClient.hpp"
#include "module/vp/module_mppdec.hpp"
#include "module/vp/module_rga.hpp"
#include "module/vo/module_drmDisplay.hpp"

#include <opencv2/opencv.hpp>

struct rga_blend_ctx {
    shared_ptr<ModuleRga> rga;
    shared_ptr<VideoBuffer> vb;
};

void callback_blend(void* _ctx, shared_ptr<MediaBuffer> buffer)
{
    rga_blend_ctx* ctx = static_cast<rga_blend_ctx*>(_ctx);
    uint32_t width = ctx->vb->getImagePara().width;
    uint32_t height = ctx->vb->getImagePara().height;
    int buf_fd = ctx->vb->getBufFd();
    void* buf = ctx->vb->getData();

    cv::Mat image(cv::Size(width, height), CV_8UC4, buf);
    std::string timeText = "Current timestamp: " + std::to_string(buffer->getPUstimestamp() / 1000) + " ms";
    int baseline = 0;
    cv::Size textSize = cv::getTextSize(timeText, cv::FONT_HERSHEY_SIMPLEX, 1.0, 1, &baseline);
    cv::Point textPosition((width - textSize.width) / 2, (height - textSize.height) / 2);
    cv::rectangle(image, textPosition, cv::Point(textPosition.x + textSize.width, textPosition.y - textSize.height), cv::Scalar(0, 0, 0, 0), cv::FILLED);
    cv::putText(image, timeText, textPosition, cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 0, 255), 2);

    if (buf_fd > 0) {
        // flush data to dma
        ctx->vb->flushDrmBuf();
        ctx->rga->setPatBuffer(buf_fd, ModuleRga::BLEND_DST_OVER);
    } else {
        ctx->rga->setPatBuffer(buf, ModuleRga::BLEND_DST_OVER);
    }
}


int main(int argc, char** argv)
{
    int ret;
    shared_ptr<ModuleRtspClient> rtsp_c = NULL;
    shared_ptr<ModuleMppDec> dec = NULL;
    shared_ptr<ModuleRga> rga = NULL;
    shared_ptr<ModuleDrmDisplay> drm_display = NULL;
    ImagePara input_para;
    rga_blend_ctx blend_ctx;
    blend_ctx.rga = NULL;
    blend_ctx.vb = NULL;

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

    // 3. rga module
    input_para = dec->getOutputImagePara();
    rga = make_shared<ModuleRga>(input_para, input_para, RGA_ROTATE_NONE);
    rga->setProductor(dec);
    ret = rga->init();
    if (ret < 0) {
        ff_error("rga init failed\n");
        return ret;
    }


    // 4. rga blend ctx
    input_para = rga->getOutputImagePara();
    // Use rga output ImagePara construct the blend BGRA ImagePara
    ImagePara BGRA_para(input_para.width, input_para.height, input_para.hstride, input_para.vstride, V4L2_PIX_FMT_BGR32);
    blend_ctx.rga = rga;
    blend_ctx.vb = make_shared<VideoBuffer>(VideoBuffer::DRM_BUFFER_CACHEABLE);
    blend_ctx.vb->allocBuffer(BGRA_para);
    memset(blend_ctx.vb->getData(), 0, blend_ctx.vb->getSize());
    // set the blend image para
    rga->setPatPara(BGRA_para.v4l2Fmt, 0, 0, BGRA_para.width, BGRA_para.height, BGRA_para.hstride, BGRA_para.vstride);
    if (blend_ctx.vb->getBufFd() > 0)
        rga->setPatBuffer(blend_ctx.vb->getBufFd(), ModuleRga::BLEND_DST_OVER);
    else
        rga->setPatBuffer(blend_ctx.vb->getData(), ModuleRga::BLEND_DST_OVER);
    /*
        If not dynamically processed blend image.
        no need to set up blend callback processing.
    */
    rga->setBlendCallback(&blend_ctx, callback_blend);


    // 5. drm display module
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
}
