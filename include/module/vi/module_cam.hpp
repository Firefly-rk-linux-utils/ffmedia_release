#ifndef __MODULE_CAM_HPP__
#define __MODULE_CAM_HPP__

#include "module/module_media.hpp"
class v4l2Cam;

class ModuleCam : public ModuleMedia
{
private:
    string dev;
    shared_ptr<v4l2Cam> cam;
    uint32_t time_sec;
    uint32_t time_usec;

protected:
    virtual bool setup() override;
    virtual ProduceResult doProduce(shared_ptr<MediaBuffer> output_buffer) override;
    virtual void bufferReleaseCallBack(shared_ptr<MediaBuffer> buffer) override;

public:
    ModuleCam(string vdev);
    ~ModuleCam();
    int changeSource(string vdev);
    int camIoctlOperation(unsigned long cmd, void* arg);
    int getCamFd();
    void setTimeOutSec(unsigned sec, unsigned usec);
    int init() override;
};
#endif
