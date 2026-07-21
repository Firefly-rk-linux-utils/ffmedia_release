#pragma once

#include "module/vp/module_inference.hpp"

using namespace FFMedia;

class Yolov5s
{
private:
    std::shared_ptr<ModuleInference> model;
    std::vector<std::string> labels;

    unsigned int width, height;
    unsigned int img_width, img_height;
    float ratio_w, ratio_h;
    float nms_threshold, box_conf_threshold;


public:
    Yolov5s();
    Yolov5s(const Yolov5s&) = delete;
    Yolov5s& operator=(const Yolov5s&) = delete;
    ~Yolov5s();

    int loadLabel(const std::string& label_path);
    int init(const std::string& model_path, int core, const std::string& label_path);
    std::shared_ptr<VideoBuffer> infer(std::shared_ptr<VideoBuffer> buffer);
};
