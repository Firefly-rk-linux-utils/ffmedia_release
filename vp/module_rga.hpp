#ifndef __MODULE_RGA_HPP__
#define __MODULE_RGA_HPP__

#include "module/module_media.hpp"
#include "base/ff_type.hpp"

class FFRga;

class ModuleRga : public ModuleMedia
{
    friend class ModuleDrmDisplay;

private:
    FFRga* rga;
    RgaRotate rotate;

protected:
    virtual EnQueueResult doEnQueue(MediaBuffer* input_buffer, MediaBuffer* output_buffer) override;

public:
    ModuleRga();
    ModuleRga(ImagePara& input_para, ImagePara& output_para, RgaRotate rotate);
    ~ModuleRga();
    int init();
    void setSrcPara(uint32_t fmt, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t hstride, uint32_t vstride);
    void setDstPara(uint32_t fmt, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t hstride, uint32_t vstride);
    void setRotate(RgaRotate rotate);
};

#endif  //__MODULE_RGA_HPP__
