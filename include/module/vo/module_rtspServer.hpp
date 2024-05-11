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
    ModuleRtspServer(const char* path, int port);
    ModuleRtspServer(const ImagePara& para, const char* path, int port);
    ~ModuleRtspServer();
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
    ModuleRtspServerExtend(shared_ptr<ModuleRtspServer> module, const char* path, int port);
    ~ModuleRtspServerExtend();
    void setAudioParameter(media_codec_t codec);
    int init() override;
};

typedef ModuleRtspServerExtend ModuleRtspServerAudioTrack;

#endif
