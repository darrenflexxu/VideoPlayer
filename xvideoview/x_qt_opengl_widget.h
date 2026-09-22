#pragma once

// GLEW 必须先于 Qt/OpenGL 头文件包含, 否则 __gl_h_ 已被定义导致 GLEW 类型缺失
extern "C" {
#include <GL/glew.h>
}

#include <QOpenGLWidget>

#include <memory>
#include <mutex>

#include "xvideo_view.h"
#include "xshader_yv12.h"
#include "xshader_nv12.h"

//////////////////////////////////////
/// 基于 QOpenGLWidget 的视频渲染实现(UI 层)
/// 属于应用程序, 不进入 XCodec, 保证 XCodec 不依赖 Qt
/// 生产者(XPlayer::Update) 推送解码帧 -> 消费者(paintGL) 在 GUI 线程渲染
/// 避免手动管理窗口句柄和 GL 上下文
class XQtOpenGLWidget : public QOpenGLWidget, public XVideoView
{
    Q_OBJECT
public:
    explicit XQtOpenGLWidget(QWidget* parent = nullptr);
    ~XQtOpenGLWidget() override;

    ////////////////////////////////////////////////
    /// 初始化渲染参数 线程安全 可多次调用
    /// 该方法不创建 GL 资源，GL 资源在 initializeGL/paintGL 中创建
    bool Init(int w, int h, Format fmt = RGBA) override;

    //清理所有申请的资源
    void Close() override;

    //处理窗口退出事件
    bool IsExit() override;

    //////////////////////////////////////////////////
    /// 渲染图像
    bool Draw(const unsigned char* data, int linesize = 0) override;
    bool Draw(const unsigned char* y, int y_pitch,
              const unsigned char* u, int u_pitch,
              const unsigned char* v, int v_pitch) override;

    //////////////////////////////////////////////////
    /// 保存一帧(引用复制)并请求 GUI 线程刷新
    bool DrawFrame(AVFrame* frame, AVCodecContext* ctx) override;

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

private:
    std::mutex frame_mtx_;      //保护待渲染帧
    AVFrame* pending_frame_ = nullptr;  //最近一帧(引用计数复制)
    int pix_w_ = 0;             //材质宽高
    int pix_h_ = 0;
    std::shared_ptr<XShaderYV12> yv12_; //YUV420P 着色器
    std::shared_ptr<XShaderNV12> nv12_; //NV12 着色器
    bool gl_ready_ = false;     //GL 上下文是否可用
    long long last_drawn_pts_ = -1; //上一次绘制的pts, 用于跳过重复帧(节拍平滑)
};
