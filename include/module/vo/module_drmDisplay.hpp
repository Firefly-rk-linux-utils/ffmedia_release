#ifndef __DRM_DISPLAY_HPP__
#define __DRM_DISPLAY_HPP__

#include <mutex>
#include <unordered_set>
#include "module/module_media.hpp"
class ModuleRga;
struct DrmDisplayDevice;
class ModuleDrmDisplay;

enum PLANE_TYPE {
    PLANE_TYPE_OVERLAY,
    PLANE_TYPE_PRIMARY,
    PLANE_TYPE_CURSOR,
    PLANE_TYPE_OVERLAY_OR_PRIMARY
};

namespace FFMedia
{
struct Rect {
    uint32_t x;
    uint32_t y;
    uint32_t w;
    uint32_t h;

    bool operator==(const Rect& b)
    {
        return (this->x == b.x)
               && (this->y == b.y)
               && (this->w == b.w)
               && (this->h == b.h);
    };
    Rect()
        : x(0), y(0), w(0), h(0){};
    Rect(uint32_t _x, uint32_t _y, uint32_t _w, uint32_t _h)
        : x(_x), y(_y), w(_w), h(_h){};
    void set(uint32_t _x, uint32_t _y, uint32_t _w, uint32_t _h)
    {
        x = _x;
        y = _y;
        w = _w;
        h = _h;
    };
};
}  // namespace FFMedia

class DrmDisplayPlane : public std::enable_shared_from_this<DrmDisplayPlane>
{
    struct PlaneBuffer {
        std::shared_timed_mutex mtx;
        vector<shared_ptr<VideoBuffer>> buffers;
        vector<int> buffers_fb;
        int free_index;
        int use_index;
        int etc_index;
    };

    friend class ModuleDrmDisplay;

public:
    DrmDisplayPlane(uint32_t fmt = V4L2_PIX_FMT_NV12, int _screen_index = 0, uint32_t plane_zpos = 0xFF);
    ~DrmDisplayPlane();

    enum LAYOUT_MODE {
        RELATIVE_LAYOUT,
        ABSOLUTE_LAYOUT
    };

    int setConnector(uint32_t conn_id);
    bool setup();
    bool setRect(uint32_t x, uint32_t y, uint32_t w, uint32_t h);
    void getSize(uint32_t* w, uint32_t* h);
    void getScreenResolution(uint32_t* w, uint32_t* h);

    bool setPlaneFullScreen();  // set a plane fills the screen
    bool restorePlaneFromFullScreen();

    LAYOUT_MODE getWindowLayoutMode() const { return window_layout_mode; }
    void setWindowLayoutMode(const LAYOUT_MODE& windowLayoutMode) { window_layout_mode = windowLayoutMode; }

    bool splitPlane(uint32_t w_parts, uint32_t h_hparts);
    bool flushAllWindowRectUpdate();

private:
    bool setupDisplayDevice();
    int drmFindPlane();
    int drmCreateFb(shared_ptr<VideoBuffer> buffer);
    bool drmVsync();

    int addWindow(ModuleDrmDisplay* window);
    void removeWindow(ModuleDrmDisplay* window);

    bool checkPlaneType(uint64_t plane_drm_type);
    bool isSamePlane(shared_ptr<DrmDisplayPlane> a, shared_ptr<DrmDisplayPlane> b);

    void processBuffer(shared_ptr<ModuleRga>& win_rga, shared_ptr<MediaBuffer> input_buffer);

private:
    shared_ptr<DrmDisplayDevice> display_device;
    int screen_index;
    int drm_fd;
    uint32_t plane_id;
    uint32_t conn_id;
    PLANE_TYPE type;
    uint32_t linear;
    uint32_t zpos;
    uint32_t v4l2Fmt;
    shared_ptr<ModuleRga> rga;
    PlaneBuffer pBuffer;
    FFMedia::Rect cur_rect;
    FFMedia::Rect last_rect;
    uint32_t w_parts;
    uint32_t h_parts;
    LAYOUT_MODE window_layout_mode;
    bool setuped;
    int index_in_display_device;
    uint32_t windows_count;
    bool size_seted;
    bool full_plane;       // It's a plane that fills the screen
    bool mini_size_plane;  // It's a plane that size is 0
    unordered_set<ModuleDrmDisplay*> windows;
};

class ModuleDrmDisplay : public ModuleMedia
{
    friend class DrmDisplayPlane;

public:
    ModuleDrmDisplay(shared_ptr<DrmDisplayPlane> plane = nullptr);
    ModuleDrmDisplay(const ImagePara& input_para, shared_ptr<DrmDisplayPlane> plane = nullptr);
    ~ModuleDrmDisplay();

    int init() override;
    void setPlanePara(uint32_t fmt);
    void setPlanePara(uint32_t fmt, uint32_t plane_zpos);
    void setPlanePara(uint32_t fmt, uint32_t plane_id, PLANE_TYPE plane_type, uint32_t plane_zpos);
    void setPlanePara(uint32_t fmt, uint32_t plane_id, PLANE_TYPE plane_type, uint32_t plane_zpos, uint32_t plane_linear, uint32_t conn_id = 0);
    bool move(uint32_t x, uint32_t y);
    bool resize(uint32_t w, uint32_t h);
    bool setPlaneRect(uint32_t x, uint32_t y, uint32_t w, uint32_t h);
    bool setWindowRect(uint32_t x, uint32_t y, uint32_t w, uint32_t h);
    void getPlaneSize(uint32_t* w, uint32_t* h);
    void getWindowSize(uint32_t* w, uint32_t* h);
    void getScreenResolution(uint32_t* w, uint32_t* h);

    bool setWindowVisibility(bool isVisible);

    bool setWindowFullScreen();  // set a window fills the screen
    bool restoreWindowFromFullScreen();

    bool setWindowFullPlane();  // set a window fills the plane
    bool restoreWindowFromFullPlane();

    // sync true:  do Rect update sync.
    // sync false: only store rect's change, don't take effect until run flushRelativeUpdate
    bool setWindowRelativeRect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, bool sync = true);
    bool flushRelativeUpdate();

    // replaced by getPlaneSize
    [[deprecated]] void getDisplayPlaneSize(uint32_t* w, uint32_t* h);
    // replaced by setPlaneRect
    [[deprecated]] bool setPlaneSize(uint32_t x, uint32_t y, uint32_t w, uint32_t h);
    // replaced by setWindowRect
    [[deprecated]] bool setWindowSize(uint32_t x, uint32_t y, uint32_t w, uint32_t h);

private:
    shared_ptr<ModuleRga> rga;

private:
    shared_ptr<DrmDisplayPlane> plane_device;
    FFMedia::Rect absolute_rect;  // absolute rect
    FFMedia::Rect last_absolute_rect;
    FFMedia::Rect relative_rect;  // relative rect
    FFMedia::Rect last_relative_rect;
    int index_in_plane;
    bool window_setuped;
    bool size_seted;
    bool full_window;  // It's a window that fills the plane
    bool visibility;
    bool mini_size_window;
    int zpos;

private:
    bool setupWindow();

protected:
    virtual ConsumeResult doConsume(shared_ptr<MediaBuffer> input_buffer, shared_ptr<MediaBuffer> output_buffer) override;
    virtual bool setup() override;
};

#endif
