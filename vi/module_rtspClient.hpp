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

private:
    RTSPClient* rtsp_client;
    int64_t time_msec;
    unsigned stream_type;
    int64_t last_timestamp;
    string url;

    bool open();
    void close();
};

#endif /* ModuleRtspClient_hpp */
