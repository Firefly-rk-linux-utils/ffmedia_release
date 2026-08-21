#include "module_infer_rknn2_yolov8.hpp"
#include "utils/utils.hpp"
#include <set>

namespace FFMedia
{

ModuleInferRKNN2Yolov8::ModuleInferRKNN2Yolov8(const std::string& module_name,
                                               const std::string& model_path,
                                               const std::string& label_path,
                                               float conf_threshold,
                                               float nms_threshold)
    : ModuleInferRKNN2(module_name, model_path, label_path),
      _conf_threshold(conf_threshold),
      _nms_threshold(nms_threshold)
{
}

ModuleInferRKNN2Yolov8::~ModuleInferRKNN2Yolov8()
{
    stop();
}

void ModuleInferRKNN2Yolov8::postprocess(const std::vector<FFTensor>& output_tensors, const std::shared_ptr<MediaBuffer>& input_buffer)
{
    auto infer_buffer = dynamic_cast<InferBuffer*>(input_buffer.get());
    if (infer_buffer == nullptr) {
        ff_error_m("Invalid input buffer, not an InferBuffer.");
        return;
    }

    infer_buffer->targets.clear();
    if (_labels.empty()) {
        ff_error_m("Invalid YOLOv8 labels\n");
        return;
    }

    std::vector<float> filter_boxes;
    std::vector<float> obj_probs;
    std::vector<int> class_id;
    int valid_count = 0;
    int grid_h, grid_w = 0;
    int stride = 0;
    // const int &model_in_w = _width;
    const int& model_in_h = _height;
    int dfl_len = output_tensors[0].shape[1] / 4;
    int output_per_branch = output_tensors.size() / 3;

    for (int i = 0; i < 3; i++) {
        void* score_sum = nullptr;
        int32_t score_sum_zp = 0;
        float score_sum_scale = 1.0;
        if (output_per_branch == 3) {
            score_sum = output_tensors[i * output_per_branch + 2].data;
            score_sum_zp = output_tensors[i * output_per_branch + 2].qnt_info.zero_point;
            score_sum_scale = output_tensors[i * output_per_branch + 2].qnt_info.scale;
        }

        int box_idx = i * output_per_branch;
        int score_idx = i * output_per_branch + 1;
        grid_h = output_tensors[box_idx].shape[2];
        grid_w = output_tensors[box_idx].shape[3];
        stride = model_in_h / grid_h;
        switch (output_tensors[box_idx].type) {
            case FFTensorDataType::INT8:
                valid_count += processI8(static_cast<int8_t*>(output_tensors[box_idx].data), output_tensors[box_idx].qnt_info.zero_point, output_tensors[box_idx].qnt_info.scale,
                                         static_cast<int8_t*>(output_tensors[score_idx].data), output_tensors[score_idx].qnt_info.zero_point, output_tensors[score_idx].qnt_info.scale,
                                         static_cast<int8_t*>(score_sum), score_sum_zp, score_sum_scale,
                                         grid_h, grid_w, stride, dfl_len,
                                         filter_boxes, obj_probs, class_id, _conf_threshold);
                break;
            case FFTensorDataType::FLOAT32:
                valid_count += processFp32(static_cast<float*>(output_tensors[box_idx].data),
                                           static_cast<float*>(output_tensors[score_idx].data),
                                           static_cast<float*>(score_sum),
                                           grid_h, grid_w, stride, dfl_len,
                                           filter_boxes, obj_probs, class_id, _conf_threshold);
                break;
            default:
                ff_error_m("Unsupported tensor data type: %d\n", output_tensors[box_idx].type);
                return;
        }
    }

    if (valid_count <= 0)
        return;

    std::vector<int> index_array;
    for (int i = 0; i < valid_count; ++i) {
        index_array.push_back(i);
    }
    quick_sort_indice_inverse(obj_probs, 0, valid_count - 1, index_array);

    std::set<int> class_set(std::begin(class_id), std::end(class_id));
    for (auto c : class_set) {
        nms(valid_count, filter_boxes, class_id, index_array, c, _nms_threshold);
    }

    for (int i = 0; i < valid_count; i++) {
        if (index_array[i] == -1)
            continue;

        int n = index_array[i];
        FFTarget target(static_cast<int>(clamp(filter_boxes[n * 4 + 0], 0, _width) / _scale),
                        static_cast<int>(clamp(filter_boxes[n * 4 + 1], 0, _height) / _scale),
                        static_cast<int>(clamp(filter_boxes[n * 4 + 2], 0, _width) / _scale),
                        static_cast<int>(clamp(filter_boxes[n * 4 + 3], 0, _height) / _scale),
                        class_id[n], obj_probs[i], _labels.at(class_id[n]));

        infer_buffer->targets.push_back(target);
    }

    return;
}


int ModuleInferRKNN2Yolov8::processI8(int8_t* box_tensor, int32_t box_zp, float box_scale,
                                      int8_t* score_tensor, int32_t score_zp, float score_scale,
                                      int8_t* score_sum_tensor, int32_t score_sum_zp, float score_sum_scale,
                                      int grid_h, int grid_w, int stride, int dfl_len,
                                      std::vector<float>& boxes,
                                      std::vector<float>& objProbs,
                                      std::vector<int>& classId,
                                      float threshold)
{
    int validCount = 0;
    int grid_len = grid_h * grid_w;
    int obj_class_num = _labels.size();
    int8_t score_thres_i8 = qnt_f32_to_affine(threshold, score_zp, score_scale);
    int8_t score_sum_thres_i8 = qnt_f32_to_affine(threshold, score_sum_zp, score_sum_scale);

    for (int i = 0; i < grid_h; i++) {
        for (int j = 0; j < grid_w; j++) {
            int offset = i * grid_w + j;
            int max_class_id = -1;

            // 通过 score sum 起到快速过滤的作用
            if (score_sum_tensor != nullptr) {
                if (score_sum_tensor[offset] < score_sum_thres_i8) {
                    continue;
                }
            }

            int8_t max_score = -score_zp;
            for (int c = 0; c < obj_class_num; c++) {
                if ((score_tensor[offset] > score_thres_i8) && (score_tensor[offset] > max_score)) {
                    max_score = score_tensor[offset];
                    max_class_id = c;
                }
                offset += grid_len;
            }

            // compute box
            if (max_score > score_thres_i8) {
                offset = i * grid_w + j;
                float box[4];
                float before_dfl[dfl_len * 4];
                for (int k = 0; k < dfl_len * 4; k++) {
                    before_dfl[k] = deqnt_affine_to_f32(box_tensor[offset], box_zp, box_scale);
                    offset += grid_len;
                }
                compute_dfl(before_dfl, dfl_len, box);

                float x1, y1, x2, y2, w, h;
                x1 = (-box[0] + j + 0.5) * stride;
                y1 = (-box[1] + i + 0.5) * stride;
                x2 = (box[2] + j + 0.5) * stride;
                y2 = (box[3] + i + 0.5) * stride;
                w = x2 - x1;
                h = y2 - y1;
                boxes.push_back(x1);
                boxes.push_back(y1);
                boxes.push_back(w);
                boxes.push_back(h);

                objProbs.push_back(deqnt_affine_to_f32(max_score, score_zp, score_scale));
                classId.push_back(max_class_id);
                validCount++;
            }
        }
    }
    return validCount;
}

int ModuleInferRKNN2Yolov8::processFp32(float* box_tensor, float* score_tensor, float* score_sum_tensor,
                                        int grid_h, int grid_w, int stride, int dfl_len,
                                        std::vector<float>& boxes,
                                        std::vector<float>& objProbs,
                                        std::vector<int>& classId,
                                        float threshold)
{
    int validCount = 0;
    int obj_class_num = _labels.size();
    int grid_len = grid_h * grid_w;
    for (int i = 0; i < grid_h; i++) {
        for (int j = 0; j < grid_w; j++) {
            int offset = i * grid_w + j;
            int max_class_id = -1;

            // 通过 score sum 起到快速过滤的作用
            if (score_sum_tensor != nullptr) {
                if (score_sum_tensor[offset] < threshold) {
                    continue;
                }
            }

            float max_score = 0;
            for (int c = 0; c < obj_class_num; c++) {
                if ((score_tensor[offset] > threshold) && (score_tensor[offset] > max_score)) {
                    max_score = score_tensor[offset];
                    max_class_id = c;
                }
                offset += grid_len;
            }

            // compute box
            if (max_score > threshold) {
                offset = i * grid_w + j;
                float box[4];
                float before_dfl[dfl_len * 4];
                for (int k = 0; k < dfl_len * 4; k++) {
                    before_dfl[k] = box_tensor[offset];
                    offset += grid_len;
                }
                compute_dfl(before_dfl, dfl_len, box);

                float x1, y1, x2, y2, w, h;
                x1 = (-box[0] + j + 0.5) * stride;
                y1 = (-box[1] + i + 0.5) * stride;
                x2 = (box[2] + j + 0.5) * stride;
                y2 = (box[3] + i + 0.5) * stride;
                w = x2 - x1;
                h = y2 - y1;
                boxes.push_back(x1);
                boxes.push_back(y1);
                boxes.push_back(w);
                boxes.push_back(h);

                objProbs.push_back(max_score);
                classId.push_back(max_class_id);
                validCount++;
            }
        }
    }
    return validCount;
}

}  // namespace FFMedia
