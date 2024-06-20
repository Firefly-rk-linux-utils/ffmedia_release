#pragma once
#include "module/module_media.hpp"
#include <rknn_api.h>
class ModuleRga;
class rknnModelInference;

class ModuleInference : public ModuleMedia
{
public:
    ModuleInference();
    ModuleInference(const ImagePara& input_para);
    ~ModuleInference();
    void setInferenceInterval(uint32_t frame_count);
    int setModelData(void* model, size_t model_size);
    int removeModel();
    void setInputImageCrop(const ImageCrop& corp);
    int init() override;

    ImageCrop getInputImageCrop();
    ImageCrop getOutputImageCrop();
    std::vector<rknn_tensor_mem*>* getOutputMemPtr();
    std::vector<rknn_tensor_attr*>* getOutputAttrPtr();
    std::vector<rknn_tensor_mem*>& getOutputMemRef();
    std::vector<rknn_tensor_attr*>& getOutputAttrRef();

    void setBufferCount(uint16_t buffer_count) { (void)buffer_count; }

public:
    virtual ConsumeResult doConsume(shared_ptr<MediaBuffer> input_buffer, shared_ptr<MediaBuffer> output_buffer) override;

protected:
    void reset() override;

private:
    shared_ptr<ModuleRga> rga;
    rknnModelInference* rmi;
    uint32_t interval;
    uint32_t cur_frame_count;
    ImageCrop input_image_crop;
    ImageCrop output_image_crop;
};