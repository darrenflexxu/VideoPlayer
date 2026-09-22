#include "predefine_header.h"
#include "xfbo.h"
#include <SDL.h>
#include <vector>

#if defined(__APPLE__)
#include <OpenGL/OpenGL.h>
#endif

XFBO::~XFBO() {
    Close();
}

#if defined(__APPLE__)
// macOS 原生 CGL 上下文(核心 4.1)。sdl2-compat(SDL3)不尊重 GL 配置属性,
// 会退化成 2.1 兼容上下文导致 #version 330 shader 无法编译。
static bool AppleCreateGLContext(void** out_ctx) {
    CGLPixelFormatAttribute attrs[] = {
        kCGLPFAOpenGLProfile, (CGLPixelFormatAttribute)kCGLOGLPVersion_GL4_Core,
        kCGLPFAColorSize,     (CGLPixelFormatAttribute)24,
        kCGLPFAAlphaSize,     (CGLPixelFormatAttribute)8,
        kCGLPFAStencilSize,   (CGLPixelFormatAttribute)0,
        kCGLPFADepthSize,     (CGLPixelFormatAttribute)0,
        kCGLPFADoubleBuffer,  (CGLPixelFormatAttribute)0,  //值 0, kCGLPFADoubleBuffer 本身是布尔
        (CGLPixelFormatAttribute)0
    };
    CGLPixelFormatObj pix = nullptr;
    GLint npix = 0;
    if (CGLChoosePixelFormat(attrs, &pix, &npix) != kCGLNoError || !pix) {
        fprintf(stderr, "XFBO: CGLChoosePixelFormat failed\n");
        return false;
    }
    CGLContextObj ctx = nullptr;
    CGLError err = CGLCreateContext(pix, nullptr, &ctx);
    CGLDestroyPixelFormat(pix);
    if (err != kCGLNoError || !ctx) {
        fprintf(stderr, "XFBO: CGLCreateContext failed: %s\n", CGLErrorString(err));
        return false;
    }
    *out_ctx = ctx;
    return CGLSetCurrentContext(ctx) == kCGLNoError;
}
#endif

