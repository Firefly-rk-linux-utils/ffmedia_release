/*
 * @Author: Kaison Deng dkx@t-chip.com.cn
 * @Date: 2026-07-02 09:04:17
 * @LastEditors: Kaison Deng dkx@t-chip.com.cn
 * @LastEditTime: 2026-08-11 00:00:00
 * @Description: 视频拼接模块， 将多个输入模块的视频帧进行拼接，输出一个视频流
 * Copyright (c) 2026-present The ffmedia project authors, All Rights Reserved.
 */
#pragma once

#include "module/module_media.hpp"

namespace FFMedia
{
class FFMEDIA_API ModuleVideoStack : public ModuleMedia
{
public:
    /**
     * @description: 构造函数
     * @param {string&} module_name     模块名称
     * @param {int} width               视频输出宽度
     * @param {int} height              视频输出高度
     * @param {float} fps               视频输出帧率
     * @return {*}
     */
    ModuleVideoStack(const std::string& module_name = "ModuleVideoStack",
                     int width = 1920, int height = 1080, float fps = 30.0f);
    ~ModuleVideoStack() override;

    /**
     * @description: 设置视频输入通道的拼接参数
     * @param {MediaChannelId} input_id                  输入通道 ID
     * @param {ImageCrop&} stack_params                  模块拼接参数
     *                                                   宽或高为 0 时禁用通道并清除原区域
     * @return {*}
     */
    void setModuleStackParams(MediaChannelId input_id,
                              const ImageCrop& stack_params);

    int init() override;

protected:
    ConsumeResult doConsume(const MediaBufferContext& input,
                            std::shared_ptr<MediaBuffer>& output_buffer) override;
    bool setup() override;
    bool teardown() override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace FFMedia
