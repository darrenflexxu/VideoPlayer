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
    for (auto& l : overlay_layers_) {
        if (l.frame) {
            av_frame_free(&l.frame);
            l.frame = nullptr;
        }
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
    for (auto& l : overlay_layers_) {
        if (l.frame) {
            av_frame_free(&l.frame);
            l.frame = nullptr;
        }
    }
    overlay_layers_.clear();
    ov_yv12_.clear();
    ov_nv12_.clear();
    ov_rgba_.clear();
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

bool XQtOpenGLWidget::setOverlayFrame(int idx, AVFrame* frame, const XOverlayRect& rect, float alpha) {
    if (idx < 0)
        return false;
    const int cap = 8;   //最大同时叠加层数
    if (idx >= cap)
        return false;
    AVFrame* copy = nullptr;
    if (frame) {
        copy = av_frame_alloc();
        if (!copy)
            return false;
        if (av_frame_ref(copy, frame) < 0) {
            av_frame_free(&copy);
            return false;
        }
    }
    {
        std::lock_guard<std::mutex> lock(frame_mtx_);
        if ((int)overlay_layers_.size() <= idx) {
            overlay_layers_.resize(idx + 1);
            ov_yv12_.resize(idx + 1);
            ov_nv12_.resize(idx + 1);
            ov_rgba_.resize(idx + 1);
        }
        if (overlay_layers_[idx].frame)
            av_frame_free(&overlay_layers_[idx].frame);
        overlay_layers_[idx].frame = copy;
        overlay_layers_[idx].rect = rect;
        overlay_layers_[idx].alpha = alpha;
    }
    update();
    return true;
}

void XQtOpenGLWidget::clearOverlays() {
    std::lock_guard<std::mutex> lock(frame_mtx_);
    for (auto& l : overlay_layers_) {
        if (l.frame) {
            av_frame_free(&l.frame);
            l.frame = nullptr;
        }
    }
}

//逐层绘制: 先放 base 再放叠加层, 由 glViewport 定位每层的矩形
void XQtOpenGLWidget::drawVideoFrame(AVFrame* frame, std::shared_ptr<XShaderYV12>& yv12,
                                     std::shared_ptr<XShaderNV12>& nv12,
                                     std::shared_ptr<XShaderRGBA>& rgba,
                                     int pw, int ph, float alpha) {
    if (!frame || !frame->data[0])
        return;
    switch (frame->format) {
    case AV_PIX_FMT_YUV420P:
    case AV_PIX_FMT_YUVJ420P: {
        if (!yv12) {
            auto rotate = XTools::get_rotation_from_frame(frame);
            yv12 = std::make_shared<XShaderYV12>(pw, ph, rotate);
        }
        yv12->Draw(frame->data[0], frame->linesize[0],
                   frame->data[1], frame->linesize[1],
                   frame->data[2], frame->linesize[2]);
        break;
    }
    case AV_PIX_FMT_NV12: {
        if (!nv12)
            nv12 = std::make_shared<XShaderNV12>(pw, ph);
        nv12->Draw(frame->data[0], frame->linesize[0],
                   frame->data[1], frame->linesize[1]);
        break;
    }
    case AV_PIX_FMT_RGBA:
    case AV_PIX_FMT_BGRA: {
        if (!rgba)
            rgba = std::make_shared<XShaderRGBA>(pw, ph);
        XShaderRGBA::EnableBlend(true);
        rgba->Draw(frame->data[0], frame->linesize[0], frame->width,
                   frame->height, alpha);
        break;
    }
    default:
        break;
    }
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
    std::vector<XOverlayLayer> ov;
    {
        std::lock_guard<std::mutex> lock(frame_mtx_);
        if (pending_frame_) {
            cur = av_frame_alloc();
            if (!cur || av_frame_ref(cur, pending_frame_) < 0) {
                av_frame_free(&cur);
                cur = nullptr;
            }
        }
        ov = overlay_layers_;   //帧指针为本类持有的引用副本, GUI线程内无释放竞争
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

    //整体清屏，等比缩放区域之外填背景色(各层着色器不再自行清屏)
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
    drawVideoFrame(cur, yv12_, nv12_, rgba_base_, pix_w_, pix_h_, 1.0f);

    //合成叠加层(画中画等): 依次按归一化矩形定位绘制
    int ov_n = (int)ov.size();
    int ov_shader_n = (int)ov_yv12_.size();
    for (int i = 0; i < ov_n; ++i) {
        AVFrame* f = ov[i].frame;
        if (!f)
            continue;
        int ox = (int)(ov[i].rect.x * fw);
        int oy = (int)(ov[i].rect.y * fh);
        int ow = (int)(ov[i].rect.w * fw);
        int oh = (int)(ov[i].rect.h * fh);
        if (ow < 1) ow = 1;
        if (oh < 1) oh = 1;
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        //GL 视口左下角为原点, 归一化 y 是左上原点, 需要翻转
        glViewport(ox, fh - (oy + oh), ow, oh);
        if (i < ov_shader_n) {
            drawVideoFrame(f, ov_yv12_[i], ov_nv12_[i], ov_rgba_[i],
                           f->width, f->height, ov[i].alpha);
            //视频层(YV12/NV12)输出 alpha 恒为1, 不参与混合; RGBA层全透明处已乘alpha
            if (f->format != AV_PIX_FMT_RGBA && f->format != AV_PIX_FMT_BGRA)
                glDisable(GL_BLEND);
        }
        glDisable(GL_BLEND);
    }

    av_frame_free(&cur);
}
