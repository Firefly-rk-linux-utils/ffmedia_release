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
    shared_ptr<ModuleRga> rga;

private:
    static shared_ptr<DrmDisplayDevice> display_device;
    shared_ptr<DrmDisplayPlane> plane_device;
    static std::mutex device_mtx;
    uint32_t window_x;
    uint32_t window_y;
    uint32_t window_w;
    uint32_t window_h;
    int index_in_plane;
    bool window_setuped;
    bool full_plane;  // It's a window that fills the plane

public:
    ModuleDrmDisplay(const ImagePara& input_para);
    ~ModuleDrmDisplay();

private:
    bool setupPlaneDevice();
    bool setupDisplayDevice();
    bool setupWindow();
    int drmFindPlane();
    int drmCreateFb(shared_ptr<VideoBuffer> buffer);
    bool rectLegalize(uint32_t& x, uint32_t& y, uint32_t& w, uint32_t& h,
                      uint32_t parent_w, uint32_t parent_h);
    bool checkPlaneType(uint64_t plane_drm_type);
    bool isSamePlane(shared_ptr<DrmDisplayPlane> a, shared_ptr<DrmDisplayPlane> b);

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
    void setPlanePara(uint32_t fmt);
    void setPlanePara(uint32_t fmt, uint32_t plane_zpos);
    void setPlanePara(uint32_t fmt, uint32_t plane_id, PLANE_TYPE plane_type, uint32_t plane_zpos);
    void setPlanePara(uint32_t fmt, uint32_t plane_id, PLANE_TYPE plane_type, uint32_t plane_zpos, uint32_t plane_linear);
    bool move(uint32_t x, uint32_t y);
    bool resize(uint32_t w, uint32_t h);
    bool setPlaneRect(uint32_t x, uint32_t y, uint32_t w, uint32_t h);
    bool setWindowRect(uint32_t x, uint32_t y, uint32_t w, uint32_t h);
    void getPlaneSize(uint32_t* w, uint32_t* h);
    void getWindowSize(uint32_t* w, uint32_t* h);
    void getScreenResolution(uint32_t* w, uint32_t* h);

    void setFullScreenPlane();   // set a plane fills the screen
    void setFullScreenWindow();  // set a window fills the plane, not really "screen"

    // replaced by getPlaneSize
    [[deprecated]] void getDisplayPlaneSize(uint32_t* w, uint32_t* h);
    // replaced by setPlaneRect
    [[deprecated]] bool setPlaneSize(uint32_t x, uint32_t y, uint32_t w, uint32_t h);
    // replaced by setWindowRect
    [[deprecated]] bool setWindowSize(uint32_t x, uint32_t y, uint32_t w, uint32_t h);
};

#endif
