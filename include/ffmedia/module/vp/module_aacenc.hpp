/*
 * @Author: dengkx dkx@t-chip.com.cn
 * @Date: 2024-08-27 09:07:55
 * @LastEditors: Kaison Deng dkx@t-chip.com.cn
 * @LastEditTime: 2026-07-01 14:00:06
 * @Description: 音频编码组件。音频编码，支持aac编码。
 * Copyright (c) 2024-present The ffmedia project authors, All Rights Reserved.
 */
#pragma once

#include <memory>

#include "module/base_config.h"
#include "module/module_media.hpp"

#if AUDIO_SUPPORT

#include "base/ff_type.hpp"

namespace FFMedia
{

class FFMEDIA_API ModuleAacEnc : public ModuleMedia
{
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
     * @description: 获取媒体附加数据。此调用应在对象初始化之后调用。
     * @return {shared_ptr<MediaBuffer>} 成功返回含有附加数据及媒体参数的 MediaBuffer，失败返回空指针。
     */
    std::shared_ptr<MediaBuffer> getExtraBuffer();

    /**
     * @description: 设置Aot。
     * @param {int} _aot    aot == 2 : "LC";
     *                             5 : "HE-AAC";
     *                             29 : "HE-AACv2";
     *                             23 : "LD";
     *                             39 : "ELD"。
     * @return {*}
     */
    void setAot(int _aot);
    int getAot();
    void setBitrate(int bitrate);
    int getBitrate();
    void setAfterburner(int _afterburner);
    int getAfterburner();
    void setEldSbr(int _eld_sbr);
    int getEldSbr();
    void setVbr(int _vbr);
    int gerVbr();

protected:
    ConsumeResult doConsume(const MediaBufferContext& input,
                            std::shared_ptr<MediaBuffer>& output_buffer) override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace FFMedia

#endif
