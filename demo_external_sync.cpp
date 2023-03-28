#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "base/media_buffer.hpp"
#include "base/video_buffer.hpp"
#include "base/pixel_fmt.hpp"
#include "base/ff_log.h"
#include "base/ff_type.hpp"

#include "module/module_media.hpp"
#include "vi/module_rtspClient.hpp"
#include "vp/module_mppdec.hpp"
#include "vp/module_rga.hpp"

#define ENABLE_OPENCV

//=============================================
#ifdef ENABLE_OPENCV
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#endif
//=============================================

#define UNUSED(x) [&x] {}()
using namespace std;

void callback_external(void* ctx, MediaBuffer* buffer)
{
    if (buffer == NULL || buffer->getMediaBufferType() != BUFFER_TYPE_VIDEO)
        return;
    VideoBuffer* buf = static_cast<VideoBuffer*>(buffer);

    void* ptr = buf->getActiveData();
    size_t size = buf->getActiveSize();
    uint32_t width = buf->getImagePara().hstride;
    uint32_t height = buf->getImagePara().vstride;

    UNUSED(size);
    //=================================================
#ifdef ENABLE_OPENCV
    cv::Mat mat(cv::Size(width, height), CV_8UC3, ptr);
    cv::imshow("external_test", mat);
    cv::waitKey(1);
#endif
    //=================================================
}

int main(int argc, char** argv)
{
    int ret;
    ModuleRtspClient* rtsp_c = NULL;
    ModuleMppDec* dec = NULL;
    ModuleRga* rga = NULL;
    ImagePara input_para;
    ImagePara output_para;

    ff_log_init();

    rtsp_c = new ModuleRtspClient("rtsp://168.168.10.20:8554/0");
    rtsp_c->setBufferCount(4);
    ret = rtsp_c->init();
    if (ret < 0) {
        ff_error("rtsp client init failed\n");
        goto FAILED;
    }

    input_para = rtsp_c->getOutputImagePara();
    dec = new ModuleMppDec(input_para);
    dec->setProductor(rtsp_c);
    dec->setBufferCount(15);
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
    output_para.v4l2Fmt = V4L2_PIX_FMT_RGB24;
    rga = new ModuleRga(input_para, output_para, RGA_ROTATE_NONE);
    rga->setProductor(dec);
    rga->setBufferCount(1);
    ret = rga->init();
    if (ret < 0) {
        ff_error("rga init failed\n");
        goto FAILED;
    }

    rga->setOutputDataCallback((void*)rga, callback_external);
    rtsp_c->start();

FAILED:
    if (dec)
        delete dec;
    if (rtsp_c)
        delete rtsp_c;
    if (rga)
        delete rga;
}
