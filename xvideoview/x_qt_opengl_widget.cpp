#include "x_qt_opengl_widget.h"

#include <cstdio>
#include <GL/glew.h>

extern "C" {
#include <libavutil/frame.h>
}

XQtOpenGLWidget::XQtOpenGLWidget(QWidget* parent) : QOpenGLWidget(parent) {
    //视频区域无需背景，避免与 GL 绘制叠加产生闪烁
    setAttribute(Qt::WA_OpaquePaintEvent);
}

XQtOpenGLWidget::~XQtOpenGLWidget() {
    std::lock_guard<std::mutex> lock(frame_mtx_);
    if (pending_frame_) {
        av_frame_free(&pending_frame_);
        pending_frame_ = nullptr;
    }
}

bool XQtOpenGLWidget::Init(int w, int h, Format fmt) {
    //仅保存渲染参数，GL 资源在 initializeGL/paintGL 中按 Qt 生命周期创建
    pix_w_ = w;
    pix_h_ = h;
    width_ = w;
    height_ = h;
    fmt_ = fmt;
    last_drawn_pts_ = -1;   //新文件/新解码起点, 重置去重计数
    return true;
}

void XQtOpenGLWidget::Close() {
    std::lock_guard<std::mutex> lock(frame_mtx_);
    if (pending_frame_) {
        av_frame_free(&pending_frame_);
        pending_frame_ = nullptr;
    }
}

bool XQtOpenGLWidget::IsExit() {
    return false;
}

bool XQtOpenGLWidget::Draw(const unsigned char* data, int linesize) {
    (void)data;
    (void)linesize;
    return false;
}

bool XQtOpenGLWidget::Draw(const unsigned char* y, int y_pitch,
                           const unsigned char* u, int u_pitch,
                           const unsigned char* v, int v_pitch) {
    (void)y;
    (void)y_pitch;
    (void)u;
    (void)u_pitch;
    (void)v;
    (void)v_pitch;
    return false;
}

bool XQtOpenGLWidget::DrawFrame(AVFrame* frame, AVCodecContext* ctx) {
    (void)ctx;
    if (!frame || !frame->data[0])
        return false;

    //跳过重复帧: 高频拉取时只有新帧才触发重绘, 避免重绘风暴并平滑节拍
    if (frame->pts == last_drawn_pts_)
        return false;
    last_drawn_pts_ = frame->pts;

    //统计显示帧率
    count_++;
    if (beg_ms_ <= 0) {
        beg_ms_ = clock();
    } else if ((clock() - beg_ms_) / (CLOCKS_PER_SEC / 1000) >= 1000) {
        render_fps_ = count_;
        count_ = 0;
        beg_ms_ = clock();
    }

    //引用复制一帧，确保解码线程释放后 GUI 线程仍可安全渲染
    AVFrame* copy = av_frame_alloc();
    if (!copy)
        return false;
    if (av_frame_ref(copy, frame) < 0) {
        av_frame_free(&copy);
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(frame_mtx_);
        if (pending_frame_)
            av_frame_free(&pending_frame_);
        pending_frame_ = copy;
    }

    //请求 GUI 线程重绘
    update();
    return true;
}

void XQtOpenGLWidget::initializeGL() {
    //在 Qt 当前的 GL 上下文中初始化 GLEW
    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    gl_ready_ = (err == GLEW_OK);
    if (!gl_ready_) {
        fprintf(stderr, "XQtOpenGLWidget: glewInit failed: %s\n",
                glewGetErrorString(err));
    }

    //上下文(重新)创建后，旧的着色器资源需重建
    yv12_.reset();
    nv12_.reset();

    glClearColor(0.0f, 0.0f, 0.4f, 1.0f);
    const char* ver = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    fprintf(stderr, "XQtOpenGLWidget: initializeGL OpenGL=%s\n",
            ver ? ver : "unknown");
}

void XQtOpenGLWidget::resizeGL(int w, int h) {
    //视口由 Qt 在 paintGL 前设置，此处仅记录尺寸变化（触发重绘）
    (void)w;
    (void)h;
}

void XQtOpenGLWidget::paintGL() {
    //取最近一帧的引用(保持最近画面，resize/重绘时可重复使用)
    AVFrame* cur = nullptr;
    {
        std::lock_guard<std::mutex> lock(frame_mtx_);
        if (pending_frame_) {
            cur = av_frame_alloc();
            if (!cur || av_frame_ref(cur, pending_frame_) < 0) {
                av_frame_free(&cur);
                cur = nullptr;
            }
        }
    }
    if (!cur)
        return;

    //Qt 已绑定上下文与 FBO，这里按设备像素设置视口
    const int fw = static_cast<int>(width() * devicePixelRatioF());
    const int fh = static_cast<int>(height() * devicePixelRatioF());

    //视频显示尺寸(像素)：旋转 90/270 时宽高互换
    int vw = pix_w_;
    int vh = pix_h_;
    if (cur->format == AV_PIX_FMT_YUV420P ||
        cur->format == AV_PIX_FMT_YUVJ420P) {
        double rotate = XTools::get_rotation_from_frame(cur);
        int r = (int)rotate % 180;
        if (r < 0) r += 180;
        if (r == 90) {
            vw = pix_h_;
            vh = pix_w_;
        }
    }

    //整体清屏，等比缩放区域之外填背景色
    glViewport(0, 0, fw, fh);
    glClear(GL_COLOR_BUFFER_BIT);

    //按视频宽高比计算 letterbox 视口，窗口缩放时保持等比例
    int rect_x = 0, rect_y = 0, rect_w = fw, rect_h = fh;
    if (vw > 0 && vh > 0) {
        double frame_ratio = (double)vw / (double)vh;
        double win_ratio = (double)fw / (double)fh;
        if (win_ratio > frame_ratio) {
            rect_h = fh;
            rect_w = (int)(fh * frame_ratio + 0.5);
            rect_x = (fw - rect_w) / 2;
        } else {
            rect_w = fw;
            rect_h = (int)(fw / frame_ratio + 0.5);
            rect_y = (fh - rect_h) / 2;
        }
        if (rect_w < 1) rect_w = 1;
        if (rect_h < 1) rect_h = 1;
    }
    glViewport(rect_x, rect_y, rect_w, rect_h);

    switch (cur->format) {
    case AV_PIX_FMT_YUV420P:
    case AV_PIX_FMT_YUVJ420P: {
        if (!yv12_) {
            auto rotate = XTools::get_rotation_from_frame(cur);
            yv12_ = std::make_shared<XShaderYV12>(pix_w_, pix_h_, rotate);
        }
        yv12_->Draw(cur->data[0], cur->linesize[0],  //Y
                    cur->data[1], cur->linesize[1],  //U
                    cur->data[2], cur->linesize[2]); //V
        break;
    }
    case AV_PIX_FMT_NV12: {
        if (!nv12_) {
            nv12_ = std::make_shared<XShaderNV12>(pix_w_, pix_h_);
        }
        nv12_->Draw(cur->data[0], cur->linesize[0],  //Y
                    cur->data[1], cur->linesize[1]); //UV
        break;
    }
    default:
        break;
    }

    av_frame_free(&cur);
}
