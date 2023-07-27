#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "module/vi/module_rtspClient.hpp"
#include "module/vp/module_mppdec.hpp"
#include "module/vo/module_drmDisplay.hpp"

int main(int argc, char** argv)
{
    int ret;
    shared_ptr<ModuleRtspClient> rtsp_c = NULL;
    shared_ptr<ModuleMppDec> dec = NULL;
    shared_ptr<ModuleDrmDisplay> drm_display = NULL;
    ImagePara input_para;
    ImagePara output_para;

    // 1. rtsp client module
    rtsp_c = make_shared<ModuleRtspClient>("rtsp://admin:firefly123@168.168.2.96:554/av_stream");
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

    // 3. drm display module
    input_para = dec->getOutputImagePara();
    drm_display = make_shared<ModuleDrmDisplay>(input_para);
    drm_display->setPlanePara(V4L2_PIX_FMT_NV12, 1);
    drm_display->setProductor(dec);
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
