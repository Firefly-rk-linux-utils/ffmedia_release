#pragma once

#include <algorithm>
#include <math.h>

namespace FFMedia
{

// 几何计算
inline float CalculateOverlap(float xmin0, float ymin0, float xmax0, float ymax0,
                              float xmin1, float ymin1, float xmax1, float ymax1);

// 排序算法
inline int quick_sort_indice_inverse(std::vector<float>& input, int left, int right,
                                     std::vector<int>& indices);

// 数值裁剪/量化工具
inline int clamp(float val, int min, int max);
inline float sigmoid(float x);
inline float unsigmoid(float y);
inline int32_t __clip(float val, float min, float max);
inline int8_t qnt_f32_to_affine(float f32, int32_t zp, float scale);
inline float deqnt_affine_to_f32(int8_t qnt, int32_t zp, float scale);
inline float deqnt_affine_u8_to_f32(uint8_t qnt, int32_t zp, float scale);

// 归一化
inline void softmax(float* input, int size);

// NMS 非极大值抑制（重载）
inline int nms(int validCount, std::vector<float>& outputLocations, std::vector<int>& order,
               float threshold, int width, int height);

// 类别NMS：基于相对坐标 (x, y, w, h) + classId过滤
inline int nms(int validCount, std::vector<float>& outputLocations, std::vector<int> classIds,
               std::vector<int>& order, int filterId, float threshold);

// dfl 计算
inline void compute_dfl(float* before_dfl, int dfl_len, float* box);


// ===============================
// 具体实现
// ===============================

inline float CalculateOverlap(float xmin0, float ymin0, float xmax0, float ymax0, float xmin1, float ymin1, float xmax1,
                              float ymax1)
{
    float w = fmax(0.f, fmin(xmax0, xmax1) - fmax(xmin0, xmin1) + 1.0);
    float h = fmax(0.f, fmin(ymax0, ymax1) - fmax(ymin0, ymin1) + 1.0);
    float i = w * h;
    float u = (xmax0 - xmin0 + 1.0) * (ymax0 - ymin0 + 1.0) + (xmax1 - xmin1 + 1.0) * (ymax1 - ymin1 + 1.0) - i;
    return u <= 0.f ? 0.f : (i / u);
}


inline int quick_sort_indice_inverse(std::vector<float>& input, int left, int right, std::vector<int>& indices)
{
    float key;
    int key_index;
    int low = left;
    int high = right;
    if (left < right) {
        key_index = indices[left];
        key = input[left];
        while (low < high) {
            while (low < high && input[high] <= key) {
                high--;
            }
            input[low] = input[high];
            indices[low] = indices[high];
            while (low < high && input[low] >= key) {
                low++;
            }
            input[high] = input[low];
            indices[high] = indices[low];
        }
        input[low] = key;
        indices[low] = key_index;
        quick_sort_indice_inverse(input, left, low - 1, indices);
        quick_sort_indice_inverse(input, low + 1, right, indices);
    }
    return low;
}


inline int clamp(float val, int min, int max)
{
    return val > min ? (val < max ? val : max) : min;
}

inline float sigmoid(float x)
{
    return 1.0 / (1.0 + expf(-x));
}

inline float unsigmoid(float y)
{
    return -1.0 * logf((1.0 / y) - 1.0);
}

inline int32_t __clip(float val, float min, float max)
{
    float f = val <= min ? min : (val >= max ? max : val);
    return f;
}

inline int8_t qnt_f32_to_affine(float f32, int32_t zp, float scale)
{
    float dst_val = (f32 / scale) + zp;
    int8_t res = (int8_t)__clip(dst_val, -128, 127);
    return res;
}

inline float deqnt_affine_to_f32(int8_t qnt, int32_t zp, float scale)
{
    return ((float)qnt - (float)zp) * scale;
}
inline float deqnt_affine_u8_to_f32(uint8_t qnt, int32_t zp, float scale)
{
    return ((float)qnt - (float)zp) * scale;
}


inline void softmax(float* input, int size)
{
    float max_val = input[0];
    for (int i = 1; i < size; ++i) {
        if (input[i] > max_val) {
            max_val = input[i];
        }
    }

    float sum_exp = 0.0;
    for (int i = 0; i < size; ++i) {
        sum_exp += expf(input[i] - max_val);
    }

    for (int i = 0; i < size; ++i) {
        input[i] = expf(input[i] - max_val) / sum_exp;
    }
}

inline float f16_to_f32(uint16_t h)
{
    const uint32_t sign = (uint32_t)(h & 0x8000u) << 16;
    uint32_t exp = (h >> 10) & 0x1Fu;
    uint32_t mant = h & 0x03FFu;
    uint32_t f;

    if (exp == 0) {
        if (mant == 0) {
            f = sign;
        } else {
            exp = 1;
            while ((mant & 0x0400u) == 0) {
                mant <<= 1;
                exp--;
            }
            mant &= 0x03FFu;
            f = sign | ((exp + 112) << 23) | (mant << 13);
        }
    } else if (exp == 31) {
        f = sign | (0xFFu << 23) | (mant << 13);
    } else {
        f = sign | ((exp + 112) << 23) | (mant << 13);
    }

    union {
        uint32_t u;
        float f;
    } caster;
    caster.u = f;
    return caster.f;
}

inline int nms(int validCount, std::vector<float>& outputLocations, std::vector<int>& order, float threshold, int width, int height)
{
    for (int i = 0; i < validCount; ++i) {
        if (order[i] == -1) {
            continue;
        }
        int n = order[i];
        for (int j = i + 1; j < validCount; ++j) {
            int m = order[j];
            if (m == -1) {
                continue;
            }
            float xmin0 = outputLocations[n * 4 + 0] * width;
            float ymin0 = outputLocations[n * 4 + 1] * height;
            float xmax0 = outputLocations[n * 4 + 2] * width;
            float ymax0 = outputLocations[n * 4 + 3] * height;

            float xmin1 = outputLocations[m * 4 + 0] * width;
            float ymin1 = outputLocations[m * 4 + 1] * height;
            float xmax1 = outputLocations[m * 4 + 2] * width;
            float ymax1 = outputLocations[m * 4 + 3] * height;

            float iou = CalculateOverlap(xmin0, ymin0, xmax0, ymax0, xmin1, ymin1, xmax1, ymax1);

            if (iou > threshold) {
                order[j] = -1;
            }
        }
    }
    return 0;
}


inline int nms(int validCount, std::vector<float>& outputLocations, std::vector<int> classIds, std::vector<int>& order,
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
            float xmin0 = outputLocations[n * 4 + 0];
            float ymin0 = outputLocations[n * 4 + 1];
            float xmax0 = outputLocations[n * 4 + 0] + outputLocations[n * 4 + 2];
            float ymax0 = outputLocations[n * 4 + 1] + outputLocations[n * 4 + 3];

            float xmin1 = outputLocations[m * 4 + 0];
            float ymin1 = outputLocations[m * 4 + 1];
            float xmax1 = outputLocations[m * 4 + 0] + outputLocations[m * 4 + 2];
            float ymax1 = outputLocations[m * 4 + 1] + outputLocations[m * 4 + 3];

            float iou = CalculateOverlap(xmin0, ymin0, xmax0, ymax0, xmin1, ymin1, xmax1, ymax1);

            if (iou > threshold) {
                order[j] = -1;
            }
        }
    }
    return 0;
}


// dfl
inline void compute_dfl(float* tensor, int dfl_len, float* box)
{
    for (int b = 0; b < 4; b++) {
        float max_value = tensor[b * dfl_len];
        for (int i = 1; i < dfl_len; ++i)
            max_value = std::max(max_value, tensor[b * dfl_len + i]);

        float exp_sum = 0;
        float acc_sum = 0;
        for (int i = 0; i < dfl_len; i++) {
            const float value = expf(tensor[b * dfl_len + i] - max_value);
            exp_sum += value;
            acc_sum += value * i;
        }
        box[b] = exp_sum > 0 ? acc_sum / exp_sum : 0;
    }
}

}  // namespace FFMedia
