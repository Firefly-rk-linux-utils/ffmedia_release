#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <termios.h>

#include "module/vi/module_rtspClient.hpp"
#include "module/vp/module_mppdec.hpp"
#include "module/vo/module_drmDisplay.hpp"

static int mygetch(void)
{
    struct termios oldt, newt;
    int ch;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return ch;
}

int main(int argc, char** argv)
{
    int ret;
    shared_ptr<ModuleRtspClient> rtsp_c = NULL;
    shared_ptr<ModuleMppDec> dec = NULL;
    shared_ptr<ModuleDrmDisplay> windows[16];
    ImagePara input_para;

    // 1. rtsp client module
    rtsp_c = make_shared<ModuleRtspClient>("rtsp://admin:firefly123@168.168.2.99:554/av_stream");
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

    shared_ptr<DrmDisplayPlane> plane = make_shared<DrmDisplayPlane>(V4L2_PIX_FMT_NV12, 0);
    plane->setRect(100, 100, 1600, 800);
    plane->setWindowLayoutMode(DrmDisplayPlane::RELATIVE_LAYOUT);
    plane->splitPlane(3, 3);

    for (int i = 0; i < 16; i++) {
        windows[i] = make_shared<ModuleDrmDisplay>(input_para, plane);
        windows[i]->setProductor(dec);
        windows[i]->init();
    }

    windows[0]->setWindowRelativeRect(0, 0, 2, 2, false);
    windows[1]->setWindowRelativeRect(2, 0, 1, 1, false);
    windows[2]->setWindowRelativeRect(2, 1, 1, 1, false);
    windows[3]->setWindowRelativeRect(0, 2, 1, 1, false);
    windows[4]->setWindowRelativeRect(1, 2, 1, 1, false);
    windows[5]->setWindowRelativeRect(2, 2, 1, 1, false);
    for (int i = 6; i < 16; i++) {
        windows[i]->setWindowRelativeRect(2, 2, 0, 0, false);
    }

    /*
    spilt plane to 3 x 3
     ------------------------------
    |         |         |         |
    |         |         |         |
    |         |         |         |
     ------------------------------
    |         |         |         |
    |         |         |         |
    |         |         |         |
     ------------------------------
    |         |         |         |
    |         |         |         |
    |         |         |         |
     ------------------------------

     windows
     ------------------------------
    |                   |         |
    |                   |  win1   |
    |                   |         |
    |       win0        -----------
    |                   |         |
    |                   |  win2   |
    |                   |         |
     ------------------------------
    |         |         |         |
    |   win3  |  win4   |  win5   |
    |         |         |         |
     ------------------------------
    */

    rtsp_c->start();

    while (true) {
        int cmd = mygetch();
        switch (cmd) {
            case '1':  // 1 window, full plane
                windows[0]->setWindowFullPlane();
                break;

            case 't':
                windows[0]->restoreWindowFromFullPlane();
                break;

            case '4':  // 4 windows
                plane->splitPlane(2, 2);
                windows[0]->setWindowRelativeRect(0, 0, 1, 1, false);
                windows[1]->setWindowRelativeRect(1, 0, 1, 1, false);
                windows[2]->setWindowRelativeRect(0, 1, 1, 1, false);
                windows[3]->setWindowRelativeRect(1, 1, 1, 1, false);

                for (int i = 4; i < 16; i++) {
                    windows[i]->setWindowRelativeRect(1, 1, 0, 0, false);
                }
                plane->flushAllWindowRectUpdate();
                break;

            case '6':  // 6 windows
                plane->splitPlane(3, 3);
                windows[0]->setWindowRelativeRect(0, 0, 2, 2, false);
                windows[1]->setWindowRelativeRect(2, 0, 1, 1, false);
                windows[2]->setWindowRelativeRect(2, 1, 1, 1, false);
                windows[3]->setWindowRelativeRect(0, 2, 1, 1, false);
                windows[4]->setWindowRelativeRect(1, 2, 1, 1, false);
                windows[5]->setWindowRelativeRect(2, 2, 1, 1, false);

                for (int i = 6; i < 16; i++) {
                    windows[i]->setWindowRelativeRect(2, 2, 0, 0, false);
                }
                plane->flushAllWindowRectUpdate();
                break;

            case '9':  // 9 windows
                plane->splitPlane(3, 3);
                for (int i = 0; i < 3; i++) {
                    for (int j = 0; j < 3; j++) {
                        windows[i * 3 + j]->setWindowRelativeRect(i, j, 1, 1, false);
                    }
                }

                for (int i = 9; i < 16; i++) {
                    windows[i]->setWindowRelativeRect(1, 1, 0, 0, false);
                }
                plane->flushAllWindowRectUpdate();
                break;

            case 'm':  // 16 windows
                plane->splitPlane(4, 4);
                for (int i = 0; i < 4; i++) {
                    for (int j = 0; j < 4; j++) {
                        windows[i * 4 + j]->setWindowRelativeRect(i, j, 1, 1, false);
                    }
                }

                plane->flushAllWindowRectUpdate();
                break;

            case 'f':  // full screen
                plane->setPlaneFullScreen();
                break;

            case 'r':
                plane->restorePlaneFromFullScreen();
                break;

            case 'F':  // full screen
                windows[0]->setWindowFullScreen();
                break;

            case 'R':
                windows[0]->restoreWindowFromFullScreen();
                break;

            case 'q':
                goto EXIT;
                break;

            default:
                break;
        }
    }

EXIT:
    rtsp_c->stop();
}
