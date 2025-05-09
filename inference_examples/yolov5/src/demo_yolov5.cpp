#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "module/vi/module_ffmpegDemux.hpp"
#include "module/vp/module_mppdec.hpp"
#include "module/vp/module_inference.hpp"
#include "module/vp/module_rga.hpp"
#include "module/vo/module_drmDisplay.hpp"
#include "postprocess.h"

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include "opencv2/imgproc.hpp"

// #define TEST_INFERENCE_TIME

#define LABEL_NALE_TXT_PATH "./model/coco_80_labels_list.txt"
static char* labels[OBJ_CLASS_NUM];

struct External_ctx {
    shared_ptr<ModuleInference> inf;
    uint32_t model_width;
    uint32_t model_height;
    float ratio_w;
    float ratio_h;

    detect_result_group_t detect_result_group;
};

void callback_external(void* _ctx, shared_ptr<MediaBuffer> buffer)
{
    External_ctx* ctx = static_cast<External_ctx*>(_ctx);

#ifdef TEST_INFERENCE_TIME
    auto start = std::chrono::high_resolution_clock::now();
#endif
    // inference
    auto ret = ctx->inf->inference(buffer);

#ifdef TEST_INFERENCE_TIME
    auto end = std::chrono::high_resolution_clock::now();
    printf("once run use %ld us\n", std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());
#endif

    if (ret == 0) {
        auto output_attrs = ctx->inf->getOutputAttr();
        auto output_mems = ctx->inf->getOutputMem();
        std::vector<float> out_scales;
        std::vector<int32_t> out_zps;
        for (auto it : output_attrs) {
            out_scales.push_back(it->scale);
            out_zps.push_back(it->zp);
        }

        post_process((int8_t*)output_mems[0]->virt_addr, (int8_t*)output_mems[1]->virt_addr,
                     (int8_t*)output_mems[2]->virt_addr, ctx->model_height, ctx->model_width,
                     BOX_THRESH, NMS_THRESH, ctx->ratio_w, ctx->ratio_h, out_zps, out_scales,
                     &ctx->detect_result_group);
    } else {
        ctx->detect_result_group.count = 0;
    }

    uint32_t width = buffer->getImagePara().hstride;
    uint32_t height = buffer->getImagePara().vstride;
    cv::Mat img(cv::Size(width, height), CV_8UC3, buffer->getActiveData());

    // draw
    char text[256];
    for (int i = 0; i < ctx->detect_result_group.count; i++) {
        detect_result_t* det_result = &(ctx->detect_result_group.results[i]);
        sprintf(text, "%s %.1f%%", labels[det_result->id], det_result->prop * 100);
        int x1 = det_result->box.left;
        int y1 = det_result->box.top;
        int x2 = det_result->box.right;
        int y2 = det_result->box.bottom;
        rectangle(img, cv::Point(x1, y1), cv::Point(x2, y2), cv::Scalar(255, 0, 0, 255), 3);
        putText(img, text, cv::Point(x1 + 10, y1 + 20), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 0, 0), 2);
    }

    static_pointer_cast<VideoBuffer>(buffer)->invalidateDrmBuf();

    // cv::imshow("inference test", img);
    // cv::waitKey(1);
}

// Usage： ./demo_yolov5 test.mp4 ./model/RK3588/yolov5s-640-640.rknn ./model/coco_80_labels_list.txt
int main(int argc, char** argv)
{
    int ret = -1;
    External_ctx* ctx = NULL;

    if (argc < 3) {
        ff_error("The number of parameters is incorrect\n");
        ff_error("\nUsage: ./demo_rknn test.mp4 ./model/RK3588/yolov5s-640-640.rknn ./model/coco_80_labels_list.txt\n");
        return ret;
    };

    if (argc > 3) {
        ret = loadLabelName(argv[3], labels, OBJ_CLASS_NUM);
    } else {
        ret = loadLabelName(LABEL_NALE_TXT_PATH, labels, OBJ_CLASS_NUM);
    }
    if (ret < 0) {
        printf("Failed to load lable name\n");
        return -1;
    }

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
            dec->setProductor(source);  // Join the source module consumer queue.
            ret = dec->init();
            if (ret < 0) {
                ff_error("Dec init failed\n");
                break;
            }
            last_module = dec;
        }
        /*
            The rga module is used to convert the output image format of the
            decode module into bgr24 and adjust the virtual width and virtual height,
            which is convenient for opencv display.
        */
        auto input_para = last_module->getOutputImagePara();
        auto output_para = input_para;
        output_para.hstride = output_para.width;
        output_para.vstride = output_para.height;
        output_para.v4l2Fmt = V4L2_PIX_FMT_BGR24;
        auto rga = make_shared<ModuleRga>(output_para, RGA_ROTATE_NONE);
        rga->setProductor(last_module);  // Join the last module consumer queue.
        ret = rga->init();
        if (ret < 0) {
            ff_error("rga init failed\n");
            break;
        }


        // drm display
        auto drm_display = make_shared<ModuleDrmDisplay>();
        drm_display->setPlanePara(V4L2_PIX_FMT_NV12);
        drm_display->setProductor(rga);  // Join the rga module consumer queue.
        drm_display->setSynchronize(make_shared<Synchronize>(SYNCHRONIZETYPE_VIDEO));
        ret = drm_display->init();
        if (ret < 0) {
            ff_error("drm display init failed\n");
            return ret;
        } else {
            input_para = drm_display->getInputImagePara();
            uint32_t t_w, t_h;
            drm_display->getPlaneSize(&t_w, &t_h);
            uint32_t w = std::min(t_w, input_para.width);
            uint32_t h = std::min(t_h, input_para.height);
            uint32_t x = (t_w - w) / 2;
            uint32_t y = (t_h - h) / 2;
            ff_info("x y w h %d %d %d %d\n", x, y, w, h);
            drm_display->setPlaneRect(x, y, w, h);
        }

        /*
            The inference module is used for manual inference and is not added
            to the module link.
        */
        auto inf = make_shared<ModuleInference>(rga->getOutputImagePara());
        // inf->setInferenceInterval(1);
        if (inf->setModelData(argv[2], 0) < 0) {
            ff_error("inf setModelData fail!\n");
            break;
        }
        ret = inf->init();
        if (ret < 0) {
            ff_error("inf init failed\n");
            break;
        }

        ctx = new External_ctx();
        ctx->inf = inf;
        auto inf_crop = inf->getOutputImageCrop();
        ctx->model_width = inf->getOutputImagePara().width;
        ctx->model_height = inf->getOutputImagePara().height;
        ctx->ratio_w = (float)inf_crop.w / output_para.width;
        ctx->ratio_h = (float)inf_crop.h / output_para.height;

        rga->setOutputDataCallback(ctx, callback_external);

        source->start();
        getchar();
        source->stop();

    } while (0);

    if (ctx)
        delete ctx;

    deinitLabelName(labels, OBJ_CLASS_NUM);
    return ret;
}
