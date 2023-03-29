#ifndef __DRM_DISPLAY_HPP__
#define __DRM_DISPLAY_HPP__

#include <mutex>
#include "module/module_media.hpp"
class ModuleRga;
struct DrmDisplayDevice;

class ModuleDrmDisplay : public ModuleMedia
{
private:
    ModuleRga* rga;
    ModuleMedia* module;

private:
    static DrmDisplayDevice* display_device;
    static std::mutex device_mtx;
    uint32_t window_x;
    uint32_t window_y;
    uint32_t window_w;
    uint32_t window_h;

public:
    ModuleDrmDisplay(ImagePara& input_para);
    ~ModuleDrmDisplay();

private:
    bool createDisplayDevice();
    int getDrmDisplayPara();
    uint32_t drmFindPlane(uint32_t drm_fmt);
    int drmCreateFb(VideoBuffer* buffer);
    bool checkPlaneType(uint64_t plane_drm_type);

protected:
    virtual EnQueueResult doEnQueue(MediaBuffer* input_buffer, MediaBuffer* output_buffer) override;
    virtual bool setup() override;

public:
    enum PLANE_TYPE {
        PLANE_TYPE_OVERLAY,
        PLANE_TYPE_PRIMARY,
        PLANE_TYPE_CURSOR,
        PLANE_TYPE_OVERLAY_OR_PRIMARY
    };

    int init();
    void setPlanePara(uint32_t fmt, uint32_t plane_id, PLANE_TYPE plane_type, uint32_t plane_linear);
    void setPlaneSize(uint32_t x, uint32_t y, uint32_t w, uint32_t h);
    void setWindowSize(uint32_t x, uint32_t y, uint32_t w, uint32_t h);
    void getDisplayPlaneSize(uint32_t* h, uint32_t* v);
};

#endif
