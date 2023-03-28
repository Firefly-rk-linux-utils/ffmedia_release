#ifndef __MODULE_RTSPSERVER_HPP__
#define __MODULE_RTSPSERVER_HPP__

#include <mutex>
#include "module/module_media.hpp"

typedef void* rtsp_demo_handle;
typedef void* rtsp_session_handle;

struct RtspServer;

class ModuleRtspServer : public ModuleMedia
{
private:
    static RtspServer* rtsp_server;
    static std::mutex live_mtx;
    rtsp_session_handle rtsp_session;
    int push_port;
    char push_path[256];

protected:
    virtual EnQueueResult doEnQueue(MediaBuffer* input_buffer, MediaBuffer* output_buffer) override;
    virtual bool setup() override;

public:
    ModuleRtspServer(ImagePara& para, const char* path, int port);
    ~ModuleRtspServer();
    int init();
};

#endif
