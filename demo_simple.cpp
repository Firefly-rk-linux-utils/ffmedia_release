#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "vi/module_rtspClient.hpp"
#include "vp/module_mppdec.hpp"
#include "vo/module_drmDisplay.hpp"

int main(int argc, char** argv)
{
    int ret;
    ModuleRtspClient* rtsp_c = NULL;
    ModuleMppDec* dec = NULL;
    ModuleDrmDisplay* drm_display = NULL;
    ImagePara input_para;
    ImagePara output_para;

    ff_log_init();

    //1. rtsp client module
    rtsp_c = new ModuleRtspClient("rtsp://admin:firefly123@168.168.2.96:554/av_stream");
    ret = rtsp_c->init();
    if (ret < 0) {
        ff_error("rtsp client init failed\n");
        goto FAILED;
    }

    //2. dec module
    input_para = rtsp_c->getOutputImagePara();
    dec = new ModuleMppDec(input_para);
    dec->setProductor(rtsp_c);
    ret = dec->init();
    if (ret < 0) {
        ff_error("Dec init failed\n");
        goto FAILED;
    }

    //3. drm display module
    input_para = dec->getOutputImagePara();
    drm_display = new ModuleDrmDisplay(input_para);
    drm_display->setPlanePara(V4L2_PIX_FMT_NV12, 0,
                             ModuleDrmDisplay::PLANE_TYPE_OVERLAY_OR_PRIMARY, 1);
    drm_display->setProductor(dec);
    ret = drm_display->init();
    if (ret < 0) {
        ff_error("drm display init failed\n");
        goto FAILED;
    } else {
        uint32_t t_w, t_h;
        drm_display->getDisplayPlaneSize(&t_w, &t_h);
        uint32_t w = std::min(t_w/2, input_para.width);
        uint32_t h = std::min(t_h/2, input_para.height);
        uint32_t x = (t_w - w) / 2;
        uint32_t y = (t_h - h) / 2;

        ff_info("x y w h %d %d %d %d\n", x, y, w, h);
        drm_display->setWindowSize(x, y, w, h);
    }

    //4. start origin producer
    rtsp_c->start();

    getchar();

    rtsp_c->stop();

FAILED:
    //Just delete the orign producer and all its consumers will be deleted automatically
    if (rtsp_c)
        delete rtsp_c;
}
