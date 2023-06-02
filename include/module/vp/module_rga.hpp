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
    virtual ConsumeResult doConsume(shared_ptr<MediaBuffer> input_buffer, shared_ptr<MediaBuffer> output_buffer) override;

public:
    ModuleRga();
    ModuleRga(const ImagePara& input_para, const ImagePara& output_para, RgaRotate rotate);
    ~ModuleRga();
    int init();
    void setSrcPara(uint32_t fmt, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t hstride, uint32_t vstride);
    void setDstPara(uint32_t fmt, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t hstride, uint32_t vstride);
    void setRotate(RgaRotate rotate);
    shared_ptr<MediaBuffer> newModuleMediaBuffer(VideoBuffer::BUFFER_TYPE buffer_type = VideoBuffer::BUFFER_TYPE::DRM_BUFFER_CACHEABLE);
    shared_ptr<MediaBuffer> exportUseMediaBuffer(shared_ptr<MediaBuffer> match_buffer, shared_ptr<MediaBuffer> input_buffer, int flag);
};

#endif  //__MODULE_RGA_HPP__
