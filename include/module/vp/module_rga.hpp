#ifndef __MODULE_RGA_HPP__
#define __MODULE_RGA_HPP__

#include "module/module_media.hpp"
#include "base/ff_type.hpp"


class FFRga;

class ModuleRga : public ModuleMedia
{
    friend class ModuleDrmDisplay;

private:
    shared_ptr<FFRga> rga;
    RgaRotate rotate;
    callback_handler blend_callback;
    void_object blend_callback_ctx;

protected:
    virtual ConsumeResult doConsume(shared_ptr<MediaBuffer> input_buffer, shared_ptr<MediaBuffer> output_buffer) override;


public:
    enum RGA_SCHEDULER_CORE {
        SCHEDULER_DEFAULT = 0,
        SCHEDULER_RGA3_CORE0,
        SCHEDULER_RGA3_CORE1,
        SCHEDULER_RGA2_CORE0 = 4,
        SCHEDULER_RGA3_DEFAULT = SCHEDULER_RGA3_CORE0 | SCHEDULER_RGA3_CORE1,
        SCHEDULER_RGA2_DEFAULT = SCHEDULER_RGA2_CORE0
    };
    enum RGA_BLEND_MODE {
        BLEND_DISABLE = 0,
        BLEND_SRC,
        BLEND_DST,
        BLEND_SRC_OVER = 0x105,
        BLEND_DST_OVER = 0x501
    };

public:
    ModuleRga();
    ModuleRga(const ImagePara& input_para, const ImagePara& output_para, RgaRotate rotate);
    ~ModuleRga();
    int init();
    void setSrcPara(uint32_t fmt, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t hstride, uint32_t vstride);
    void setDstPara(uint32_t fmt, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t hstride, uint32_t vstride);
    void setPatPara(uint32_t fmt, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t hstride, uint32_t vstride);
    void setPatBuffer(void* buf, RGA_BLEND_MODE mode);
    void setPatBuffer(int fd, RGA_BLEND_MODE mode);
    void setBlendCallback(void_object_p ctx, callback_handler callback);
    void setRotate(RgaRotate rotate);
    void setRgaSchedulerCore(RGA_SCHEDULER_CORE core);
    shared_ptr<MediaBuffer> newModuleMediaBuffer(VideoBuffer::BUFFER_TYPE buffer_type = VideoBuffer::BUFFER_TYPE::DRM_BUFFER_CACHEABLE);
    shared_ptr<MediaBuffer> exportUseMediaBuffer(shared_ptr<MediaBuffer> match_buffer, shared_ptr<MediaBuffer> input_buffer, int flag);
    int dstFillColor(int color);

    static void alignStride(uint32_t fmt, uint32_t& wstride, uint32_t& hstride);
};

#endif  //__MODULE_RGA_HPP__
