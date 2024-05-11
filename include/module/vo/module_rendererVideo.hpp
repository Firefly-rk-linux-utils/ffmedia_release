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
    ModuleRendererVideo(string titele);
    ModuleRendererVideo(const ImagePara& para, string titele);
    ~ModuleRendererVideo();
    int init() override;
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
