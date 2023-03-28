#ifndef __MODULE_CAM_HPP__
#define __MODULE_CAM_HPP__

#include "module/module_media.hpp"
class v4l2Cam;

class ModuleCam : public ModuleMedia
{
private:
    char dev[32];
    v4l2Cam* cam;

protected:
    virtual bool setup() override;
    virtual ProcessResult doProcess(MediaBuffer* output_buffer) override;
    virtual void bufferReleaseCallBack(MediaBuffer* buffer) override;

public:
    ModuleCam(const char* dev);
    ~ModuleCam();
    int init();
};
#endif
