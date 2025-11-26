#include <fstream>
#include "yolov5s.hpp"

#include "postprocess.h"

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include "opencv2/imgproc.hpp"

Yolov5s::Yolov5s()
{
    model = make_shared<ModuleInference>();
    box_conf_threshold = BOX_THRESH;
    nms_threshold = NMS_THRESH;
}

Yolov5s::~Yolov5s()
{
}

int Yolov5s::loadLabel(const std::string& label_path)
{
    std::ifstream file(label_path);
    if (!file.is_open())
        return -1;

    labels.clear();
    std::string line;
    while (getline(file, line)) {
        labels.push_back(line);
        if (labels.size() > 200)
            break;
    }
    return 0;
}

int Yolov5s::init(const std::string& model_path, int core, const std::string& lable_path)
{
    int ret;

    ret = loadLabel(lable_path);
    if (ret < 0) {
        ff_error_m("Failed to load model label\n");
        return ret;
    }

    ModuleInference::NPU_SCHEDULER_CORE core_mask = ModuleInference::NPU_CORE_0;
    switch (core % 3) {
        case 0:
            core_mask = ModuleInference::NPU_CORE_0;
            break;
        case 1:
            core_mask = ModuleInference::NPU_CORE_1;
            break;
        case 2:
            core_mask = ModuleInference::NPU_CORE_2;
            break;
    }

    model->removeModel();
    ret = model->setModelData(const_cast<char*>(model_path.c_str()), 0, core_mask);
    if (ret < 0) {
        ff_error_m("Failed to set model data\n");
        return ret;
    }

    ret = model->init();
    if (ret) {
        ff_error_m("Failed to init model\n");
        return ret;
    }

    width = model->getOutputImagePara().width;
    height = model->getOutputImagePara().height;
    img_width = 0;
    img_height = 0;
    return ret;
}

std::shared_ptr<VideoBuffer> Yolov5s::infer(std::shared_ptr<VideoBuffer> buffer)
{
    auto buf_image = buffer->getImagePara();
    if (buf_image.v4l2Fmt != V4L2_PIX_FMT_BGR24)
        return buffer;

    if (buf_image.width != img_width || buf_image.height != img_height) {
        model->changedInputImagePara(buf_image);
        auto img_crop = model->getOutputImageCrop();
        ratio_w = (float)img_crop.w / buf_image.width;
        ratio_h = (float)img_crop.h / buf_image.height;

        img_width = buf_image.width;
        img_height = buf_image.height;
    }

    // inference
    auto ret = model->inference(buffer);
    if (ret != 0)
        return buffer;

    // post process
    detect_result_group_t detect_result_group;
    auto output_attrs = model->getOutputAttr();
    auto output_mems = model->getOutputMem();
    std::vector<float> out_scales;
    std::vector<int32_t> out_zps;
    for (auto& it : output_attrs) {
        out_scales.push_back(it->scale);
        out_zps.push_back(it->zp);
    }

    post_process((int8_t*)output_mems[0]->virt_addr, (int8_t*)output_mems[1]->virt_addr,
                 (int8_t*)output_mems[2]->virt_addr, height, width,
                 box_conf_threshold, nms_threshold, ratio_w, ratio_h,
                 out_zps, out_scales, &detect_result_group);


    cv::Mat img(buf_image.height, buf_image.width, CV_8UC3, buffer->getActiveData(), buf_image.hstride * 3);
    // draw
    char text[256];
    for (int i = 0; i < detect_result_group.count; i++) {
        detect_result_t* det_result = &(detect_result_group.results[i]);
        sprintf(text, "%s %.1f%%", labels.at(det_result->id).c_str(), det_result->prop * 100);
        int x1 = det_result->box.left;
        int y1 = det_result->box.top;
        int x2 = det_result->box.right;
        int y2 = det_result->box.bottom;
        rectangle(img, cv::Point(x1, y1), cv::Point(x2, y2), cv::Scalar(255, 0, 0, 255), 3);
        putText(img, text, cv::Point(x1 + 10, y1 + 20), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 0, 0), 2);
    }

    return buffer;
}
