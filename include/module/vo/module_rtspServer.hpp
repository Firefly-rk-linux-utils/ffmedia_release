/*
 * @Author: dengkx dkx@t-chip.com.cn
 * @Date: 2024-08-27 09:07:55
 * @LastEditors: dengkx dkx@t-chip.com.cn
 * @LastEditTime: 2024-09-09 17:28:09
 * @Description: 输出组件。rtsp服务器，支持tcp和udp推流。
 * Copyright (c) 2024-present The ffmedia project authors, All Rights Reserved.
 */
#ifndef __MODULE_RTSPSERVER_HPP__
#define __MODULE_RTSPSERVER_HPP__

#include <mutex>
#include "module/module_media.hpp"

typedef void* rtsp_demo_handle;
typedef void* rtsp_session_handle;

struct RtspServer;

class ModuleRtspServer : public ModuleMedia
{
    friend class ModuleRtspServerExtend;

private:
    static shared_ptr<RtspServer> rtsp_server;
    static std::mutex rtsp_mtx;
    rtsp_session_handle rtsp_session;
    int push_port;
    char push_path[256];

    media_codec_t video_codec;

protected:
    virtual ConsumeResult doConsume(shared_ptr<MediaBuffer> input_buffer, shared_ptr<MediaBuffer> output_buffer) override;
    virtual bool setup() override;

public:
    /**
     * @description: ModuleRtspServer 的构建函数。
     * @param {char*} path  rtsp地址路径。
     * @param {int} port    rtsp端口号。
     * @return {*}
     */
    ModuleRtspServer(const char* path, int port);
    ModuleRtspServer(const ImagePara& para, const char* path, int port);
    ~ModuleRtspServer();

    /**
     * @description: 初始化对象。
     * @return {int} 成功返回 0，失败返回负数。
     */
    int init() override;
};

typedef ModuleRtspServer ModuleRtspServerVideoTrack;

class ModuleRtspServerExtend : public ModuleMedia
{
    shared_ptr<ModuleRtspServer> rtsp_s;
    media_codec_t audio_codec;

protected:
    virtual ConsumeResult doConsume(shared_ptr<MediaBuffer> input_buffer, shared_ptr<MediaBuffer> output_buffer) override;
    virtual bool setup() override;

public:
    /**
     * @description: ModuleRtspServer 的扩展类，可使用ModuleRtspServer来构建它，以实现同时推流视频和音频或推流音频。
     * @param {shared_ptr<ModuleRtspServer>} module     ModuleRtspServer对象。
     * @param {char*} path                              rtsp地址路径。
     * @param {int} port                                rtsp端口号。
     * @return {*}
     */
    ModuleRtspServerExtend(shared_ptr<ModuleRtspServer> module, const char* path, int port);
    ~ModuleRtspServerExtend();

    /**
     * @description: 设置音频参数。此调用应在初始化对象之前使用。
     * @param {media_codec_t} codec     音频数据格式。
     * @return {*}
     */
    void setAudioParameter(media_codec_t codec);

    /**
     * @description: 初始化对象。
     * @return {int} 成功返回 0，失败返回负数。
     */
    int init() override;
};

typedef ModuleRtspServerExtend ModuleRtspServerAudioTrack;

#endif
