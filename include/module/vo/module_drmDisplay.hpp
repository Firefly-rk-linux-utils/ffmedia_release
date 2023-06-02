#ifndef __DRM_DISPLAY_HPP__
#define __DRM_DISPLAY_HPP__

#include <mutex>
#include "module/module_media.hpp"
class ModuleRga;
struct DrmDisplayDevice;
struct DrmDisplayPlane;

class ModuleDrmDisplay : public ModuleMedia
{
private:
    ModuleRga* rga;
    ModuleMedia* module;

private:
    static DrmDisplayDevice* display_device;
    DrmDisplayPlane* plane_device;
    static std::mutex device_mtx;
    uint32_t window_x;
    uint32_t window_y;
    uint32_t window_w;
    uint32_t window_h;
    int index_in_plane;

public:
    ModuleDrmDisplay(const ImagePara& input_para);
    ~ModuleDrmDisplay();

private:
    bool setupPlaneDevice();
    bool setupDisplayDevice();
    int drmFindPlane();
    int drmCreateFb(shared_ptr<VideoBuffer> buffer);
    bool checkPlaneType(uint64_t plane_drm_type);
    bool isSamePlane(DrmDisplayPlane* a, DrmDisplayPlane* b);

protected:
    virtual ConsumeResult doConsume(shared_ptr<MediaBuffer> input_buffer, shared_ptr<MediaBuffer> output_buffer) override;
    virtual bool setup() override;

public:
    enum PLANE_TYPE {
        PLANE_TYPE_OVERLAY,
        PLANE_TYPE_PRIMARY,
        PLANE_TYPE_CURSOR,
        PLANE_TYPE_OVERLAY_OR_PRIMARY
    };

    int init();
    int move(uint32_t x, uint32_t y);
    void setPlanePara(uint32_t fmt);
    void setPlanePara(uint32_t fmt, uint32_t plane_zpos);
    void setPlanePara(uint32_t fmt, uint32_t plane_id, PLANE_TYPE plane_type, uint32_t plane_zpos);
    void setPlanePara(uint32_t fmt, uint32_t plane_id, PLANE_TYPE plane_type, uint32_t plane_zpos, uint32_t plane_linear);
    void setPlaneSize(uint32_t x, uint32_t y, uint32_t w, uint32_t h);
    void setWindowSize(uint32_t x, uint32_t y, uint32_t w, uint32_t h);
    void getDisplayPlaneSize(uint32_t* h, uint32_t* v);
};

#endif
