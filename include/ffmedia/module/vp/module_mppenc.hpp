/*
 * @Author: dengkx dkx@t-chip.com.cn
 * @Date: 2024-08-27 09:07:55
 * @LastEditors: Kaison Deng dkx@t-chip.com.cn
 * @LastEditTime: 2026-07-01 11:38:53
 * @Description: 视频编码组件。支持H264、H265及MJPEG编码。
 * Copyright (c) 2024-present The ffmedia project authors, All Rights Reserved.
 */

#pragma once

#include "module/module_media.hpp"
#include "base/ff_type.hpp"

namespace FFMedia
{
class VideoBuffer;

class FFMEDIA_API ModuleMppEnc : public ModuleMedia
{
private:
    class Impl;
    std::unique_ptr<Impl> impl_;

    std::shared_ptr<VideoBuffer> encoderExtraData(std::shared_ptr<VideoBuffer>& buffer);

protected:
    ConsumeResult doConsume(const MediaBufferContext& input,
                            std::shared_ptr<MediaBuffer>& output_buffer) override;
    virtual ProduceResult doProduce(std::shared_ptr<MediaBuffer>& buffer) override;
    virtual int initBuffer() override;
    virtual void bufferReleaseCallBack(const std::shared_ptr<MediaBuffer>& buffer) override;
    virtual bool setup() override;
    virtual bool teardown() override;
    void initOutputImagePara();
    void resetEncoderState();
    void clearInputFrames();

public:
    /**
     * @description:  ModuleMppEnc 的构造函数。
     * @param {media_codec_t} type      编码格式类型，支持 H264、H265 和 MJPEG。
     * @param {int} fps                 编码帧率。
     * @param {int} gop                 两个关键帧之间的间隔数。
     * @param {int} bps                 编码的码率。
     * @param {EncodeRcMode} mode       码率控制模式。
     * @param {float} quality_scale     编码质量系数。范围为0.0到1.0。
     * @param {EncodeProfile} profile   编码的h264/265的profile。
     * @return {*}
     */
    ModuleMppEnc(media_codec_t type = MEDIA_CODEC_VIDEO_H265, int fps = 30, int gop = 60, int bps = 2048,
                 EncodeRcMode mode = ENCODE_RC_MODE_CBR, float quality_scale = 0.8f,
                 EncodeProfile profile = ENCODE_PROFILE_HIGH);

    /**
     * @description:  ModuleMppEnc 的构造函数。
     * @param {EncodeType} type         编码格式类型。
     * @param {ImagePara} input_para    输入图像参数。
     * @param {int} fps                 编码帧率。
     * @param {int} gop                 两个关键帧之间的间隔数。
     * @param {int} bps                 编码的码率。
     * @param {EncodeRcMode} mode       码率控制模式。
     * @param {float} quality_scale     编码质量系数。范围为0.0到1.0。
     * @param {EncodeProfile} profile   编码的h264/265的profile。
     * @return {*}
     */
    ModuleMppEnc(EncodeType type, const ImagePara& input_para = ImagePara(), int fps = 30, int gop = 60, int bps = 2048,
                 EncodeRcMode mode = ENCODE_RC_MODE_CBR, float quality_scale = 0.8f,
                 EncodeProfile profile = ENCODE_PROFILE_HIGH);
    ~ModuleMppEnc();
    /**
     * @description: 设置输出数据的时间戳间隔。默认通过fps计算。
     * @param {int64_t} _duration   时间间隔，微秒。为0则使用输入数据的时间戳。
     * @return {*}
     */
    void setDuration(int64_t _duration);
    /**
     * @description: 改变对象的编码参数。此调用应在对象停止时调用。
     * @param {media_codec_t} type      编码格式类型，支持 H264、H265 和 MJPEG。
     * @param {int} fps                 编码帧率。
     * @param {int} gop                 两个关键帧之间的间隔数。
     * @param {int} bps                 编码的码率。
     * @param {EncodeRcMode} mode       码率控制模式。
     * @param {float} quality_scale     编码质量系数。范围为0.0到1.0。
     * @param {EncodeProfile} profile   编码的h264/265的profile。
     * @return {*}
     */
    int changeEncodeParameter(media_codec_t type, int fps = 30, int gop = 60, int bps = 2048,
                              EncodeRcMode mode = ENCODE_RC_MODE_CBR, float quality_scale = 0.8f,
                              EncodeProfile profile = ENCODE_PROFILE_HIGH);

    /**
     * @description: 设置帧内刷新（GDR）。此调用应在对象初始化之前调用。
     * @param {bool} intra_refresh      开机或关闭帧内刷新。
     * @param {int} refresh_mode        刷新模式：0(按行刷新)、1(按列刷新)。
     * @param {int} refresh_num         每次刷新多少MB行或列。
     * @return {*}
     */
    void setIntraRefresh(bool intra_refresh, int refresh_mode, int refresh_num);

    /**
     * @description: 设置获取帧超时时间，默认为0。
     * @param {int} timeout_ms  超时时间；
     * @return {*}
     */
    void setOutputTimeOut(int timeout_ms);
    /**
     * @description: 设置输入缓存池大小，通过缓存输入帧以使用多核编码提高并发性能; 默认为1。
     * @param {int} size     缓存池大小；必须小于生产者输出缓冲区数量。
     * @return {*}
     */
    void setInputCachePoolSize(int size);

    /**
     * @description: 初始化对象。
     * @return {int} 成功返回 0，失败返回负数。
     */
    int init() override;

    /**
     * @description: 获取附加数据。此调用应在对象初始化之后调用。
     * @return {shared_ptr<MediaBuffer>} 成功返回含有附加数据及媒体参数的 MediaBuffer，失败返回空指针。
     */
    std::shared_ptr<MediaBuffer> getExtraBuffer();
};
}  // namespace FFMedia
