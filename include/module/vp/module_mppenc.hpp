/*
 * @Author: dengkx dkx@t-chip.com.cn
 * @Date: 2024-08-27 09:07:55
 * @LastEditors: dengkx dkx@t-chip.com.cn
 * @LastEditTime: 2024-09-19 16:41:18
 * @Description: 视频编码组件。支持H264、H265及MJPEG编码。
 * Copyright (c) 2024-present The ffmedia project authors, All Rights Reserved.
 */
#ifndef __MODULE_MPPENC_HPP__
#define __MODULE_MPPENC_HPP__

#include "module/module_media.hpp"
#include "base/ff_type.hpp"

class MppEncoder;
class VideoBuffer;

class ModuleMppEnc : public ModuleMedia
{
private:
    EncodeType encode_type;
    shared_ptr<MppEncoder> enc;
    int fps;
    int gop;
    int bps;
    EncodeRcMode mode;
    EncodeQuality quality;
    EncodeProfile profile;
    int64_t cur_pts;
    int64_t duration;
    shared_ptr<VideoBuffer> encoderExtraData(shared_ptr<VideoBuffer> buffer);

protected:
    virtual ConsumeResult doConsume(shared_ptr<MediaBuffer> input_buffer, shared_ptr<MediaBuffer> output_buffer) override;
    virtual ProduceResult doProduce(shared_ptr<MediaBuffer> buffer) override;
    virtual int initBuffer() override;
    virtual void bufferReleaseCallBack(shared_ptr<MediaBuffer> buffer) override;
    virtual bool setup() override;
    void chooseOutputParaFmt();
    void reset() override;

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
     * @description: 初始化对象。
     * @return {int} 成功返回 0，失败返回负数。
     */
    int init() override;
};
#endif
