/*
 * @Author: dengkx dkx@t-chip.com.cn
 * @Date: 2024-05-22 17:34:39
 * @LastEditors: Kaison Deng dkx@t-chip.com.cn
 * @LastEditTime: 2025-08-20 11:32:33
 * @Description: 输出组件。视频渲染器，使用opengGles接口渲染视频到X11窗口。
 * Copyright (c) 2024-present The ffmedia project authors, All Rights Reserved.
 */
#ifndef __MODULE_RENDERERVIDEO_HPP__
#define __MODULE_RENDERERVIDEO_HPP__

#include "module/module_media.hpp"
#include <GLES3/gl3.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

class ModuleRga;
class Shader;
class Texture;
class Model;
class ModuleRendererVideo : public ModuleMedia
{
public:
    /**
     * @description: ModuleRendererVideo 的构建函数。
     * @param {string} title   创建的X11窗口标题名称。
     * @return {*}
     */
    ModuleRendererVideo(string title);
    ModuleRendererVideo(const ImagePara para, string title);
    ModuleRendererVideo(const ImagePara para = ImagePara());

    ~ModuleRendererVideo();

    /**
     * @description: 设置显示窗口位置及大小。
     * @param {uint32_t} x  显示窗口起点x坐标。
     * @param {uint32_t} y  显示窗口起点y坐标。
     * @param {uint32_t} w  显示窗口宽度。
     * @param {uint32_t} h  显示窗口高度。
     * @return {*}
     */
    int setWindowRect(int x, int y, uint32_t w, uint32_t h);

    /**
     * @description: 设置显示图像位置及大小。
     * @param {int} x       显示图像在窗口的x坐标。
     * @param {int} y       显示图像在窗口的y坐标。
     * @param {uint32_t} w  显示图像宽度。
     * @param {uint32_t} h  显示图像高度。
     * @return {*}
     */
    int setImageRect(int x, int y, uint32_t w, uint32_t h);
    /**
     * @description: 设置窗口可见性。
     * @param {bool} isVisible
     * @return {*}
     */
    int setWindowVisibility(bool isVisible);

    /**
     * @description: 初始化对象。
     * @return {int} 成功返回 0，失败返回负数。
     */
    int init() override;

    /**
     * @description: 改变对象的输出图像分辨率和清除窗口。此调用应在对象停止时使用。
     * @param {int} width   图像宽度。
     * @param {int} height  图像高度。
     * @return {*}
     */
    int changeOutputResolution(int width, int height);

protected:
    virtual ConsumeResult doConsume(shared_ptr<MediaBuffer>& input_buffer, shared_ptr<MediaBuffer>& output_buffer) override;
    virtual bool setup() override;
    virtual bool teardown() override;
    void reset() override;

private:
    EGLBoolean x11WinCreate();
    GLboolean userInterrupt();
    GLboolean esCreateWindow(GLuint flags);
    void esInitialize();
    void resizeViewport(int width, int height);

protected:
    struct Region {
        int x, y;
        uint32_t width, height;
        Region(int x = 0, int y = 0, uint32_t width = 0, uint32_t height = 0)
            : x(x), y(y), width(width), height(height){};
    };

private:
    shared_ptr<ModuleRga> rga;
    Shader* shader;
    Texture *tex1, *tex2;
    Model* quadModel;

    /// Display handle
    EGLNativeDisplayType eglNativeDisplay;
    /// Window handle
    EGLNativeWindowType eglNativeWindow;
    unsigned long x_wmDeleteMessage;
    /// EGL display
    EGLDisplay eglDisplay;
    /// EGL context
    EGLContext eglContext;
    /// EGL surface
    EGLSurface eglSurface;

    Region winRegion, imageRegion;
    bool visibility;

    string title;
};

#endif
