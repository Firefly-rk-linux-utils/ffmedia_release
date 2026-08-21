#include "module_infer_rknn2_yolov8_position.hpp"
#include "utils/float16.hpp"
#include "utils/utils.hpp"
#include <set>

namespace FFMedia
{

ModuleInferRKNN2Yolov8Position::ModuleInferRKNN2Yolov8Position(const std::string& module_name,
                                                               const std::string& model_path,
                                                               const std::string& label_path,
                                                               float conf_threshold,
                                                               float nms_threshold)
    : ModuleInferRKNN2Yolov8(module_name, model_path, label_path),
      _conf_threshold(conf_threshold),
      _nms_threshold(nms_threshold)
{
}

ModuleInferRKNN2Yolov8Position::~ModuleInferRKNN2Yolov8Position()
{
    stop();
}


int ModuleInferRKNN2Yolov8Position::yolov8_pose_nms(int validCount, std::vector<float>& outputLocations, std::vector<int> classIds, std::vector<int>& order,
                                                    int filterId, float threshold)
{
    for (int i = 0; i < validCount; ++i) {
        int n = order[i];
        if (n == -1 || classIds[n] != filterId) {
            continue;
        }
        for (int j = i + 1; j < validCount; ++j) {
            int m = order[j];
            if (m == -1 || classIds[m] != filterId) {
                continue;
            }
            float xmin0 = outputLocations[n * 5 + 0];
            float ymin0 = outputLocations[n * 5 + 1];
            float xmax0 = outputLocations[n * 5 + 0] + outputLocations[n * 5 + 2];
            float ymax0 = outputLocations[n * 5 + 1] + outputLocations[n * 5 + 3];

            float xmin1 = outputLocations[m * 5 + 0];
            float ymin1 = outputLocations[m * 5 + 1];
            float xmax1 = outputLocations[m * 5 + 0] + outputLocations[m * 5 + 2];
            float ymax1 = outputLocations[m * 5 + 1] + outputLocations[m * 5 + 3];

            float iou = CalculateOverlap(xmin0, ymin0, xmax0, ymax0, xmin1, ymin1, xmax1, ymax1);

            if (iou > threshold) {
                order[j] = -1;
            }
        }
    }
    return 0;
}

void ModuleInferRKNN2Yolov8Position::postprocess(const std::vector<FFTensor>& output_tensors, const std::shared_ptr<MediaBuffer>& input_buffer)
{

    auto infer_buffer = dynamic_cast<InferBuffer*>(input_buffer.get());
    if (infer_buffer == nullptr) {
        ff_error_m("Invalid input buffer, not an InferBuffer.");
        return;
    }

    infer_buffer->pose_targets.clear();
    if (_labels.empty()) {
        ff_error_m("Invalid YOLOv8 post labels\n");
        return;
    }

    std::vector<float> filterBoxes;
    std::vector<float> objProbs;
    std::vector<int> classId;
    int validCount = 0;
    int stride = 0;
    int grid_h = 0;
    int grid_w = 0;
    int model_in_w = _width;
    int model_in_h = _height;
    int index = 0;

    for (int i = 0; i < 3; i++) {
        grid_h = output_tensors[i].shape[2];  // app_ctx->output_attrs[box_idx].dims[2];
        grid_w = output_tensors[i].shape[3];
        stride = model_in_h / grid_h;
        if (output_tensors[0].type == FFTensorDataType::INT8) {
            validCount += processI8((int8_t*)output_tensors[i].data, grid_h, grid_w, stride, filterBoxes, objProbs,
                                    classId, _conf_threshold, output_tensors[i].qnt_info.zero_point, output_tensors[i].qnt_info.scale, index);
        } else {
            validCount += processFp32((float*)output_tensors[i].data, grid_h, grid_w, stride, filterBoxes, objProbs,
                                      classId, _conf_threshold, output_tensors[i].qnt_info.zero_point, output_tensors[i].qnt_info.scale, index);
        }
        index += grid_h * grid_w;
    }

    // no object detect
    if (validCount <= 0) {
        ff_info_m("No valid object detected.\n");
        return;
    }
    std::vector<int> indexArray;
    for (int i = 0; i < validCount; ++i) {
        indexArray.push_back(i);
    }
    quick_sort_indice_inverse(objProbs, 0, validCount - 1, indexArray);

    std::set<int> class_set(std::begin(classId), std::end(classId));

    for (auto c : class_set) {
        yolov8_pose_nms(validCount, filterBoxes, classId, indexArray, c, _nms_threshold);
    }

    /* box valid detect target */
    for (int i = 0; i < validCount; ++i) {
        if (indexArray[i] == -1) {
            continue;
        }

        int n = indexArray[i];
        float x1 = filterBoxes[n * 5 + 0];
        float y1 = filterBoxes[n * 5 + 1];
        float w = filterBoxes[n * 5 + 2];
        float h = filterBoxes[n * 5 + 3];
        int keypoints_index = (int)filterBoxes[n * 5 + 4];
        std::vector<FFPoseKeypoint> kpts;
        kpts.reserve(17);
        for (int j = 0; j < 17; ++j) {
            float kpt_x, kpt_y, kpt_score;
            if (output_tensors[0].type == FFTensorDataType::INT8) {
                kpt_x = ((float)((rknpu2::float16*)output_tensors[3].data)[j * 3 * 8400 + 0 * 8400 + keypoints_index]) / output_tensors[3].qnt_info.scale;
                kpt_y = ((float)((rknpu2::float16*)output_tensors[3].data)[j * 3 * 8400 + 1 * 8400 + keypoints_index]) / output_tensors[3].qnt_info.scale;
                kpt_score = (float)((rknpu2::float16*)output_tensors[3].data)[j * 3 * 8400 + 2 * 8400 + keypoints_index];
            } else {
                kpt_x = (((float*)output_tensors[3].data)[j * 3 * 8400 + 0 * 8400 + keypoints_index]) / output_tensors[3].qnt_info.scale;
                kpt_y = (((float*)output_tensors[3].data)[j * 3 * 8400 + 1 * 8400 + keypoints_index]) / output_tensors[3].qnt_info.scale;
                kpt_score = ((float*)output_tensors[3].data)[j * 3 * 8400 + 2 * 8400 + keypoints_index];
            }
            kpt_x = (float)clamp(kpt_x, 0, model_in_w) / _scale;
            kpt_y = (float)clamp(kpt_y, 0, model_in_h) / _scale;
            kpts.push_back({(int)kpt_x, (int)kpt_y, kpt_score});
        }

        int id = classId[n];
        float obj_conf = objProbs[i];
        FFPoseTarget target((int)(clamp(x1, 0, model_in_w) / _scale),
                            (int)(clamp(y1, 0, model_in_h) / _scale),
                            (int)(clamp(w, 0, model_in_w) / _scale),
                            (int)(clamp(h, 0, model_in_h) / _scale),
                            id,
                            obj_conf);
        target.keypoints = std::move(kpts);
        infer_buffer->pose_targets.push_back(std::move(target));
    }

    return;
}


int ModuleInferRKNN2Yolov8Position::processI8(int8_t* input, int grid_h, int grid_w, int stride,
                                              std::vector<float>& boxes, std::vector<float>& boxScores, std::vector<int>& classId, float threshold,
                                              int32_t zp, float scale, int index)
{
    constexpr int input_loc_len = 64;
    // int tensor_len = input_loc_len + _labels.size();
    int validCount = 0;

    int8_t thres_i8 = qnt_f32_to_affine(unsigmoid(threshold), zp, scale);
    for (int h = 0; h < grid_h; h++) {
        for (int w = 0; w < grid_w; w++) {
            for (size_t a = 0; a < _labels.size(); a++) {
                if (input[(input_loc_len + a) * grid_w * grid_h + h * grid_w + w] >= thres_i8) {  //[1,tensor_len,grid_h,grid_w]
                    float box_conf_f32 = sigmoid(deqnt_affine_to_f32(input[(input_loc_len + a) * grid_w * grid_h + h * grid_w + w],
                                                                     zp, scale));
                    float loc[input_loc_len];
                    for (int i = 0; i < input_loc_len; ++i) {
                        loc[i] = deqnt_affine_to_f32(input[i * grid_w * grid_h + h * grid_w + w], zp, scale);
                    }

                    for (int i = 0; i < input_loc_len / 16; ++i) {
                        softmax(&loc[i * 16], 16);
                    }
                    float xywh_[4] = {0, 0, 0, 0};
                    float xywh[4] = {0, 0, 0, 0};
                    for (int dfl = 0; dfl < 16; ++dfl) {
                        xywh_[0] += loc[dfl] * dfl;
                        xywh_[1] += loc[1 * 16 + dfl] * dfl;
                        xywh_[2] += loc[2 * 16 + dfl] * dfl;
                        xywh_[3] += loc[3 * 16 + dfl] * dfl;
                    }
                    xywh_[0] = (w + 0.5) - xywh_[0];
                    xywh_[1] = (h + 0.5) - xywh_[1];
                    xywh_[2] = (w + 0.5) + xywh_[2];
                    xywh_[3] = (h + 0.5) + xywh_[3];
                    xywh[0] = ((xywh_[0] + xywh_[2]) / 2) * stride;
                    xywh[1] = ((xywh_[1] + xywh_[3]) / 2) * stride;
                    xywh[2] = (xywh_[2] - xywh_[0]) * stride;
                    xywh[3] = (xywh_[3] - xywh_[1]) * stride;
                    xywh[0] = xywh[0] - xywh[2] / 2;
                    xywh[1] = xywh[1] - xywh[3] / 2;
                    boxes.push_back(xywh[0]);                          // x
                    boxes.push_back(xywh[1]);                          // y
                    boxes.push_back(xywh[2]);                          // w
                    boxes.push_back(xywh[3]);                          // h
                    boxes.push_back(float(index + (h * grid_w) + w));  // keypoints index
                    boxScores.push_back(box_conf_f32);
                    classId.push_back(a);
                    validCount++;
                }
            }
        }
    }
    return validCount;
}

int ModuleInferRKNN2Yolov8Position::processFp32(float* input, int grid_h, int grid_w, int stride,
                                                std::vector<float>& boxes, std::vector<float>& boxScores, std::vector<int>& classId, float threshold,
                                                int32_t zp, float scale, int index)
{
    constexpr int input_loc_len = 64;
    // int tensor_len = input_loc_len + _labels.size();
    int validCount = 0;
    float thres_fp = unsigmoid(threshold);
    for (int h = 0; h < grid_h; h++) {
        for (int w = 0; w < grid_w; w++) {
            for (size_t a = 0; a < _labels.size(); a++) {
                if (input[(input_loc_len + a) * grid_w * grid_h + h * grid_w + w] >= thres_fp) {  //[1,tensor_len,grid_h,grid_w]
                    float box_conf_f32 = sigmoid(input[(input_loc_len + a) * grid_w * grid_h + h * grid_w + w]);
                    float loc[input_loc_len];
                    for (int i = 0; i < input_loc_len; ++i) {
                        loc[i] = input[i * grid_w * grid_h + h * grid_w + w];
                    }

                    for (int i = 0; i < input_loc_len / 16; ++i) {
                        softmax(&loc[i * 16], 16);
                    }
                    float xywh_[4] = {0, 0, 0, 0};
                    float xywh[4] = {0, 0, 0, 0};
                    for (int dfl = 0; dfl < 16; ++dfl) {
                        xywh_[0] += loc[dfl] * dfl;
                        xywh_[1] += loc[1 * 16 + dfl] * dfl;
                        xywh_[2] += loc[2 * 16 + dfl] * dfl;
                        xywh_[3] += loc[3 * 16 + dfl] * dfl;
                    }
                    xywh_[0] = (w + 0.5) - xywh_[0];
                    xywh_[1] = (h + 0.5) - xywh_[1];
                    xywh_[2] = (w + 0.5) + xywh_[2];
                    xywh_[3] = (h + 0.5) + xywh_[3];
                    xywh[0] = ((xywh_[0] + xywh_[2]) / 2) * stride;
                    xywh[1] = ((xywh_[1] + xywh_[3]) / 2) * stride;
                    xywh[2] = (xywh_[2] - xywh_[0]) * stride;
                    xywh[3] = (xywh_[3] - xywh_[1]) * stride;
                    xywh[0] = xywh[0] - xywh[2] / 2;
                    xywh[1] = xywh[1] - xywh[3] / 2;
                    boxes.push_back(xywh[0]);                          // x
                    boxes.push_back(xywh[1]);                          // y
                    boxes.push_back(xywh[2]);                          // w
                    boxes.push_back(xywh[3]);                          // h
                    boxes.push_back(float(index + (h * grid_w) + w));  // keypoints index
                    boxScores.push_back(box_conf_f32);
                    classId.push_back(a);
                    validCount++;
                }
            }
        }
    }
    return validCount;
}


}  // namespace FFMedia
