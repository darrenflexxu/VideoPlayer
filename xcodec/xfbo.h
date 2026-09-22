#pragma once

#include "xtools.h"
#include "xshader_yv12.h"
#include "xshader_nv12.h"
#include "xshader_rgba.h"
#include <memory>
#include <vector>

//离屏 FBO 渲染器: 用于合成导出
//管理 OpenGL 离屏上下文、FBO、着色器实例
class XCODEC_API XFBO {
public:
    XFBO() = default;
    ~XFBO();

    //创建指定分辨率的 FBO
    bool Init(int w, int h);
    void Close();

    //开始一帧合成: 绑定 FBO、清屏
    void BeginFrame();

    //绘制 YUV420P / YUVJ420P 视频帧到当前视口
    void DrawYV12(const unsigned char* y, int y_pitch,
                  const unsigned char* u, int u_pitch,
                  const unsigned char* v, int v_pitch,
                  double rotate = 0);

    //绘制 NV12 视频帧到当前视口
    void DrawNV12(const unsigned char* y, int y_pitch,
                  const unsigned char* uv, int uv_pitch);

    //绘制 RGBA 贴图(贴纸/字幕)到当前视口
    void DrawRGBA(const unsigned char* rgba, int pitch, int w, int h, float alpha);

    //设置绘制视口(归一化 0~1, 左上原点), 内部转换为设备像素并翻转 Y
    void SetViewport(double x, double y, double w, double h);

    //结束帧: 解绑 FBO
    void EndFrame();

    //将 FBO 颜色附件读回 CPU (RGBA), 调用者负责分配 out_buf (w*h*4 字节)
    bool ReadPixelsRGBA(unsigned char* out_buf);

    int width() const { return width_; }
    int height() const { return height_; }
    bool valid() const;

private:
    int width_ = 0;
    int height_ = 0;

    struct Impl {
        void* window = nullptr;       //SDL_Window*
        void* glctx = nullptr;        //SDL_GLContext
        GLuint fbo = 0;
        GLuint color_tex = 0;
        int w = 0, h = 0;
    };
    std::unique_ptr<Impl> impl_;

    //着色器(复用单例)
    std::shared_ptr<XShaderYV12> yv12_;
    std::shared_ptr<XShaderNV12> nv12_;
    std::shared_ptr<XShaderRGBA> rgba_;
};