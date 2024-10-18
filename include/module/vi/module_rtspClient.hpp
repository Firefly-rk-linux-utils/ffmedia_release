/*
 * @Author: dengkx dkx@t-chip.com.cn
 * @Date: 2024-08-27 09:07:55
 * @LastEditors: dengkx dkx@t-chip.com.cn
 * @LastEditTime: 2024-09-25 17:53:14
 * @Description: 输入源组件，Rtsp客户端，支持TCP、UDP及多播协议流。
 * Copyright (c) 2024-present The ffmedia project authors, All Rights Reserved.
 */
#ifndef __MODULE_RTSPCLIENT_HPP__
#define __MODULE_RTSPCLIENT_HPP__

#include "module/module_media.hpp"
class RTSPClient;

class ModuleRtspClient : public ModuleMedia
{
public:
    enum SESSION_STATUS {
        SESSION_STATUS_CLOSED,
        SESSION_STATUS_OPENED,
        SESSION_STATUS_PLAYING,
        SESSION_STATUS_PAUSE,
    };

public:
    /**
     * @description: ModuleRtspClient 的构建函数。
     * @param {string} rtsp_url                 rtsp服务器地址。
     * @param {RTSP_STREAM_TYPE} _stream_type   rtsp媒体流协议类型。
     * @param {bool} enable_video               使能接收rtsp视频流。
     * @param {bool} enable_audio               使能接收rtsp音频流。
     * @return {*}
     */
    ModuleRtspClient(string rtsp_url, RTSP_STREAM_TYPE _stream_type = RTSP_STREAM_TYPE_UDP,
                     bool enable_video = true, bool enable_audio = false);
    ~ModuleRtspClient();

    /**
     * @description: 改变rtsp服务器地址。此调用应在对象停止时使用。
     * @param {string} rtsp_url                 新的rtsp服务器地址。
     * @param {RTSP_STREAM_TYPE} _stream_type   rtsp媒体流协议类型。
     * @return {int}                            成功返回0，失败返回负数。
     */
    int changeSource(string rtsp_url, RTSP_STREAM_TYPE _stream_type = RTSP_STREAM_TYPE_UDP);

    /**
     * @description: 初始化 ModuleRtspClient 对象。
     * @return {int} 成功返回0，失败返回负数。
     */
    int init() override;

    /**
     * @description: 获取视频格式。此调用应在对象初始化后使用。
     * @return {*}
     */
    media_codec_t getVideoCodec();

    /**
     * @description: 获取视频附加数据。此调用应在对象初始化后使用。
     * @return {*}
     */
    const uint8_t* videoExtraData();

    /**
     * @description: 获取视频附加数据大小。此调用应在对象初始化后使用。
     * @return {*}
     */
    unsigned videoExtraDataSize();

    /**
     * @description: 获取音频格式。此调用应在对象初始化后使用。
     * @return {*}
     */
    media_codec_t getAudioCodec();

    /**
     * @description: 获取音频样品信息。此调用应在对象初始化后使用。
     * @return {*}
     */
    SampleInfo getAudioSampleInfo();

    /**
     * @description: 获取音频附加数据。此调用应在对象初始化后使用。
     * @return {*}
     */
    const uint8_t* audioExtraData();

    /**
     * @description: 获取音频附加数据大小。此调用应在对象初始化后使用。
     * @return {*}
     */
    unsigned audioExtraDataSize();

    /**
     * @description: 获取音频通道数量。
     * @return {int} 返回音频通道数量。
     */
    int audioChannel();

    /**
     * @description: 获取音频样品速率。
     * @return {int} 返回音频样品速率。
     */
    int audioSampleRate();
    /**
     * @description: 获取视频帧率
     * @return {int} 返回视频帧率
     */
    uint32_t videoFPS();

    /**
     * @description: 设置获取网络数据的超时时间。
     * @param {unsigned} sec    秒。
     * @param {unsigned} usec   微秒。
     * @return {*}
     */
    void setTimeOutSec(unsigned sec, unsigned nsec) { time_msec = sec * 1000 + nsec / 1000; }
    [[deprecated]] void setMaxTimeOutCount(int count) { (void)count; }

    /**
     * @description: 获取流状态。
     * @return {SESSION_STATUS} 返回流状态。
     */
    SESSION_STATUS getSessionStatus();

protected:
    virtual ProduceResult doProduce(shared_ptr<MediaBuffer> output_buffer) override;
    virtual void bufferReleaseCallBack(shared_ptr<MediaBuffer> buffer) override;
    virtual bool setup() override;
    virtual bool teardown() override;
    int fromRtpGetVideoParameter();
    void initMediaInfo();
    static void closeHandlerFunc(void* arg, int err, int result);

private:
    shared_ptr<RTSPClient> rtsp_client;
    int64_t time_msec;
    RTSP_STREAM_TYPE stream_type;
    string url;
    int abnormalStatusFlag;
    bool first_audio_frame;

    media_codec_t videoCodec;
    media_codec_t audioCodec;
    SampleInfo audioSampleInfo;

    bool open();
};

#endif /* ModuleRtspClient_hpp */
