#ifndef __MODULE_RTSPCLIENT_HPP__
#define __MODULE_RTSPCLIENT_HPP__

#include "module/module_media.hpp"
class RTSPClient;

class ModuleRtspClient : public ModuleMedia
{
public:
    ModuleRtspClient(const char* url, int _stream_type = 0);
    ~ModuleRtspClient();
    int init();
    const uint8_t* videoExtraData();
    unsigned videoExtraDataSize();
    const uint8_t* audioExtraData();
    unsigned audioExtraDataSize();
    int audioChannel();
    int audioSampleRate();
    uint32_t videoFPS();
    void setTimeOutSec(unsigned sec, unsigned nsec) { time_msec = sec * 1000 + nsec / 1000; }

protected:
    virtual ProcessResult doProcess(MediaBuffer* output_buffer) override;
    virtual bool setup() override;
    virtual bool teardown() override;

private:
    RTSPClient* rtsp_client;
    int64_t time_msec;
    unsigned stream_type;
    string url;
    const int SESSION_STATUS_OPENED = 0;
    const int SESSION_STATUS_CLOSED = 1;
    int session_status;

    bool open();
};

#endif /* ModuleRtspClient_hpp */
