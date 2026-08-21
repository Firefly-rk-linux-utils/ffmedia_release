#pragma once
#include "module_infer_rknn2.hpp"

namespace FFMedia
{

class ModuleInferRKNN2RetinaFace : public ModuleInferRKNN2
{
public:
    ModuleInferRKNN2RetinaFace(const std::string& module_name,
                               const std::string& model_path,
                               float conf_threshold = 0.5,
                               float nms_threshold = 0.4,
                               float vis_threshold = 0.4);
    ~ModuleInferRKNN2RetinaFace();

protected:
    virtual void postprocess(const std::vector<FFTensor>& output_tensors, const std::shared_ptr<MediaBuffer>& input_buffer) override;
    int filterValidResult(float* scores, std::vector<float>& loc, float* landms, const float boxPriors[][4],
                          std::vector<int>& filter_indices, std::vector<float>& obj_probs, float threshold, const int num_results);

protected:
    float _conf_threshold;
    float _nms_threshold;
    float _vis_threshold;
};

}  // namespace FFMedia