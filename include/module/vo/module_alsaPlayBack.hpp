/*
 * @Author: dengkx dkx@t-chip.com.cn
 * @Date: 2024-08-27 09:07:55
 * @LastEditors: dengkx dkx@t-chip.com.cn
 * @LastEditTime: 2024-09-09 15:57:57
 * @Description: 输出组件，通过alsa接口播放音频。
 * Copyright (c) 2024-present The ffmedia project authors, All Rights Reserved.
 */
#pragma once
#include "module/module_media.hpp"

class AlsaPlayBack;
class ModuleAlsaPlayBack : public ModuleMedia
{
public:
    /**
     * @description: ModuleAlsaPlayBack 的构建函数。
     * @param {string&} dev     alsa声卡播放设备
     * @return {*}
     */
    ModuleAlsaPlayBack(const std::string& dev);

    /**
     * @description: ModuleAlsaPlayBack 的构建函数。
     * @param {string&} dev                 alsa声卡播放设备。
     * @param {SampleInfo&} sample_info     指定播放的音频样品描述。
     * @param {AI_LAYOUT_E} layout          音频通道布局样式。
     * @return {*}
     */
    ModuleAlsaPlayBack(const std::string& dev, const SampleInfo& sample_info, AI_LAYOUT_E layout = AI_LAYOUT_NORMAL);
    ~ModuleAlsaPlayBack();

    /**
     * @description: 改变Alsa声卡播放设备，此调用应在对象停止时使用。
     * @param {string&} dev                 Alsa声卡播放设备，
     * @param {SampleInfo&} sample_info     指定播放的音频样品描述。
     * @param {AI_LAYOUT_E} layout          音频通道布局样式。
     * @return {int}                        成功返回 0，失败返回负数。
     */
    int changeDevice(const std::string& dev, const SampleInfo& sample_info, AI_LAYOUT_E layout = AI_LAYOUT_NORMAL);

    /**
     * @description: 初始化对象。
     * @return {int} 成功返回 0，失败返回负数。
     */
    int init() override;

protected:
    virtual ConsumeResult doConsume(shared_ptr<MediaBuffer> input_buffer, shared_ptr<MediaBuffer> output_buffer) override;
    virtual bool setup() override;
    virtual bool teardown() override;

private:
    AlsaPlayBack* playBack;
    std::string device;
    SampleInfo sampleInfo;
    AI_LAYOUT_E layout;
};