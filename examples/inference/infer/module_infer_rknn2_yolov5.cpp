#include "module_infer_rknn2_yolov5.hpp"
#include "utils/utils.hpp"
#include <set>

namespace FFMedia
{

ModuleInferRKNN2Yolov5::ModuleInferRKNN2Yolov5(const std::string& module_name,
                                               const std::string& model_path,
                                               const std::string& label_path,
                                               float conf_threshold,
                                               float nms_threshold)
    : ModuleInferRKNN2(module_name, model_path, label_path),
      _conf_threshold(conf_threshold),
      _nms_threshold(nms_threshold)
{
}

ModuleInferRKNN2Yolov5::~ModuleInferRKNN2Yolov5()
{
    stop();
}

void ModuleInferRKNN2Yolov5::postprocess(const std::vector<FFTensor>& output_tensors, const std::shared_ptr<MediaBuffer>& input_buffer)
{
    auto infer_buffer = dynamic_cast<InferBuffer*>(input_buffer.get());
    if (infer_buffer == nullptr) {
        ff_error_m("Invalid input buffer, not an InferBuffer.");
        return;
    }

    infer_buffer->targets.clear();
    if (_labels.empty()) {
        ff_error_m("Invalid YOLOv5 labels\n");
        return;
    }

    std::vector<float> filter_boxes;
    std::vector<float> obj_probs;
    std::vector<int> class_id;
    int valid_count = 0;
    int stride, grid_h, grid_w = 0;
    for (size_t i = 0; i < output_tensors.size(); i++) {
        grid_h = output_tensors[i].shape[2];
        grid_w = output_tensors[i].shape[3];
        stride = _width / grid_h;
        switch (output_tensors[i].type) {
            case FFTensorDataType::INT8:
                valid_count += processI8(static_cast<int8_t*>(output_tensors[i].data),
                                         _anchors[i],
                                         grid_h, grid_w, stride,
                                         filter_boxes, obj_probs, class_id,
                                         _conf_threshold,
                                         output_tensors[i].qnt_info.zero_point,
                                         output_tensors[i].qnt_info.scale);
                break;
            case FFTensorDataType::FLOAT32:
                valid_count += processFp32(static_cast<float*>(output_tensors[i].data),
                                           _anchors[i],
                                           grid_h, grid_w, stride, filter_boxes,
                                           obj_probs, class_id, _conf_threshold);
                break;
            default:
                ff_error_m("Unsupported tensor data type: %d\n", output_tensors[i].type);
                return;
        }
    }

    if (valid_count <= 0)
        return;

    std::vector<int> index_array;
    for (int i = 0; i < valid_count; i++)
        index_array.push_back(i);

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


int ModuleInferRKNN2Yolov5::processI8(int8_t* input, const int* anchor, int grid_h,
                                      int grid_w, int stride,
                                      std::vector<float>& boxes,
                                      std::vector<float>& obj_probs,
                                      std::vector<int>& class_id,
                                      float threshold, int32_t zp, float scale)
{
    int valid_count = 0;
    int grid_len = grid_h * grid_w;
    int8_t thres_i8 = qnt_f32_to_affine(threshold, zp, scale);
    int obj_class_num = _labels.size();
    int prop_box_size = obj_class_num + 5;
    for (int a = 0; a < 3; a++) {
        for (int i = 0; i < grid_h; i++) {
            for (int j = 0; j < grid_w; j++) {
                int8_t box_confidence = input[(prop_box_size * a + 4) * grid_len + i * grid_w + j];
                if (box_confidence >= thres_i8) {
                    int offset = (prop_box_size * a) * grid_len + i * grid_w + j;
                    int8_t* in_ptr = input + offset;
                    float box_x = (deqnt_affine_to_f32(*in_ptr, zp, scale)) * 2.0 - 0.5;
                    float box_y = (deqnt_affine_to_f32(in_ptr[grid_len], zp, scale)) * 2.0 - 0.5;
                    float box_w = (deqnt_affine_to_f32(in_ptr[2 * grid_len], zp, scale)) * 2.0;
                    float box_h = (deqnt_affine_to_f32(in_ptr[3 * grid_len], zp, scale)) * 2.0;
                    box_x = (box_x + j) * (float)stride;
                    box_y = (box_y + i) * (float)stride;
                    box_w = box_w * box_w * (float)anchor[a * 2];
                    box_h = box_h * box_h * (float)anchor[a * 2 + 1];
                    box_x -= (box_w / 2.0);
                    box_y -= (box_h / 2.0);

                    int8_t maxClassProbs = in_ptr[5 * grid_len];
                    int maxClassId = 0;

                    for (int k = 1; k < obj_class_num; ++k) {
                        int8_t prob = in_ptr[(5 + k) * grid_len];
                        if (prob > maxClassProbs) {
                            maxClassId = k;
                            maxClassProbs = prob;
                        }
                    }
                    if (maxClassProbs > thres_i8) {
                        obj_probs.push_back((deqnt_affine_to_f32(maxClassProbs, zp, scale)) * (deqnt_affine_to_f32(box_confidence, zp, scale)));
                        class_id.push_back(maxClassId);
                        valid_count++;
                        boxes.push_back(box_x);
                        boxes.push_back(box_y);
                        boxes.push_back(box_w);
                        boxes.push_back(box_h);
                    }
                }
            }
        }
    }
    return valid_count;
}

int ModuleInferRKNN2Yolov5::processFp32(float* input, const int* anchor, int grid_h,
                                        int grid_w, int stride,
                                        std::vector<float>& boxes,
                                        std::vector<float>& obj_probs,
                                        std::vector<int>& class_id,
                                        float threshold)
{
    int valid_count = 0;
    int grid_len = grid_h * grid_w;
    int obj_class_num = _labels.size();
    int prop_box_size = obj_class_num + 5;
    for (int a = 0; a < 3; a++) {
        for (int i = 0; i < grid_h; i++) {
            for (int j = 0; j < grid_w; j++) {
                float box_confidence = input[(prop_box_size * a + 4) * grid_len + i * grid_w + j];
                if (box_confidence >= threshold) {
                    int offset = (prop_box_size * a) * grid_len + i * grid_w + j;
                    float* in_ptr = input + offset;
                    float box_x = *in_ptr * 2.0 - 0.5;
                    float box_y = in_ptr[grid_len] * 2.0 - 0.5;
                    float box_w = in_ptr[2 * grid_len] * 2.0;
                    float box_h = in_ptr[3 * grid_len] * 2.0;
                    box_x = (box_x + j) * (float)stride;
                    box_y = (box_y + i) * (float)stride;
                    box_w = box_w * box_w * (float)anchor[a * 2];
                    box_h = box_h * box_h * (float)anchor[a * 2 + 1];
                    box_x -= (box_w / 2.0);
                    box_y -= (box_h / 2.0);

                    float maxClassProbs = in_ptr[5 * grid_len];
                    int maxClassId = 0;
                    for (int k = 1; k < obj_class_num; ++k) {
                        float prob = in_ptr[(5 + k) * grid_len];
                        if (prob > maxClassProbs) {
                            maxClassId = k;
                            maxClassProbs = prob;
                        }
                    }
                    if (maxClassProbs > threshold) {
                        obj_probs.push_back(maxClassProbs * box_confidence);
                        class_id.push_back(maxClassId);
                        valid_count++;
                        boxes.push_back(box_x);
                        boxes.push_back(box_y);
                        boxes.push_back(box_w);
                        boxes.push_back(box_h);
                    }
                }
            }
        }
    }
    return valid_count;
}


}  // namespace FFMedia
