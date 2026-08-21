#include "module_infer_rknn2_retina_face.hpp"
#include "utils/rknn_box_priors.h"
#include "utils/utils.hpp"
#include <math.h>
#include <cstring>


namespace FFMedia
{

ModuleInferRKNN2RetinaFace::ModuleInferRKNN2RetinaFace(
    const std::string& module_name,
    const std::string& model_path,
    float conf_threshold,
    float nms_threshold,
    float vis_threshold)
    : ModuleInferRKNN2(module_name, model_path, ""),
      _conf_threshold(conf_threshold),
      _nms_threshold(nms_threshold),
      _vis_threshold(vis_threshold)
{
    setOutputTensorDataType(FFTensorDataType::FLOAT32);
}

ModuleInferRKNN2RetinaFace::~ModuleInferRKNN2RetinaFace()
{
    stop();
}

void ModuleInferRKNN2RetinaFace::postprocess(const std::vector<FFTensor>& output_tensors, const std::shared_ptr<MediaBuffer>& input_buffer)
{
    auto infer_buffer = dynamic_cast<InferBuffer*>(input_buffer.get());
    if (infer_buffer == nullptr) {
        ff_error_m("Invalid input buffer, not an InferBuffer.");
        return;
    }

    infer_buffer->face_targets.clear();
    if (output_tensors.size() < 3
        || output_tensors[0].type != FFTensorDataType::FLOAT32
        || output_tensors[1].type != FFTensorDataType::FLOAT32
        || output_tensors[2].type != FFTensorDataType::FLOAT32) {
        ff_error_m("Invalid RetinaFace output tensors\n");
        return;
    }

    auto loc = static_cast<float*>(output_tensors[0].data);
    size_t num_elements = output_tensors[0].size / sizeof(float);  // 计算元素个数
    std::vector<float> location(loc, loc + num_elements);

    auto scores = static_cast<float*>(output_tensors[1].data);
    auto landms = static_cast<float*>(output_tensors[2].data);
    const float(*prior_ptr)[4];
    int num_priors = 0;
    if (_height == 320) {
        num_priors = 4200;  // anchors box number
        prior_ptr = BOX_PRIORS_320;
    } else if (_height == 640) {
        num_priors = 16800;  // anchors box number
        prior_ptr = BOX_PRIORS_640;
    } else {
        ff_error_m("model_shape error!!!\n");
        return;
    }

    std::vector<int> filter_indices = std::vector<int>(num_priors, 0);
    std::vector<float> props = std::vector<float>(num_priors, 0);

    int valid_count = filterValidResult(scores, location, landms, prior_ptr,
                                        filter_indices, props, _conf_threshold,
                                        num_priors);
    if (valid_count <= 0)
        return;

    quick_sort_indice_inverse(props, 0, valid_count - 1, filter_indices);
    const auto& output_para = getOutputImagePara();
    nms(valid_count, location, filter_indices, _nms_threshold, output_para.width, output_para.height);

    for (int i = 0; i < valid_count; i++) {
        if (filter_indices[i] == -1 || props[i] < _vis_threshold) {
            continue;
        }
        int n = filter_indices[i];
        float x = location[n * 4 + 0] * _width;
        float y = location[n * 4 + 1] * _height;
        float width = location[n * 4 + 2] * _width - x;
        float height = location[n * 4 + 3] * _height - y;
        FFFaceTarget face_target(static_cast<int>(clamp(x, 0, _width) / _scale),
                                 static_cast<int>(clamp(y, 0, _height) / _scale),
                                 static_cast<int>(clamp(width, 0, _width) / _scale),
                                 static_cast<int>(clamp(height, 0, _height) / _scale),
                                 props[i], 0);

        // Facial feature points
        for (int j = 0; j < 5; j++) {
            face_target.key_points.push_back({(static_cast<int>(clamp(landms[n * 10 + 2 * j] * _width, 0, _width) / _scale)),
                                              (static_cast<int>(clamp(landms[n * 10 + 2 * j + 1] * _height, 0, _height) / _scale))});
        }

        infer_buffer->face_targets.push_back(face_target);
    }

    return;
}


int ModuleInferRKNN2RetinaFace::filterValidResult(float* scores, std::vector<float>& loc, float* landms, const float boxPriors[][4],
                                                  std::vector<int>& filter_indices, std::vector<float>& obj_probs, float threshold, const int num_results)
{
    int validCount = 0;
    const float VARIANCES[2] = {0.1, 0.2};
    // Scale them back to the input size.
    for (int i = 0; i < num_results; ++i) {
        float face_score = scores[i * 2 + 1];
        if (face_score > threshold) {
            filter_indices[validCount] = i;
            obj_probs[validCount] = face_score;
            // decode location to origin position
            float xcenter = loc[i * 4 + 0] * VARIANCES[0] * boxPriors[i][2] + boxPriors[i][0];
            float ycenter = loc[i * 4 + 1] * VARIANCES[0] * boxPriors[i][3] + boxPriors[i][1];
            float w = (float)expf(loc[i * 4 + 2] * VARIANCES[1]) * boxPriors[i][2];
            float h = (float)expf(loc[i * 4 + 3] * VARIANCES[1]) * boxPriors[i][3];

            float xmin = xcenter - w * 0.5f;
            float ymin = ycenter - h * 0.5f;
            float xmax = xmin + w;
            float ymax = ymin + h;

            loc[i * 4 + 0] = xmin;
            loc[i * 4 + 1] = ymin;
            loc[i * 4 + 2] = xmax;
            loc[i * 4 + 3] = ymax;
            for (int j = 0; j < 5; ++j) {
                landms[i * 10 + 2 * j] = landms[i * 10 + 2 * j] * VARIANCES[0] * boxPriors[i][2] + boxPriors[i][0];
                landms[i * 10 + 2 * j + 1] = landms[i * 10 + 2 * j + 1] * VARIANCES[0] * boxPriors[i][3] + boxPriors[i][1];
            }
            ++validCount;
        }
    }

    return validCount;
}


}  // namespace FFMedia
