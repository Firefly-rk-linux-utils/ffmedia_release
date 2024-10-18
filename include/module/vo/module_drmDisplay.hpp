/*
 * @Author: dengkx dkx@t-chip.com.cn
 * @Date: 2024-08-27 09:07:55
 * @LastEditors: dengkx dkx@t-chip.com.cn
 * @LastEditTime: 2024-09-25 10:01:41
 * @Description: 输出组件。drm显示输出。
 * Copyright (c) 2024-present The ffmedia project authors, All Rights Reserved.
 */
#ifndef __DRM_DISPLAY_HPP__
#define __DRM_DISPLAY_HPP__

#include <mutex>
#include <unordered_set>
#include "module/module_media.hpp"

class ModuleRga;
struct DrmDisplayDevice;
class ModuleDrmDisplay;
class RWLock;

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
        RWLock* lock;
        vector<shared_ptr<VideoBuffer>> buffers;
        vector<int> buffers_fb;
        int free_index;
        int use_index;
        int etc_index;
    };

    friend class ModuleDrmDisplay;

public:
    /**
     * @description: drm显示图层。
     * @param {uint32_t} fmt            图层使用的图像格式
     * @param {int} _screen_index       图层编号，可通过它指定使用图层。
     * @param {uint32_t} plane_zpos     图层相对于其他图层的前后顺序，较高的值表示图层在前面，较低的值表示图层在后面。
     * @return {*}
     */
    DrmDisplayPlane(uint32_t fmt = V4L2_PIX_FMT_NV12, int _screen_index = 0, uint32_t plane_zpos = 0xFF);
    ~DrmDisplayPlane();

    enum LAYOUT_MODE {
        RELATIVE_LAYOUT,
        ABSOLUTE_LAYOUT
    };

    /**
     * @description: 设置图层绑定的显示器，此调用应在对象setup之前使用。
     * @param {uint32_t} conn_id    显示器id。
     * @return {int}                成功返回0。
     */
    int setConnector(uint32_t conn_id);

    /**
     * @description: setup对象。
     * @return {bool} 成功返回true。
     */
    bool setup();

    /**
     * @description: 设置图层在屏幕的显示区域。
     * @param {uint32_t} x  图层起点x坐标。
     * @param {uint32_t} y  图层起点y坐标。
     * @param {uint32_t} w  图层的宽度。
     * @param {uint32_t} h  图层的高度。
     * @return {*}
     */
    bool setRect(uint32_t x, uint32_t y, uint32_t w, uint32_t h);

    /**
     * @description: 获取图层大小。
     * @param {uint32_t*} w 指向uint32_t*的内存地址，图层宽度设置在该指针。
     * @param {uint32_t*} h 指向uint32_t*的内存地址，图层高度设置在该指针。
     * @return {*}
     */
    void getSize(uint32_t* w, uint32_t* h);

    /**
     * @description: 获取屏幕大小。
     * @param {uint32_t*} w 指向uint32_t*的内存地址，屏幕宽度设置在该指针。
     * @param {uint32_t*} h 指向uint32_t*的内存地址，屏幕高度设置在该指针。
     * @return {*}
     */
    void getScreenResolution(uint32_t* w, uint32_t* h);

    /**
     * @description: 设置图层填充满整个屏幕。
     * @return {bool} 成功返回true。
     */
    bool setPlaneFullScreen();  // set a plane fills the screen
    /**
     * @description: 恢复到上一次的图层设置。
     * @return {bool} 成功返回true。
     */
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

    void processBuffer(ModuleDrmDisplay* window, shared_ptr<MediaBuffer> input_buffer);

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
    /**
     * @description: ModuleDrmDisplay 的构建函数。
     * @param {shared_ptr<DrmDisplayPlane>} plane 设置显示使用的图层对象。
     * @return {*}
     */
    ModuleDrmDisplay(shared_ptr<DrmDisplayPlane> plane = nullptr);
    ModuleDrmDisplay(const ImagePara& input_para, shared_ptr<DrmDisplayPlane> plane = nullptr);
    ~ModuleDrmDisplay();


    /**
     * @description: 初始化对象。
     * @return {int} 成功返回 0，失败返回负数。
     */
    int init() override;

    void setPlanePara(uint32_t fmt);
    void setPlanePara(uint32_t fmt, uint32_t plane_zpos);
    void setPlanePara(uint32_t fmt, uint32_t plane_id, PLANE_TYPE plane_type, uint32_t plane_zpos);
    /**
     * @description: 设置显示使用的图层参数。
     * @param {uint32_t} fmt            图层显示的图像格式，如NV12、RGB23等。
     * @param {uint32_t} plane_id       图层id，为0自动选择。
     * @param {PLANE_TYPE} plane_type   图层类型。
     * @param {uint32_t} plane_zpos     图层的zpos。
     * @param {uint32_t} plane_linear
     * @param {uint32_t} conn_id        图层绑定屏幕的id。
     * @return {*}
     */
    void setPlanePara(uint32_t fmt, uint32_t plane_id, PLANE_TYPE plane_type, uint32_t plane_zpos, uint32_t plane_linear, uint32_t conn_id = 0);

    /**
     * @description: 移动图层。
     * @param {uint32_t} x  图层起点x坐标。
     * @param {uint32_t} y  图层起点y坐标。
     * @return {*}
     */
    bool move(uint32_t x, uint32_t y);

    /**
     * @description: 重新调整图层大小。
     * @param {uint32_t} w  图层宽度。
     * @param {uint32_t} h  图层高度。
     * @return {*}
     */
    bool resize(uint32_t w, uint32_t h);

    /**
     * @description: 设置屏幕上的图层位置及图层大小， move和resize合集。
     * @param {uint32_t} x  图层起点x坐标。
     * @param {uint32_t} y  图层起点y坐标。
     * @param {uint32_t} w  图层宽度。
     * @param {uint32_t} h  图层高度。
     * @return {*}
     */
    bool setPlaneRect(uint32_t x, uint32_t y, uint32_t w, uint32_t h);

    /**
     * @description: 设置图层上的显示窗口位置及大小。
     * @param {uint32_t} x  显示窗口起点x坐标。
     * @param {uint32_t} y  显示窗口起点y坐标。
     * @param {uint32_t} w  显示窗口宽度。
     * @param {uint32_t} h  显示窗口高度。
     * @return {*}
     */
    bool setWindowRect(uint32_t x, uint32_t y, uint32_t w, uint32_t h);
    void getPlaneSize(uint32_t* w, uint32_t* h);
    void getWindowSize(uint32_t* w, uint32_t* h);

    /**
     * @description: 获取屏幕大小。
     * @param {uint32_t*} w 指向uint32_t*的内存地址，屏幕宽度设置在该指针。
     * @param {uint32_t*} h 指向uint32_t*的内存地址，屏幕高度设置在该指针。
     * @return {*}
     */
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

    /**
     * @description: 设置图层图像更新窗口。如果不设置，则图层的第一个窗口是他的图像更新窗口，图层图像更新速度将取决于该窗口。
     * @param {bool} isSync
     * @return {*}
     */
    void setWindowSyncPlane(bool isSync);

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
    bool sync_window;

private:
    bool setupWindow();

protected:
    virtual ConsumeResult doConsume(shared_ptr<MediaBuffer> input_buffer, shared_ptr<MediaBuffer> output_buffer) override;
    virtual bool setup() override;
};

#endif
