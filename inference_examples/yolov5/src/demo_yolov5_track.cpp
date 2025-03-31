#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <iostream>

#include "module/vi/module_ffmpegDemux.hpp"
#include "module/vp/module_mppdec.hpp"
#include "module/vp/module_inference.hpp"
#include "module/vp/module_rga.hpp"
#include "module/vp/module_mppenc.hpp"
#include "module/vo/module_rtspServer.hpp"
#include "postprocess.h"
#include "BYTETracker.h"

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include "opencv2/imgproc.hpp"

struct External_ctx {
    shared_ptr<ModuleMedia> module;
    BYTETracker tracker;
    std::vector<rknn_tensor_attr*> output_attrs;
    std::vector<rknn_tensor_mem*> output_mems;
    uint32_t model_width;
    uint32_t model_height;
    float ratio_w;
    float ratio_h;

    std::mutex mtx;
    std::condition_variable rga;
    std::condition_variable inf;
    int64_t pts;
};

void callback_rga(void* _ctx, shared_ptr<MediaBuffer> buffer)
{
    External_ctx* ctx = static_cast<External_ctx*>(_ctx);
    shared_ptr<VideoBuffer> buf = static_pointer_cast<VideoBuffer>(buffer);
    detect_result_group_t detect_result_group;
    {
        std::unique_lock<std::mutex> lk(ctx->mtx);
        int64_t pts = buf->getPUstimestamp();
        // Make sure to process the same frame of image
        while (pts != ctx->pts) {
            if (pts > ctx->pts) {
                // Notify the inference module to process the next frame of the image.
                ctx->inf.notify_one();
                if (ctx->rga.wait_for(lk, std::chrono::milliseconds(3000)) == cv_status::timeout)
                    return;
            } else {
                return;
            }
        }

        std::vector<float> out_scales;
        std::vector<int32_t> out_zps;
        for (uint32_t i = 0; i < ctx->output_attrs.size(); ++i) {
            out_scales.push_back(ctx->output_attrs[i]->scale);
            out_zps.push_back(ctx->output_attrs[i]->zp);
        }
        post_process((int8_t*)ctx->output_mems[0]->virt_addr, (int8_t*)ctx->output_mems[1]->virt_addr, (int8_t*)ctx->output_mems[2]->virt_addr,
                     ctx->model_height, ctx->model_width,
                     BOX_THRESH, NMS_THRESH, ctx->ratio_w, ctx->ratio_h, out_zps, out_scales, &detect_result_group);

        // Notify the inference module to process the next frame of the image.
        ctx->inf.notify_one();
    }

    uint32_t width = buf->getImagePara().hstride;
    uint32_t height = buf->getImagePara().vstride;
    cv::Mat imgBgr(cv::Size(width, height), CV_8UC3, buf->getActiveData());

    // std::vector<detect_result_t> objects;
    // for (int i = 0; i < detect_result_group.count; i++) {
    //     if (detect_result_group.results[i].id == 0) //person
    //         objects.push_back(detect_result_group.results[i]);
    // }
    // std::vector<STrack> output_stracks = ctx->tracker.update(objects);

    std::vector<detect_result_t> objects(detect_result_group.results,
                                         detect_result_group.results + detect_result_group.count);
    std::vector<STrack> output_stracks = ctx->tracker.update(objects);

    for (unsigned long i = 0; i < output_stracks.size(); i++) {
        std::vector<float> tlwh = output_stracks[i].tlwh;
        bool vertical = tlwh[2] / tlwh[3] > 1.6;
        if (tlwh[2] * tlwh[3] > 20 && !vertical) {
            cv::Scalar s = ctx->tracker.get_color(output_stracks[i].track_id);
            cv::putText(imgBgr, cv::format("%d", output_stracks[i].track_id), cv::Point(tlwh[0], tlwh[1] - 5),
                        0, 0.6, cv::Scalar(0, 0, 255), 2, cv::LINE_AA);
            cv::rectangle(imgBgr, cv::Rect(tlwh[0], tlwh[1], tlwh[2], tlwh[3]), s, 2);
        }
    }

    buf->invalidateDrmBuf();

    cv::imshow(ctx->module->getName(), imgBgr);
    cv::waitKey(1);
}

void callback_inf(void* _ctx, shared_ptr<MediaBuffer> buffer)
{
    External_ctx* ctx = static_cast<External_ctx*>(_ctx);
    {
        // Notify the rga module of processing and wait for the rga module to complete processing before returning.
        std::unique_lock<std::mutex> lk(ctx->mtx);
        ctx->pts = buffer->getPUstimestamp();
        ctx->rga.notify_one();
        ctx->inf.wait_for(lk, std::chrono::milliseconds(3000));
    }
}


// Usage： ./demo_yolov5_track /dev/videox ./model/RK3588/yolov5s-640-640.rknn port
int main(int argc, char** argv)
{
    int ret = -1;

    ImagePara input_para;
    ImagePara output_para;
    External_ctx* ctx1 = NULL;

    if (argc < 3) {
        ff_error("The number of parameters is incorrect\n");
        ff_error("\nUsage: ./demo_rknn rtsp://xxx ./model/RK3588/yolov5s-640-640.rknn\n");
        return ret;
    };

    do {
        shared_ptr<ModuleMedia> last_module;
        auto source = make_shared<ModuleFFmpegDemux>(argv[1], -1);
        ret = source->init();
        if (ret < 0) {
            ff_error("source init failed\n");
            break;
        }
        last_module = source;

        auto source_para = source->getOutputImagePara();
        if (v4l2fmtIsCompressed(source_para.v4l2Fmt)) {
            auto dec = make_shared<ModuleMppDec>();
            dec->setProductor(source);
            ret = dec->init();
            if (ret < 0) {
                ff_error("Dec init failed\n");
                break;
            }
            last_module = dec;
        }

        auto inf = make_shared<ModuleInference>();
        inf->setProductor(last_module);
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

        auto input_para = last_module->getOutputImagePara();
        auto output_para = input_para;
        output_para.width = input_para.width;
        output_para.height = input_para.height;
        output_para.hstride = output_para.width;
        output_para.vstride = output_para.height;
        output_para.v4l2Fmt = V4L2_PIX_FMT_BGR24;
        auto rga = make_shared<ModuleRga>(output_para, RGA_ROTATE_NONE);
        // The producer of the rga module is the same as the inference module producer.
        rga->setProductor(last_module);
        ret = rga->init();
        if (ret < 0) {
            ff_error("rga init failed\n");
            break;
        }

        ctx1 = new External_ctx();
        ctx1->module = rga;
        ctx1->tracker = BYTETracker(30, 30);
        ctx1->output_attrs = inf->getOutputAttr();
        ctx1->output_mems = inf->getOutputMem();
        // The resolution of the output image of the inference module depends on the model.
        input_para = inf->getOutputImagePara();
        output_para = rga->getOutputImagePara();
        ctx1->model_width = input_para.width;
        ctx1->model_height = input_para.height;
        auto inf_crop = inf->getOutputImageCrop();
        ctx1->ratio_w = (float)inf_crop.w / output_para.width;
        ctx1->ratio_h = (float)inf_crop.h / output_para.height;
        ctx1->pts = -1;

        rga->setOutputDataCallback(ctx1, callback_rga);
        inf->setOutputDataCallback(ctx1, callback_inf);

        source->start();
        getchar();
        source->stop();

    } while (0);


    if (ctx1)
        delete ctx1;
    return ret;
}
