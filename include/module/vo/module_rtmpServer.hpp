#ifndef __MODULE_RTMPSERVER_HPP__
#define __MODULE_RTMPSERVER_HPP__

#include <string>
#include "module/module_media.hpp"


class rtmpServer;
class ModuleRtmpServer : public ModuleMedia
{
private:
    shared_ptr<rtmpServer> rtmp_server;
    int push_port;
    string push_path;

protected:
    virtual ConsumeResult doConsume(shared_ptr<MediaBuffer> input_buffer, shared_ptr<MediaBuffer> output_buffer) override;
    virtual bool setup() override;
    virtual bool teardown() override;

public:
    ModuleRtmpServer(const char* path, int port);
    ModuleRtmpServer(const ImagePara& para, const char* path, int port);
    ~ModuleRtmpServer();
    virtual int init() override;
    void setMaxClientCount(int count);
    int getMaxClientCount();
    int getCurClientCount();
    void setMaxTimeOutCount(int count);
    int getMaxTimeOutCount();
    void setTimeOutSec(int sec, int usec);
};

#endif
