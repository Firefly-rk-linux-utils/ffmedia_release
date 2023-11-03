#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "module/vi/module_rtspClient.hpp"
#include "module/vp/module_mppdec.hpp"
#include "module/vo/module_drmDisplay.hpp"

const char* rtsp_url1 = "rtsp://admin:firefly123@168.168.2.99:554/av_stream";
const char* rtsp_url2 = "rtsp://admin:firefly123@168.168.2.96:554/av_stream";

int main(int argc, char** argv)
{
    shared_ptr<ModuleRtspClient> rtsp_c = NULL;
    shared_ptr<ModuleMppDec> dec = NULL;
    shared_ptr<ModuleDrmDisplay> drm_display = NULL;

    // 1. rtsp client module
    rtsp_c = make_shared<ModuleRtspClient>(rtsp_url1);

    // 2. dec module
    dec = make_shared<ModuleMppDec>();
    dec->setProductor(rtsp_c);

    // 3. drm display module
    drm_display = make_shared<ModuleDrmDisplay>();
    drm_display->setPlanePara(V4L2_PIX_FMT_NV12, 1);
    drm_display->setProductor(dec);

    // 4. start origin producer
    rtsp_c->start();
    getchar();
    rtsp_c->stop();

    rtsp_c->changeSource(rtsp_url2);
    rtsp_c->start();
    getchar();
    rtsp_c->stop();
    return 0;
}
