#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "module/vi/module_rtspClient.hpp"
#include "module/vp/module_mppdec.hpp"
#include "module/vp/module_inference.hpp"
#include "module/vp/module_rga.hpp"
#include "postprocess.h"

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include "opencv2/imgproc.hpp"

struct External_ctx {
    shared_ptr<ModuleMedia> module;
    std::vector<rknn_tensor_attr*> output_attrs;
    std::vector<rknn_tensor_mem*> output_mems;
};

void callback_inf(void* _ctx, shared_ptr<MediaBuffer> buffer)
{
    External_ctx* ctx = static_cast<External_ctx*>(_ctx);
    shared_ptr<VideoBuffer> buf = static_pointer_cast<VideoBuffer>(buffer);
    void* ptr = buf->getActiveData();
    uint32_t width = buf->getImagePara().hstride;
    uint32_t height = buf->getImagePara().vstride;
    cv::Mat imgRgb(cv::Size(width, height), CV_8UC3, ptr);

    detect_result_group_t detect_result_group;
    std::vector<float> out_scales;
    std::vector<int32_t> out_zps;
    for (uint32_t i = 0; i < ctx->output_attrs.size(); ++i) {
        out_scales.push_back(ctx->output_attrs[i]->scale);
        out_zps.push_back(ctx->output_attrs[i]->zp);
    }
    post_process((int8_t*)ctx->output_mems[0]->virt_addr, (int8_t*)ctx->output_mems[1]->virt_addr, (int8_t*)ctx->output_mems[2]->virt_addr,
                 height, width,
                 BOX_THRESH, NMS_THRESH, 1, 1, out_zps, out_scales, &detect_result_group);

    char text[256];
    for (int i = 0; i < detect_result_group.count; i++) {
        detect_result_t* det_result = &(detect_result_group.results[i]);
        sprintf(text, "%s %.1f%%", det_result->name, det_result->prop * 100);
        int x1 = det_result->box.left;
        int y1 = det_result->box.top;
        int x2 = det_result->box.right;
        int y2 = det_result->box.bottom;
        rectangle(imgRgb, cv::Point(x1, y1), cv::Point(x2, y2), cv::Scalar(255, 0, 0, 255), 3);
        putText(imgRgb, text, cv::Point(x1, y1 + 12), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 0));
    }

    if (detect_result_group.count > 0)
        buf->flushDrmBuf();
}

void callback_rga(void* _ctx, shared_ptr<MediaBuffer> buffer)
{
    shared_ptr<VideoBuffer> buf = static_pointer_cast<VideoBuffer>(buffer);
    void* ptr = buf->getActiveData();
    uint32_t width = buf->getImagePara().hstride;
    uint32_t height = buf->getImagePara().vstride;
    // flush dma buf to cpu
    buf->invalidateDrmBuf();
    cv::Mat mat(cv::Size(width, height), CV_8UC3, ptr);
    cv::imshow("callback_rga", mat);
    cv::waitKey(1);
}


// Usage： ./demo_rknn rtsp://xxx ./model/RK3588/yolov5s-640-640.rknn
int main(int argc, char** argv)
{
    int ret = -1;
    shared_ptr<ModuleRtspClient> rtsp_c = NULL;
    shared_ptr<ModuleMppDec> dec = NULL;
    shared_ptr<ModuleInference> inf = NULL;
    shared_ptr<ModuleRga> rga = NULL;

    ImagePara input_para;
    ImagePara output_para;
    External_ctx* ctx1 = NULL;

    if (argc < 3) {
        ff_error("The number of parameters is incorrect\n");
        return ret;
    };

    do {

        rtsp_c = make_shared<ModuleRtspClient>(argv[1], RTSP_STREAM_TYPE_TCP);
        ret = rtsp_c->init();
        if (ret < 0) {
            ff_error("rtsp client init failed\n");
            break;
        }

        dec = make_shared<ModuleMppDec>();
        dec->setProductor(rtsp_c);
        ret = dec->init();
        if (ret < 0) {
            ff_error("Dec init failed\n");
            break;
        }

        inf = make_shared<ModuleInference>();
        inf->setProductor(dec);
        inf->setInferenceInterval(1);
        if (inf->setModelData(argv[2], 0) < 0) {
            ff_error("inf setModelData fail!\n");
            break;
        }
        ret = inf->init();
        if (ret < 0) {
            ff_error("inf init failed\n");
            break;
        }

        ctx1 = new External_ctx();
        ctx1->module = inf;
        ctx1->output_attrs = inf->getOutputAttrRef();
        ctx1->output_mems = inf->getOutputMemRef();
        inf->setOutputDataCallback(ctx1, callback_inf);

        input_para = dec->getOutputImagePara();
        output_para = input_para;
        output_para.width = input_para.width;
        output_para.height = input_para.height;
        output_para.hstride = output_para.width;
        output_para.vstride = output_para.height;
        output_para.v4l2Fmt = V4L2_PIX_FMT_BGR24;
        rga = make_shared<ModuleRga>(output_para, RGA_ROTATE_NONE);
        rga->setProductor(inf);
        rga->setOutputDataCallback(nullptr, callback_rga);
        ret = rga->init();
        if (ret < 0) {
            ff_error("rga init failed\n");
            break;
        }

        rtsp_c->start();
        getchar();
        rtsp_c->stop();

    } while (0);

    if (ctx1)
        delete ctx1;
    return ret;
}
