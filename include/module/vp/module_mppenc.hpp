/*
 * @Author: dengkx dkx@t-chip.com.cn
 * @Date: 2024-08-27 09:07:55
 * @LastEditors: Kaison Deng dkx@t-chip.com.cn
 * @LastEditTime: 2025-11-19 09:08:05
 * @Description: 视频编码组件。支持H264、H265及MJPEG编码。
 * Copyright (c) 2024-present The ffmedia project authors, All Rights Reserved.
 */
#ifndef __MODULE_MPPENC_HPP__
#define __MODULE_MPPENC_HPP__

#include "module/module_media.hpp"
#include "base/ff_type.hpp"
#include <deque>

class MppEncoder;
class VideoBuffer;

class ModuleMppEnc : public ModuleMedia
{
private:
    EncodeType encode_type;
    shared_ptr<MppEncoder> enc;
    int64_t cur_pts;
    int64_t duration;
    int frame_count;
    std::deque<shared_ptr<VideoBuffer>> input_cache_deque;
    int output_timeout;
    int target_timeout;

    shared_ptr<VideoBuffer> encoderExtraData(shared_ptr<VideoBuffer>& buffer);

protected:
    virtual ConsumeResult doConsume(shared_ptr<MediaBuffer>& input_buffer, shared_ptr<MediaBuffer>& output_buffer) override;
    virtual ProduceResult doProduce(shared_ptr<MediaBuffer>& buffer) override;
    virtual int initBuffer() override;
    virtual void bufferReleaseCallBack(const shared_ptr<MediaBuffer>& buffer) override;
    virtual bool setup() override;
    virtual bool teardown() override;
    void chooseOutputParaFmt();
    void reset() override;
    void clearInputFrames();

public:
    /**
     * @description:  ModuleMppEnc 的构造函数。
     * @param {EncodeType} type         编码格式类型。
     * @param {int} fps                 编码帧率。
     * @param {int} gop                 两个关键帧之间的间隔数。
     * @param {int} bps                 编码的码率。
     * @param {EncodeRcMode} mode       码率控制模式。
     * @param {EncodeQuality} quality   编码质量。
     * @param {EncodeProfile} profile   编码的h264/265的profile。
     * @return {*}
     */
    ModuleMppEnc(EncodeType type, int fps = 30, int gop = 60, int bps = 2048,
                 EncodeRcMode mode = ENCODE_RC_MODE_CBR, EncodeQuality quality = ENCODE_QUALITY_BEST,
                 EncodeProfile profile = ENCODE_PROFILE_HIGH);
    ModuleMppEnc(EncodeType type, const ImagePara& input_para, int fps = 30, int gop = 60, int bps = 2048,
                 EncodeRcMode mode = ENCODE_RC_MODE_CBR, EncodeQuality quality = ENCODE_QUALITY_BEST,
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
     * @param {EncodeType} type         编码格式类型。
     * @param {int} fps                 编码帧率。
     * @param {int} gop                 两个关键帧之间的间隔数。
     * @param {int} bps                 编码的码率。
     * @param {EncodeRcMode} mode       码率控制模式。
     * @param {EncodeQuality} quality   编码质量。
     * @param {EncodeProfile} profile   编码的h264/265的profile。
     * @return {*}
     */
    int changeEncodeParameter(EncodeType type, int fps = 30, int gop = 60, int bps = 2048,
                              EncodeRcMode mode = ENCODE_RC_MODE_CBR, EncodeQuality quality = ENCODE_QUALITY_BEST,
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
     * @description: 获取附加数据。此调用应在对象初始化之后调用。
     * @return {shared_ptr<MediaBuffer>} 成功返回含有附加数据及媒体参数的 MediaBuffer，失败返回空指针。
     */
    shared_ptr<MediaBuffer> getExtraBuffer();
};
#endif
