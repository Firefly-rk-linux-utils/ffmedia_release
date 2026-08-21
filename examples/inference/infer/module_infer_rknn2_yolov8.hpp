#pragma once
#include "module_infer_rknn2.hpp"

namespace FFMedia
{

class ModuleInferRKNN2Yolov8 : public ModuleInferRKNN2
{
public:
    ModuleInferRKNN2Yolov8(const std::string& module_name,
                           const std::string& model_path,
                           const std::string& label_path,
                           float conf_threshold = 0.4,
                           float nms_threshold = 0.45);
    ~ModuleInferRKNN2Yolov8();

protected:
    virtual void postprocess(const std::vector<FFTensor>& output_tensors, const std::shared_ptr<MediaBuffer>& input_buffer) override;
    int processI8(int8_t* box_tensor, int32_t box_zp, float box_scale,
                  int8_t* score_tensor, int32_t score_zp, float score_scale,
                  int8_t* score_sum_tensor, int32_t score_sum_zp, float score_sum_scale,
                  int grid_h, int grid_w, int stride, int dfl_len,
                  std::vector<float>& boxes,
                  std::vector<float>& objProbs,
                  std::vector<int>& classId,
                  float threshold);
    int processFp32(float* box_tensor, float* score_tensor, float* score_sum_tensor,
                    int grid_h, int grid_w, int stride, int dfl_len,
                    std::vector<float>& boxes,
                    std::vector<float>& objProbs,
                    std::vector<int>& classId,
                    float threshold);

protected:
    float _conf_threshold;
    float _nms_threshold;
};

}  // namespace FFMedia