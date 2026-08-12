/*
 * @Author: dengkx dkx@t-chip.com.cn
 * @Date: 2024-08-27 09:07:55
 * @LastEditors: Kaison Deng dkx@t-chip.com.cn
 * @LastEditTime: 2026-07-01 13:59:34
 * @Description: 音频解码组件。支持aac格式解码。
 * Copyright (c) 2024-present The ffmedia project authors, All Rights Reserved.
 */

#pragma once

#include <memory>

#include "module/base_config.h"
#include "module/module_media.hpp"
#include "base/ff_type.hpp"

#if AUDIO_SUPPORT
namespace FFMedia
{

class FFMEDIA_API ModuleAacDec : public ModuleMedia
{
public:
    ModuleAacDec();
    ModuleAacDec(std::shared_ptr<MediaBuffer> extra_buffer);

    /**
     * @description: ModuleAacDec 的构造函数。以下参数可从aac流中获取，可不用设置。
     * @param {const uint8_t*} _extradata   音频额外数据。
     * @param {unsigned} _extradata_size    音频额外数据大小。
     * @param {int} _sample_rate            音频样品速率。
     * @param {int} _nb_channels            音频通道数量。
     * @return {*}
     */
    ModuleAacDec(const uint8_t* _extradata, unsigned _extradata_size,
                 int _sample_rate, int _nb_channels = -1);
    ~ModuleAacDec();

    /**
     * @description: 改变对象输入的音频样品信息。输入音频改变需要调用该接口，重新初始化音频解码器，此调用应在对象停止时使用。
     * @param {SampleInfo&} sample_info     音频样品信息。
     * @return {int}                        成功返回 0，失败返回负数。
     */
    int changeSampleInfo(const SampleInfo& sample_info);

    /**
     * @description: 初始化对象。
     * @return {int} 成功返回 0，失败返回负数。
     */
    int init() override;

protected:
    ConsumeResult doConsume(const MediaBufferContext& input,
                            std::shared_ptr<MediaBuffer>& output_buffer) override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace FFMedia

#endif  // AUDIO_SUPPORT
