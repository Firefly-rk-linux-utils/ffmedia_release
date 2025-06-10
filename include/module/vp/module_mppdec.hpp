/*
 * @Author: dengkx dkx@t-chip.com.cn
 * @Date: 2024-08-27 09:07:55
 * @LastEditors: dengkx dkx@t-chip.com.cn
 * @LastEditTime: 2025-04-25 14:48:34
 * @Description: 视频解码组件。支持H264、H265及MJPEG解码。
 * Copyright (c) 2024-present The ffmedia project authors, All Rights Reserved.
 */
#ifndef __MODULE__MPPDEC_HPP__
#define __MODULE__MPPDEC_HPP__

#include "module/module_media.hpp"
#include "base/ff_type.hpp"

class MppDecoder;

class ModuleMppDec : public ModuleMedia
{
private:
    shared_ptr<MppDecoder> dec;
    DecodeType decode_type;
    uint32_t need_split;
    int output_timeout;
    int target_timeout;

protected:
    void setAlign(DecodeType decode_type);
    virtual ConsumeResult doConsume(shared_ptr<MediaBuffer>& input_buffer, shared_ptr<MediaBuffer>& output_buffer) override;
    virtual ProduceResult doProduce(shared_ptr<MediaBuffer>& buffer) override;
    virtual int initBuffer() override;
    virtual void bufferReleaseCallBack(shared_ptr<MediaBuffer>& buffer) override;
    void reset() override;

public:
    ModuleMppDec();
    ModuleMppDec(const ImagePara& input_para);
    /**
     * @description: ModuleMppDec 的构造函数。
     * @param {ImagePara&} input_para   输入图像参数信息。
     * @param {DecodeType} type         解码类型。
     * @return {*}
     */
    ModuleMppDec(const ImagePara& input_para, DecodeType type);
    ~ModuleMppDec();

    /**
     * @description: 设置分帧模式，默认不分帧。
     * @param {uint32_t} split  分帧参数，0关闭分帧模式，1开启分帧模式。
     * @return {*}
     */
    void setNeedSplit(uint32_t split);

    /**
     * @description: 设置获取帧超时时间，默认为10ms。
     * @param {int} timeout_ms  超时时间
     * @return {*}
     */
    void setOutputTimeOut(int timeout_ms);

    /**
     * @description: 初始化对象。
     * @return {int} 成功返回 0，失败返回负数。
     */
    int init() override;

    /**
     * @description: 从缓冲池中导出指定索引值缓冲区。
     * @param {uint16_t} index              缓冲池的索引值。
     * @return {shared_ptr<MediaBuffer>}    返回导出的缓冲区。
     */
    virtual shared_ptr<MediaBuffer> exportBufferFromBufferPool(uint16_t index) override;

    /**
     * @description: 归还缓冲区到缓冲池的指定索引。
     * @param {shared_ptr<MediaBuffer>} buffer  归还的缓冲区。
     * @param {uint16_t} index                  缓冲池的索引值。
     * @return {int}                            成功返回0，失败返回负数。
     */
    virtual int importBufferToBufferPool(shared_ptr<MediaBuffer> buffer, uint16_t index) override;
};

#endif
