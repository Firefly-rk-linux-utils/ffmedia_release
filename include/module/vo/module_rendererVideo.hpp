/*
 * @Author: dengkx dkx@t-chip.com.cn
 * @Date: 2024-05-22 17:34:39
 * @LastEditors: dengkx dkx@t-chip.com.cn
 * @LastEditTime: 2024-09-19 16:43:56
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
private:
    shared_ptr<ModuleRga> rga;
    Shader* shader;
    Texture *tex1, *tex2;
    Model* quadModel;
    shared_ptr<VideoBuffer> buffer;
    void *y, *uv;

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

    string title;

public:
    /**
     * @description: ModuleRendererVideo 的构建函数。
     * @param {string} title   创建的X11窗口标题名称。
     * @return {*}
     */
    ModuleRendererVideo(string title);
    ModuleRendererVideo(const ImagePara& para, string title);
    ~ModuleRendererVideo();

    /**
     * @description: 初始化对象。
     * @return {int} 成功返回 0，失败返回负数。
     */
    int init() override;

    /**
     * @description: 改变对象的输出图像分辨率。此调用应在对象停止时使用。
     * @param {int} width   图像宽度。
     * @param {int} height  图像高度。
     * @return {*}
     */
    int changeOutputResolution(int width, int height);

protected:
    virtual ConsumeResult doConsume(shared_ptr<MediaBuffer> input_buffer, shared_ptr<MediaBuffer> output_buffer) override;
    virtual bool setup() override;
    virtual bool teardown() override;
    void reset() override;

private:
    EGLBoolean x11WinCreate(const char* title);
    GLboolean userInterrupt();
    GLboolean esCreateWindow(const char* title, GLint width, GLint height, GLuint flags);
    void esInitialize();
    void resizeViewport(int width, int height);
};

#endif
