#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "module/vi/module_rtspClient.hpp"
#include "module/vp/module_mppdec.hpp"
#include "module/vo/module_drmDisplay.hpp"

int main(int argc, char** argv)
{

    int count = 0;
    int ret;
    shared_ptr<ModuleRtspClient> rtsp_c = NULL;
    shared_ptr<ModuleMppDec> dec = NULL;
    shared_ptr<ModuleDrmDisplay> drm_display0 = NULL;
    shared_ptr<ModuleDrmDisplay> drm_display1 = NULL;
    shared_ptr<ModuleDrmDisplay> drm_display2 = NULL;
    shared_ptr<ModuleDrmDisplay> drm_display3 = NULL;
    ImagePara input_para;

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

    // ZPOS 1, 1/2 window
    input_para = dec->getOutputImagePara();
    drm_display0 = make_shared<ModuleDrmDisplay>(input_para);
    drm_display0->setPlanePara(V4L2_PIX_FMT_NV12, 1);
    drm_display0->setPlaneRect(50, 50, 1200, 400);
    drm_display0->setProductor(dec);
    ret = drm_display0->init();
    if (ret < 0) {
        ff_error("drm display init failed\n");
        return ret;
    }
    drm_display0->setWindowRect(0, 0, 540, 360);

    // ZPOS 1, 2/2 window
    drm_display1 = make_shared<ModuleDrmDisplay>(input_para);
    drm_display1->setPlanePara(V4L2_PIX_FMT_NV12, 1);
    // same zpos as drm_display0, so the plane size can't be set again
    // drm_display1->setPlaneRect(50, 50, 1200, 400);
    drm_display1->setProductor(dec);
    ret = drm_display1->init();
    if (ret < 0) {
        ff_error("drm display init failed\n");
        return ret;
    }
    drm_display1->setWindowRect(600, 20, 540, 360);

    // ZPOS 2, 1 window
    drm_display2 = make_shared<ModuleDrmDisplay>(input_para);
    drm_display2->setPlanePara(V4L2_PIX_FMT_NV12, 2);
    drm_display2->setPlaneRect(70, 380, 700, 500);
    drm_display2->setProductor(dec);
    ret = drm_display2->init();
    if (ret < 0) {
        ff_error("drm display init failed\n");
        return ret;
    }
    drm_display2->setWindowRect(10, 20, 600, 400);

    // ZPOS 3, 1 window, use default size(same as plane size)
    drm_display3 = make_shared<ModuleDrmDisplay>(input_para);
    drm_display3->setPlanePara(V4L2_PIX_FMT_NV12, 3);
    drm_display3->setPlaneRect(550, 450, 540, 380);
    drm_display3->setProductor(dec);
    ret = drm_display3->init();
    if (ret < 0) {
        ff_error("drm display init failed\n");
        return ret;
    }

    // 4. start origin producer
    rtsp_c->start();

    while (count < 500) {
        count++;
        drm_display3->move(count, count);
        usleep(1000 * 100);
    }

    getchar();

    rtsp_c->stop();
}
