/*
 * @Author: dengkx dkx@t-chip.com.cn
 * @Date: 2024-08-27 09:07:55
 * @LastEditors: dengkx dkx@t-chip.com.cn
 * @LastEditTime: 2024-09-19 16:42:54
 * @Description: 音频编码组件。音频编码，支持aac编码。
 * Copyright (c) 2024-present The ffmedia project authors, All Rights Reserved.
 */
#ifndef __MODULE__AACENC_HPP__
#define __MODULE__AACENC_HPP__

#include "module/module_media.hpp"
#include "base/ff_type.hpp"

struct AACENCODER;

class ModuleAacEnc : public ModuleMedia
{
    AACENCODER* enc;
    SampleInfo sample_info;
    int aot;
    int bit_rate;
    int afterburner;
    int eld_sbr;
    int vbr;

public:
    ModuleAacEnc();
    /**
     * @description: ModuleAacEnc 的构造函数。
     * @param {SampleInfo&} sample_info     音频样品信息。支持的参数如下：
     *                                      SampleFormat:
     *                                       SAMPLE_FMT_S16, SAMPLE_FMT_NONE
     *                                      sample_rate：
     *                                       96000, 88200, 64000, 48000, 44100, 32000,
     *                                       24000, 22050, 16000, 12000, 11025, 8000, 0
     *                                      nb_channels：
     *                                       1 ~ 8
     * @return {*}
     */
    ModuleAacEnc(const SampleInfo& sample_info);
    ~ModuleAacEnc();

    /**
     * @description: 改变音频样品信息。此调用应在对象停止时使用。
     * @param {SampleInfo&} sample_info 新的输入音频样品信息。
     * @return {*}
     */
    int changeSampleInfo(const SampleInfo& sample_info);

    /**
     * @description: 初始化对象。
     * @return {int} 成功返回 0，失败返回负数。
     */
    int init() override;

    /**
     * @description: 设置Aot。
     * @param {int} _aot    aot == 2 : "LC";
     *                             5 : "HE-AAC";
     *                             29 : "HE-AACv2";
     *                             23 : "LD";
     *                             39 : "ELD"。
     * @return {*}
     */
    void setAot(int _aot) { aot = _aot; }
    int getAot() { return aot; }
    void setBitrate(int bitrate) { bit_rate = bitrate; }
    int getBitrate() { return bit_rate; }
    void setAfterburner(int _afterburner) { afterburner = _afterburner; }
    int getAfterburner() { return afterburner; }
    void setEldSbr(int _eld_sbr) { eld_sbr = _eld_sbr; }
    int getEldSbr() { return eld_sbr; }
    void setVbr(int _vbr) { vbr = _vbr; }
    int gerVbr() { return vbr; }

protected:
    virtual ConsumeResult doConsume(shared_ptr<MediaBuffer> input_buffer, shared_ptr<MediaBuffer> output_buffer) override;
    virtual bool setup() override;
    virtual bool teardown() override;

private:
    int open();
    void close();
};

#endif
