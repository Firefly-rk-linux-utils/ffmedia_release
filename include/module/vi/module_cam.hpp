#ifndef __MODULE_CAM_HPP__
#define __MODULE_CAM_HPP__

#include "module/module_media.hpp"
class v4l2Cam;

class ModuleCam : public ModuleMedia
{
private:
    string dev;
    shared_ptr<v4l2Cam> cam;

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
    int init() override;
};
#endif
