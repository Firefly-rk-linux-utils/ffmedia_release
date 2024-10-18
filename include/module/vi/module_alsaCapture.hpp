/*
 * @Author: dengkx dkx@t-chip.com.cn
 * @Date: 2024-08-27 09:07:55
 * @LastEditors: dengkx dkx@t-chip.com.cn
 * @LastEditTime: 2024-09-09 15:58:04
 * @Description: 输入源组件，音频数据通过alsa接口采集。
 * Copyright (c) 2024-present The ffmedia project authors, All Rights Reserved.
 */
#pragma once
#include "module/module_media.hpp"

class AlsaCapture;
class ModuleAlsaCapture : public ModuleMedia
{
public:
    /**
     * @description: ModuleAlsaCapture 的构造函数.
     * @param {string&} dev     Alsa采集声卡设备.
     * @return {*}
     */
    ModuleAlsaCapture(const std::string& dev);

    /**
     * @description: ModuleAlsaCapture 的构造函数.
     * @param {string&} dev                 Alsa采集声卡设备。
     * @param {SampleInfo&} sample_info     指定采集的音频样品信息。
     * @param {AI_LAYOUT_E} layout          音频通道布局样式。
     * @return {*}
     */
    ModuleAlsaCapture(const std::string& dev, const SampleInfo& sample_info, AI_LAYOUT_E layout = AI_LAYOUT_NORMAL);
    ~ModuleAlsaCapture();

    /**
     * @description: 改变Alsa采集声卡设备。此调用应在对象停止时使用。
     * @param {string&} dev                 Alsa采集声卡设备。
     * @param {SampleInfo&} sample_info     指定采集的音频样品信息。
     * @param {AI_LAYOUT_E} layout          音频通道布局样式。
     * @return {int}                        成功返回 0，失败返回负数。
     */
    int changeSource(const std::string& dev, const SampleInfo& sample_info, AI_LAYOUT_E layout = AI_LAYOUT_NORMAL);

    /**
     * @description: 初始化 ModuleAlsaCapture 对象。
     * @return {int} 成功返回 0，失败返回负数。
     */
    int init() override;

protected:
    virtual ProduceResult doProduce(shared_ptr<MediaBuffer> output_buffer) override;
    virtual bool setup() override;
    virtual bool teardown() override;

private:
    AlsaCapture* capture;
    std::string device;
    SampleInfo sampleInfo;
    AI_LAYOUT_E layout;
};