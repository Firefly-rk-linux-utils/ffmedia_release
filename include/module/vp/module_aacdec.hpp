/*
 * @Author: dengkx dkx@t-chip.com.cn
 * @Date: 2024-08-27 09:07:55
 * @LastEditors: dengkx dkx@t-chip.com.cn
 * @LastEditTime: 2024-09-19 16:43:18
 * @Description: 音频解码组件。支持aac格式解码。
 * Copyright (c) 2024-present The ffmedia project authors, All Rights Reserved.
 */
#ifndef __MODULE__AACDEC_HPP__
#define __MODULE__AACDEC_HPP__

#include "module/module_media.hpp"
#include "base/ff_type.hpp"

class AlsaPlayBack;
struct AAC_DECODER_INSTANCE;

class ModuleAacDec : public ModuleMedia
{
    AAC_DECODER_INSTANCE* dec;
    uint8_t* extradata;
    unsigned extradata_size;
    SampleInfo sampleInfo;

public:
    ModuleAacDec();
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
    virtual ConsumeResult doConsume(shared_ptr<MediaBuffer> input_buffer, shared_ptr<MediaBuffer> output_buffer) override;
    virtual bool teardown() override;

private:
    int open();
    void close();
};

#endif
