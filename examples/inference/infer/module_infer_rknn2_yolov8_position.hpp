#pragma once
#include "module_infer_rknn2_yolov8.hpp"

namespace FFMedia
{

/**
 * @brief 基于RKNN2的Yolov8目标检测推理模块，
 */
class ModuleInferRKNN2Yolov8Position : public ModuleInferRKNN2Yolov8
{
public:
    ModuleInferRKNN2Yolov8Position(const std::string& module_name,
                                   const std::string& model_path,
                                   const std::string& label_path,
                                   float conf_threshold = 0.4,
                                   float nms_threshold = 0.45);
    ~ModuleInferRKNN2Yolov8Position();

protected:
    virtual void postprocess(const std::vector<FFTensor>& output_tensors, const std::shared_ptr<MediaBuffer>& input_buffer) override;
    int processI8(int8_t* input, int grid_h, int grid_w, int stride,
                  std::vector<float>& boxes, std::vector<float>& boxScores, std::vector<int>& classId, float threshold,
                  int32_t zp, float scale, int index);
    int processFp32(float* input, int grid_h, int grid_w, int stride,
                    std::vector<float>& boxes, std::vector<float>& boxScores, std::vector<int>& classId, float threshold,
                    int32_t zp, float scale, int index);

    static int yolov8_pose_nms(int validCount, std::vector<float>& outputLocations, std::vector<int> classIds, std::vector<int>& order,
                               int filterId, float threshold);

protected:
    float _conf_threshold;
    float _nms_threshold;
};

}  // namespace FFMedia