/*
 * @Author: dengkx dkx@t-chip.com.cn
 * @Date: 2024-08-27 09:07:55
 * @LastEditors: Kaison Deng dkx@t-chip.com.cn
 * @LastEditTime: 2026-07-01 11:39:04
 * @Description: 视频解码组件。支持H264、H265及MJPEG解码。
 * Copyright (c) 2024-present The ffmedia project authors, All Rights Reserved.
 */
#pragma once

#include "module/module_media.hpp"
#include "base/ff_type.hpp"

namespace FFMedia
{
class MppDecoder;

class FFMEDIA_API ModuleMppDec : public ModuleMedia
{
private:
    class Impl;
    std::unique_ptr<Impl> impl_;

    int buildOutputBufferPool(
        const std::shared_ptr<MppDecoder>& decoder,
        const ImagePara& output_para, OutputBufferPool& pool,
        size_t& buffer_size);
    int installDecoderBuffers(
        const std::shared_ptr<MppDecoder>& decoder,
        const OutputBufferList& buffers, bool clear_existing);

protected:
    void initOutputImagePara();
    DecodeType v4l2FmtToDecodeType(uint32_t v4l2_fmt);
    void setAlign(DecodeType decode_type);
    ConsumeResult doConsume(const MediaBufferContext& input,
                            std::shared_ptr<MediaBuffer>& output_buffer) override;
    virtual ProduceResult doProduce(std::shared_ptr<MediaBuffer>& buffer) override;
    virtual int initBuffer() override;
    virtual void bufferReleaseCallBack(const std::shared_ptr<MediaBuffer>& buffer) override;
    virtual bool teardown() override;

public:
    ModuleMppDec(const ImagePara& input_para = ImagePara());
    /**
     * @description: ModuleMppDec 的构造函数。
     * @param {ImagePara&} input_para   输入图像参数信息。
     * @param {DecodeType} type         解码类型。
     * @return {*}
     */
    ModuleMppDec(const ImagePara& input_para, DecodeType type);
    ~ModuleMppDec();

    /**
     * @description: 设置内部分帧处理模式，默认为0。
     * @param {uint32_t} split  分帧参数，0关闭分帧模式，1开启分帧模式。
     * @return {*}
     */
    void setNeedSplit(uint32_t split);

    /**
     * @description: 设置快速解析模式，提升解码的软硬件并行度。默认为1。
     * @param {uint32_t} fast   快速解析参数，0关闭快速解析，1开启快速解析。
     * @return {*}
     */
    void setFastMode(uint32_t fast);

    /**
     * @description: 设置去隔行配置，默认为1。
     * @param {uint32_t} deinterlace    去隔行参数，0关闭去隔行，1开启去隔行。
     * @return {*}
     */
    void setDeinterlace(uint32_t deinterlace);

    /**
     * @description: 设置获取帧超时时间，默认为0。
     * @param {int} timeout_ms  超时时间
     * @return {*}
     */
    void setOutputTimeOut(int timeout_ms);

    /**
     * @description: 设置申请buffer类型。默认为DRM_BUFFER_NONCACHEABLE。
     * @param {BUFFER_TYPE} type    buffer类型。
     * @return {*}
     */
    void setBufferType(VideoBuffer::BUFFER_TYPE type);

    /**
     * @description: 初始化对象。
     * @return {int} 成功返回 0，失败返回负数。
     */
    int init() override;
};

}  // namespace FFMedia
