#pragma once
#include "module_infer_rknn2.hpp"

namespace FFMedia
{

class ModuleInferRKNN2Yolov5 : public ModuleInferRKNN2
{
public:
    ModuleInferRKNN2Yolov5(const std::string& module_name,
                           const std::string& model_path,
                           const std::string& label_path,
                           float conf_threshold = 0.4,
                           float nms_threshold = 0.45);
    ~ModuleInferRKNN2Yolov5();

protected:
    virtual void postprocess(const std::vector<FFTensor>& output_tensors, const std::shared_ptr<MediaBuffer>& input_buffer) override;
    int processI8(int8_t* input, const int* anchor, int grid_h, int grid_w, int stride, std::vector<float>& boxes, std::vector<float>& obj_probs, std::vector<int>& class_id, float threshold, int32_t zp, float scale);
    int processFp32(float* input, const int* anchor, int grid_h, int grid_w, int stride, std::vector<float>& boxes, std::vector<float>& obj_probs, std::vector<int>& class_id, float threshold);

protected:
    float _conf_threshold;
    float _nms_threshold;

private:
    const int _anchors[3][6] = {{10, 13, 16, 30, 33, 23},
                                {30, 61, 62, 45, 59, 119},
                                {116, 90, 156, 198, 373, 326}};
};

}  // namespace FFMedia
