#pragma once

#include "module_infer.hpp"
#include "module/vp/module_rga.hpp"
#include "ff_tensor.hpp"
#include <rknn_api.h>

namespace FFMedia
{

class ModuleInferRKNN2 : public ModuleInfer
{
public:
    ModuleInferRKNN2(const std::string& module_name, const std::string& model_path, const std::string& label_path);
    ~ModuleInferRKNN2();

    int setCoremask(int core);

protected:
    int initConverter(const ImagePara& input_image_param, const ImagePara& output_image_param);
    void loadLabel();

    virtual int preprocess(const std::shared_ptr<MediaBuffer>& input_buffer, std::shared_ptr<MediaBuffer>& output_buffer);
    virtual int infer(const std::shared_ptr<MediaBuffer>& output_buffer, std::vector<FFTensor>& output_tensors);
    virtual void postprocess(const std::vector<FFTensor>& output_tensors, const std::shared_ptr<MediaBuffer>& input_buffer) = 0;
    virtual int inferCombinations(const std::shared_ptr<MediaBuffer>& input_buffer) override;

    void setOutputTensorDataType(FFTensorDataType data_type);

private:
    virtual int initModel() override;
    void removeModel();
    int setModel(void* model, size_t model_size);
    int initModelBuffer();
    int setModelIntInputMem(size_t size);
    int setModelExtInputMem(int fd, void* addr, size_t size);
    int setModelExtOutputMem(int index, int fd, void* addr, size_t size);
    int setModelIntOutputMem(int index, size_t size);

    std::vector<FFTensor> rknnTensorsToFFTensors();
    FFTensorDataType rknnToFFTensorsDataType(rknn_tensor_type type);
    rknn_tensor_type FFTensorsDataTypeToRknn(FFTensorDataType type);

protected:
    std::string _model_path;
    std::string _label_path;
    int _width = 0;
    int _height = 0;
    int _channel = 0;
    std::vector<std::string> _labels;
    float _scale = 1.0f;
    std::vector<FFTensor> _output_tensors;
    FFTensorDataType _output_data_type;

private:
    std::shared_ptr<ModuleRga> _converter;
    std::shared_ptr<MediaBuffer> _model_input_buffer;

private:
    rknn_context _ctx;
    rknn_tensor_mem* _input_mem;
    rknn_tensor_attr _input_attr;
    std::vector<rknn_tensor_mem*> _output_mems;
    std::vector<rknn_tensor_attr> _output_attrs;
};

}  // namespace FFMedia
