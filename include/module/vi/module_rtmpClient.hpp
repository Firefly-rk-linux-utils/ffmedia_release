#ifndef __MODULE_RTMPCLIENT_HPP__
#define __MODULE_RTMPCLIENT_HPP__

#include "module/module_media.hpp"
class rtmpClient;

class ModuleRtmpClient : public ModuleMedia
{
public:
    ModuleRtmpClient(string rtmp_url, ImagePara para = ImagePara(), int _publish = 1);
    ~ModuleRtmpClient();
    int changeSource(string rtmp_url, int _publish = 1);
    int init() override;
    const uint8_t* videoExtraData();
    unsigned videoExtraDataSize();
    const uint8_t* audioExtraData();
    unsigned audioExtraDataSize();

    void setTimeOutSec(int sec, int usec);

protected:
    virtual ConsumeResult doConsume(shared_ptr<MediaBuffer> input_buffer, shared_ptr<MediaBuffer> output_buffer) override;
    virtual ProduceResult doProduce(shared_ptr<MediaBuffer> output_buffer) override;
    virtual bool setup() override;
    virtual bool teardown() override;
    virtual void bufferReleaseCallBack(shared_ptr<MediaBuffer> buffer) override;

private:
    shared_ptr<rtmpClient> rtmp_client;
    string url;
    int publish;
    shared_ptr<MediaBuffer> probe_buffer;
    bool first_audio_frame;
};


#endif