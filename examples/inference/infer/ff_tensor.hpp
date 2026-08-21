#pragma once
#include <vector>

namespace FFMedia
{
enum FFTensorDataType {
    UNKNOWN,
    FLOAT32,
    FLOAT16,
    INT4,
    INT8,
    UINT8,
    INT16,
    UINT16,
    INT32,
    UINT32,
    INT64,
    UINT64,
    BOOL,
};

struct FFTensorQuantInfo {
    float scale;
    int zero_point;
};

struct FFTensor {
    void* data;
    size_t size;
    FFTensorDataType type;
    std::vector<int> shape;
    FFTensorQuantInfo qnt_info;
};

}  // namespace FFMedia
