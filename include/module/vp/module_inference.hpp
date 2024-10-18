/*
 * @Author: dengkx dkx@t-chip.com.cn
 * @Date: 2024-08-27 09:07:55
 * @LastEditors: dengkx dkx@t-chip.com.cn
 * @LastEditTime: 2024-10-17 16:46:37
 * @Description: 处理组件。模型推理。
 * Copyright (c) 2024-present The ffmedia project authors, All Rights Reserved.
 */
#pragma once
#include "module/module_media.hpp"
#include <rknn_api.h>
class ModuleRga;
class rknnModelInference;

class ModuleInference : public ModuleMedia
{
public:
    ModuleInference();
    /**
     * @description: ModuleInference 的构建函数。
     * @param {ImagePara&} input_para 输入图像参数。
     * @return {*}
     */
    ModuleInference(const ImagePara& input_para);
    ~ModuleInference();

    /**
     * @description: 设置推理帧间隔数。默认为0，每帧图像都推理。
     * @param {uint32_t} frame_count    帧数量。
     * @return {*}
     */
    void setInferenceInterval(uint32_t frame_count);

    /**
     * @description: 设置模型数据或模型路径。此调用应在对象初始化之前或停止期间调用。
     * @param {void*} model         模型数据或路径。
     * @param {size_t} model_size   模型大小。输入模型路径时大小设置0。
     * @return {*}
     */
    int setModelData(void* model, size_t model_size);

    /**
     * @description: 移除加载的模型。此调用应在对象初始化之前或停止期间调用。
     * @return {*}
     */
    int removeModel();

    /**
     * @description: 设置推理图像区域，默认区域为输入图像大小。
     * @param {ImageCrop&} corp     图像区域。
     * @return {*}
     */
    void setInputImageCrop(const ImageCrop& corp);

    /**
     * @description: 初始化对象。
     * @return {int} 成功返回 0，失败返回负数。
     */
    int init() override;

    /**
     * @description: 改变输入图像参数。
     * @param {ImagePara&} image_param  图像参数信息。
     * @return {*}
     */
    void changedInputImagePara(const ImagePara& image_param);

    /**
     * @description: 获取对象使用输入图像区域。
     * @return {ImageCrop}
     */
    ImageCrop getInputImageCrop();

    /**
     * @description: 获取输出的图像区域，对象会根据模型大小等比例将输入图像调整到输出图像中。
     * @return {ImageCrop}
     */
    ImageCrop getOutputImageCrop();

    /**
     * @description: 获取推理结果容器
     * @return {std::vector<rknn_tensor_mem*>}
     */
    std::vector<rknn_tensor_mem*> getOutputMem();
    std::vector<rknn_tensor_attr*> getOutputAttr();

    void setBufferCount(uint16_t buffer_count) { (void)buffer_count; }

    /**
     * @description: 推理接口。可通过该接口进行手动推理。此调用应在对象初始化之后调用。
     * @param {shared_ptr<MediaBuffer>&} input_buffer    输入图像。要推理的图像。
     * @return {int}                                    成功返回0，失败返回负数。
     */
    int inference(shared_ptr<MediaBuffer>& input_buffer);

protected:
    virtual ConsumeResult doConsume(shared_ptr<MediaBuffer> input_buffer, shared_ptr<MediaBuffer> output_buffer) override;
    void reset() override;

private:
    shared_ptr<ModuleRga> rga;
    rknnModelInference* rmi;
    uint32_t interval;
    uint32_t cur_frame_count;
    ImageCrop input_image_crop;
    ImageCrop output_image_crop;
};