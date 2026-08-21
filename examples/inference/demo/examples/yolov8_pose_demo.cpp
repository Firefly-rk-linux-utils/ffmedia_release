#include "module/vi/module_fileReader.hpp"
#include "module/vp/module_mppdec.hpp"
#include "infer/module_infer_rknn2_yolov8_position.hpp"
#include "track/module_byte_track.hpp"
#include "osd/module_osd_pose.hpp"

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>

using namespace FFMedia;

int main(int argc, char** argv)
{
    int ret;

    if (argc != 4) {
        ff_error("Usage: %s <video> <model.rknn> <labels.txt>", argv[0]);
        return 1;
    }

    auto src = std::make_shared<ModuleFileReader>(argv[1], true);
    ret = src->init();
    if (ret < 0) {
        ff_error("Failed to init file reader module\n");
        return ret;
    }

    auto mpp_dec = std::make_shared<ModuleMppDec>(src->getOutputImagePara());
    ret = mpp_dec->connectProducer(src);
    if (ret < 0) {
        ff_error("Failed to connect producer for mpp decoder module\n");
        return ret;
    }
    ret = mpp_dec->init();
    if (ret < 0) {
        ff_error("Failed to init mpp decoder module\n");
        return ret;
    }

    auto infer_module = std::make_shared<ModuleInferRKNN2Yolov8Position>("yolo_infer_pose", argv[2], argv[3]);
    ret = infer_module->connectProducer(mpp_dec);
    if (ret < 0) {
        ff_error("Failed to connect producer for infer module\n");
        return ret;
    }
    ret = infer_module->init();
    if (ret < 0) {
        ff_error("Failed to init infer module\n");
        return ret;
    }

    auto track_module = std::make_shared<ModuleByteTrack>("yolo_track", FFMedia::ModuleTrackFor::POSE);
    ret = track_module->connectProducer(infer_module);
    if (ret < 0) {
        ff_error("Failed to connect producer for track module\n");
        return ret;
    }
    ret = track_module->init();
    if (ret < 0) {
        ff_error("Failed to init track module\n");
        return ret;
    }

    auto osd_module = std::make_shared<ModuleOsdPose>("yolo_osd");
    ret = osd_module->connectProducer(track_module);
    if (ret < 0) {
        ff_error("Failed to connect producer for osd module\n");
        return ret;
    }
    ret = osd_module->init();
    if (ret < 0) {
        ff_error("Failed to init osd module\n");
        return ret;
    }

    osd_module->setMediaBufferProduceHooker([](const std::string&, int, const std::shared_ptr<MediaBuffer>& buffer) {
        auto infer_buffer = std::dynamic_pointer_cast<FFMedia::InferBuffer>(buffer);
        if (infer_buffer == nullptr)
            return;

        auto image_param = infer_buffer->getImagePara();
        cv::Mat img(image_param.height, image_param.width, CV_8UC3, infer_buffer->getActiveData(), image_param.hstride * 3);
        cv::imshow("yolov8_pose_demo", img);
        cv::waitKey(1);
    });


    src->start();
    getchar();
    src->stop();
    return 0;
}