bool XFBO::Init(int w, int h)
{
    Close();
    impl_ = std::make_unique<Impl>();
    impl_->w = w;
    impl_->h = h;
    width_ = w;
    height_ = h;

#if defined(__APPLE__)
    if (!AppleCreateGLContext(&impl_->glctx))
        return false;
#else
    Close();
    impl_ = std::make_unique<Impl>();
    impl_->w = w;
    impl_->h = h;
    width_ = w;
    height_ = h;

    //创建隐藏 SDL 窗口获取 GL 上下文
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);

    impl_->window = SDL_CreateWindow("XFBO", 0, 0, w, h,
                                     SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    if (!impl_->window) {
        fprintf(stderr, "XFBO: SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }
    impl_->glctx = SDL_GL_CreateContext(static_cast<SDL_Window*>(impl_->window));
    if (!impl_->glctx) {
        fprintf(stderr, "XFBO: SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        return false;
    }
    SDL_GL_MakeCurrent(static_cast<SDL_Window*>(impl_->window), static_cast<SDL_GLContext>(impl_->glctx));
#endif

    //GLEW 初始化
    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        fprintf(stderr, "XFBO: glewInit failed: %s\n", glewGetErrorString(err));
        return false;
    }

    //创建 FBO + 颜色纹理附件
    glGenFramebuffers(1, &impl_->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, impl_->fbo);
    glGenTextures(1, &impl_->color_tex);
    glBindTexture(GL_TEXTURE_2D, impl_->color_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, impl_->color_tex, 0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "XFBO: incomplete FBO: 0x%x\n", status);
        return false;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    //着色器将在 BeginFrame 首次使用时懒加载
    return true;
}

void XFBO::Close()
{
    if (!impl_)
        return;
#if defined(__APPLE__)
    if (impl_->glctx) {
        CGLSetCurrentContext(nullptr);
        yv12_.reset();
        nv12_.reset();
        rgba_.reset();
        if (impl_->fbo) {
            glDeleteFramebuffers(1, &impl_->fbo);
            impl_->fbo = 0;
        }
        if (impl_->color_tex) {
            glDeleteTextures(1, &impl_->color_tex);
            impl_->color_tex = 0;
        }
        CGLDestroyContext((CGLContextObj)impl_->glctx);
    }
#else
    //释放着色器(需在 GL 上下文中)
    if (impl_->glctx) {
        SDL_GL_MakeCurrent(static_cast<SDL_Window*>(impl_->window), static_cast<SDL_GLContext>(impl_->glctx));
        yv12_.reset();
        nv12_.reset();
        rgba_.reset();
        if (impl_->fbo) {
            glDeleteFramebuffers(1, &impl_->fbo);
            impl_->fbo = 0;
        }
        if (impl_->color_tex) {
            glDeleteTextures(1, &impl_->color_tex);
            impl_->color_tex = 0;
        }
        SDL_GL_DeleteContext(static_cast<SDL_GLContext>(impl_->glctx));
        impl_->glctx = nullptr;
    }
    if (impl_->window) {
        SDL_DestroyWindow(static_cast<SDL_Window*>(impl_->window));
        impl_->window = nullptr;
    }
#endif
    impl_.reset();
}

void XFBO::BeginFrame()
{
    if (!impl_ || !impl_->glctx)
        return;
#if defined(__APPLE__)
    CGLSetCurrentContext((CGLContextObj)impl_->glctx);
#else
    SDL_GL_MakeCurrent(static_cast<SDL_Window*>(impl_->window), static_cast<SDL_GLContext>(impl_->glctx));
#endif
    glBindFramebuffer(GL_FRAMEBUFFER, impl_->fbo);
    glViewport(0, 0, width_, height_);
    glClearColor(0.0f, 0.0f, 0.4f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void XFBO::EndFrame()
{
    if (!impl_ || !impl_->glctx)
        return;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void XFBO::SetViewport(double x, double y, double w, double h)
{
    if (!impl_ || !impl_->glctx)
        return;
    int ox = static_cast<int>(x * width_);
    int oy = static_cast<int>(y * height_);
    int ow = static_cast<int>(w * width_);
    int oh = static_cast<int>(h * height_);
    if (ow < 1) ow = 1;
    if (oh < 1) oh = 1;
    //GL 视口左下角为原点, 归一化 y 是左上原点, 需要翻转
    glViewport(ox, height_ - (oy + oh), ow, oh);
}

void XFBO::DrawYV12(const unsigned char* y, int y_pitch,
                    const unsigned char* u, int u_pitch,
                    const unsigned char* v, int v_pitch,
                    double rotate)
{
    if (!yv12_) {
        yv12_ = std::make_shared<XShaderYV12>(width_, height_, rotate);
    } else if (rotate != 0) {
        //旋转变化需重建着色器
        yv12_ = std::make_shared<XShaderYV12>(width_, height_, rotate);
    }
    yv12_->Draw(y, y_pitch, u, u_pitch, v, v_pitch);
}

void XFBO::DrawNV12(const unsigned char* y, int y_pitch,
                    const unsigned char* uv, int uv_pitch)
{
    if (!nv12_)
        nv12_ = std::make_shared<XShaderNV12>(width_, height_);
    nv12_->Draw(y, y_pitch, uv, uv_pitch);
}

void XFBO::DrawRGBA(const unsigned char* rgba, int pitch, int w, int h, float alpha)
{
    if (!rgba_) 
        rgba_ = std::make_shared<XShaderRGBA>(width_, height_);
    XShaderRGBA::EnableBlend(true);
    rgba_->Draw(rgba, pitch, w, h, alpha);
}

bool XFBO::ReadPixelsRGBA(unsigned char* out_buf)
{
    if (!impl_ || !impl_->glctx || !out_buf)
        return false;
#if defined(__APPLE__)
    CGLSetCurrentContext((CGLContextObj)impl_->glctx);
#else
    SDL_GL_MakeCurrent(static_cast<SDL_Window*>(impl_->window), static_cast<SDL_GLContext>(impl_->glctx));
#endif
    glBindFramebuffer(GL_FRAMEBUFFER, impl_->fbo);
    glReadPixels(0, 0, width_, height_, GL_RGBA, GL_UNSIGNED_BYTE, out_buf);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

bool XFBO::valid() const {
    return impl_ && impl_->fbo != 0;
}