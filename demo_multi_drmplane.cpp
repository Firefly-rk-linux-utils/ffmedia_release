#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "vi/module_rtspClient.hpp"
#include "vp/module_mppdec.hpp"
#include "vo/module_drmDisplay.hpp"

int main(int argc, char** argv)
{

    int count = 0;
    int ret;
    ModuleRtspClient* rtsp_c = NULL;
    ModuleMppDec* dec = NULL;
    ModuleDrmDisplay* drm_display0 = NULL;
    ModuleDrmDisplay* drm_display1 = NULL;
    ModuleDrmDisplay* drm_display2 = NULL;
    ModuleDrmDisplay* drm_display3 = NULL;
    ImagePara input_para;

    ff_log_init();

    // 1. rtsp client module
    rtsp_c = new ModuleRtspClient("rtsp://admin:firefly123@168.168.2.96:554/av_stream");
    ret = rtsp_c->init();
    if (ret < 0) {
        ff_error("rtsp client init failed\n");
        goto FAILED;
    }

    // 2. dec module
    input_para = rtsp_c->getOutputImagePara();
    dec = new ModuleMppDec(input_para);
    dec->setProductor(rtsp_c);
    ret = dec->init();
    if (ret < 0) {
        ff_error("Dec init failed\n");
        goto FAILED;
    }

    // 3. drm display module

    // ZPOS 1, 1/2 window
    input_para = dec->getOutputImagePara();
    drm_display0 = new ModuleDrmDisplay(input_para);
    drm_display0->setPlanePara(V4L2_PIX_FMT_NV12, 1);
    drm_display0->setPlaneSize(50, 50, 1200, 400);
    drm_display0->setProductor(dec);
    ret = drm_display0->init();
    if (ret < 0) {
        ff_error("drm display init failed\n");
        goto FAILED;
    }
    drm_display0->setWindowSize(0, 0, 540, 360);

    // ZPOS 1, 2/2 window
    drm_display1 = new ModuleDrmDisplay(input_para);
    drm_display1->setPlanePara(V4L2_PIX_FMT_NV12, 1);
    // same zpos as drm_display0, so the plane size can't be set again
    // drm_display1->setPlaneSize(50, 50, 1200, 400);
    drm_display1->setProductor(dec);
    ret = drm_display1->init();
    if (ret < 0) {
        ff_error("drm display init failed\n");
        goto FAILED;
    }
    drm_display1->setWindowSize(600, 20, 540, 360);

    // ZPOS 2, 1 window
    drm_display2 = new ModuleDrmDisplay(input_para);
    drm_display2->setPlanePara(V4L2_PIX_FMT_NV12, 2);
    drm_display2->setPlaneSize(70, 380, 700, 500);
    drm_display2->setProductor(dec);
    ret = drm_display2->init();
    if (ret < 0) {
        ff_error("drm display init failed\n");
        goto FAILED;
    }
    drm_display2->setWindowSize(10, 20, 600, 400);

    // ZPOS 3, 1 window, use default size(same as plane size)
    drm_display3 = new ModuleDrmDisplay(input_para);
    drm_display3->setPlanePara(V4L2_PIX_FMT_NV12, 3);
    drm_display3->setPlaneSize(550, 450, 540, 380);
    drm_display3->setProductor(dec);
    ret = drm_display3->init();
    if (ret < 0) {
        ff_error("drm display init failed\n");
        goto FAILED;
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

FAILED:
    // Just delete the orign producer and all its consumers will be deleted automatically
    if (rtsp_c)
        delete rtsp_c;
}
