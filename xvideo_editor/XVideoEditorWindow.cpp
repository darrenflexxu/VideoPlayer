#include "XVideoEditorWindow.h"
#include "x_qt_opengl_widget.h"
#include "xdemux_task.h"
#include "xconvertor.h"
#include "xcomposite_exporter.h"
#include "xexport_model.h"
#include "xtools.h"
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QSlider>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QTimerEvent>
#include <QPainter>
#include <QFont>
#include <algorithm>
#include <thread>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
}

//毫秒 -> 时间字符串 (mm:ss / h:mm:ss)
static QString msToText(long long ms)
{
    if (ms < 0) ms = 0;
    long long sec = ms / 1000;
    long long h = sec / 3600;
    long long m = (sec % 3600) / 60;
    long long s = sec % 60;
    if (h > 0)
        return QString("%1:%2:%3").arg(h).arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'));
    return QString("%1:%2").arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'));
}

//毫秒 -> "mm:ss" 短时间字符串(片段文本用)
static QString msToMinSec(long long ms)
{
    if (ms < 0) return "00:00";
    long long sec = ms / 1000;
    long long h = sec / 3600;
    long long m = (sec % 3600) / 60;
    long long s = sec % 60;
    if (h > 0)
        return QString("%1:%2:%3").arg(h).arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'));
    return QString("%1:%2").arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'));
}

XVideoEditorWindow::XVideoEditorWindow(QWidget* parent)
    : QWidget(parent)
{
    setWindowTitle("视频编辑器");

    auto* root = new QVBoxLayout(this);
    auto* split = new QSplitter(Qt::Vertical, this);
    preview_ = new XQtOpenGLWidget(split);
    timeline_ = new TimelineWidget(&model_, split);
    split->addWidget(preview_);
    split->addWidget(timeline_);
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 2);
    split->setSizes({480, 300});
    root->addWidget(split);

    //播放时间水印(QLabel 覆盖预览左上角, 避免在GL画布上用QPainter画字乱码)
    time_overlay_ = new QLabel(preview_);
    time_overlay_->setStyleSheet(
        "color:#ffffff; background-color:rgba(0,0,0,150);"
        "border-radius:6px; padding:4px 10px; font-size:14px; font-weight:bold;");
    time_overlay_->setAttribute(Qt::WA_TransparentForMouseEvents);
    time_overlay_->adjustSize();
    time_overlay_->move(12, 10);

    auto* bar = new QHBoxLayout;
    play_btn_ = new QPushButton("播放", this);
    add_btn_ = new QPushButton("添加视频", this);
    zoom_in_btn_ = new QPushButton("放大", this);
    zoom_out_btn_ = new QPushButton("缩小", this);
    export_btn_ = new QPushButton("导出", this);
    status_ = new QLabel("00:00 / 00:00", this);
    status_->setMinimumWidth(180);
    bar->addWidget(play_btn_);
    bar->addWidget(add_btn_);
    bar->addWidget(zoom_in_btn_);
    bar->addWidget(zoom_out_btn_);
    bar->addWidget(export_btn_);
    bar->addSpacing(16);
    bar->addWidget(new QLabel("音量", this));
    volume_slider_ = new QSlider(Qt::Horizontal, this);
    volume_slider_->setRange(0, 100);
    volume_slider_->setValue(100);
    volume_slider_->setMaximumWidth(140);
    bar->addWidget(volume_slider_);
    bar->addStretch(1);
    bar->addWidget(status_);
    root->addLayout(bar);

    player_.setModel(&model_);
    player_.setView(preview_);

    connect(timeline_, &TimelineWidget::seekRequested, this,
            [this](long long ms) {
                player_.seekTo(ms);
                play_btn_->setText(player_.paused() ? "播放" : "暂停");
            });
    connect(timeline_, &TimelineWidget::timelineChanged, this,
            [this]() {
                player_.resyncBase();   //剪切前段后立即重定位, 避免被裁区继续播放
                updateLabels();
            });
    connect(play_btn_, &QPushButton::clicked, this,
            &XVideoEditorWindow::togglePlay);
    connect(add_btn_, &QPushButton::clicked, this,
            &XVideoEditorWindow::onAddClicked);
    //放大/缩小: 以时间轴视图中心为锚点缩放
    connect(zoom_in_btn_, &QPushButton::clicked, this,
            [this]() {
                timeline_->zoomBy(1.4, timeline_->mapToScene(
                    timeline_->viewport()->rect().center()));
            });
    connect(zoom_out_btn_, &QPushButton::clicked, this,
            [this]() {
                timeline_->zoomBy(1.0 / 1.4, timeline_->mapToScene(
                    timeline_->viewport()->rect().center()));
            });
    connect(export_btn_, &QPushButton::clicked, this,
            &XVideoEditorWindow::doExport);
    //音量: 0~100 -> 0.0~1.0, 作用于基片与所有叠加层
    connect(volume_slider_, &QSlider::valueChanged, this,
            [this](int v) { player_.set_volume(v / 100.0f); });

    resize(960, 640);
    startTimer(33);            //UI 刷新: 播放头/状态/导出
    frame_timer_ = startTimer(8);  //高频拉帧: 平滑节拍(空闲时开销极小)
}

bool XVideoEditorWindow::importFiles(const QStringList& files)
{
    if (files.isEmpty())
        return false;
    for (const QString& f : files)
        addClip(f);
    return true;
}

void XVideoEditorWindow::addClip(const QString& url)
{
    //探针: 打开解封装判断流构成与总时长(音视频取较大者)
    XDemuxTask probe;
    if (!probe.Open(url.toStdString()))
        return;
    auto vp = probe.CopyVideoPara();
    auto ap = probe.CopyAudioPara();
    long long dur = 0;
    if (vp)
        dur = std::max(dur, vp->total_ms);
    if (ap)
        dur = std::max(dur, ap->total_ms);
    probe.Stop();
    if (dur <= 0)
        return;

    //新片段: 每个文件独占一条轨道, 但时间上紧贴上一个片段末尾
    //即: track = clipCount(), pos = 当前时间轴总时长(所有轨道最后片段的终点)
    int track = model_.clipCount();
    long long pos = model_.total_ms();  //上一片段结束位置
    if (vp) {
        model_.addTrack(track, ClipType::VIDEO, QString("视频轨 %1").arg(track + 1));
        // 兼容旧接口: addClip(url, src_start, src_end, trackIndex) 会自动计算 pos_ms,
        // 这里需要手动指定位置, 用 addClip(const Clip&)
        Clip c;
        c.type = ClipType::VIDEO;
        c.url = url;
        c.source_start_ms = 0;
        c.source_end_ms = dur;
        c.source_total_ms = dur;
        c.pos_ms = pos;
        c.trackIndex = track;
        model_.addClip(c);
    } else {
        model_.addTrack(track, ClipType::AUDIO, QString("音频轨 %1").arg(track + 1));
        Clip c;
        c.type = ClipType::AUDIO;
        c.url = url;
        c.source_start_ms = 0;
        c.source_end_ms = dur;
        c.source_total_ms = dur;
        c.pos_ms = pos;
        c.trackIndex = track;
        model_.addClip(c);
    }
    timeline_->refresh();
    timeline_->setPlayheadMs(player_.pos_ms());
    updateLabels();
}

void XVideoEditorWindow::onAddClicked()
{
    QStringList files = QFileDialog::getOpenFileNames(
        this, "选择视频/音频文件", QString(),
        "视频/音频文件 (*.mp4 *.mov *.mkv *.avi *.ts *.m4a);;所有文件 (*)");
    importFiles(files);
}

void XVideoEditorWindow::togglePlay()
{
    if (player_.paused()) {
        timeline_->resumeFollow();   //开始播放时恢复播放头自动跟随
        player_.play();
    } else {
        player_.pause();
    }
    play_btn_->setText(player_.paused() ? "播放" : "暂停");
}

void XVideoEditorWindow::doExport()
{
    if (model_.empty() || exporting_)
        return;
    QString out = QFileDialog::getSaveFileName(
        this, "导出视频", QStringLiteral("拼接_输出.mp4"), "MP4 文件 (*.mp4)");
    if (out.isEmpty())
        return;
    exporting_ = true;
    export_done_ = false;
    if (status_)
        status_->setText("导出中...");
    std::thread([this, out]() { exportWorker(out); }).detach();
}

void XVideoEditorWindow::exportWorker(const QString& out)
{
    //拷贝一份模型, 避免与UI线程的编辑操作竞争
    const std::vector<Clip> model_snapshot(model_.clips().begin(), model_.clips().end());
    if (model_snapshot.empty()) {
        QMutexLocker locker(&export_mutex_);
        export_ok_ = false;
        export_msg_ = "时间轴为空";
        export_done_ = true;
        return;
    }

    //TimelineModel -> XExportModel: 保留轨道/位置/裁剪/矩形, 类型与贴纸/字幕一一对应
    XExportModel xm;
    long long total = 0;
    for (const Clip& c : model_snapshot) {
        XExportClip xc;
        switch (c.type) {
        case ClipType::AUDIO:   xc.type = XClipType::AUDIO;   break;
        case ClipType::SUBTITLE:xc.type = XClipType::SUBTITLE;break;
        case ClipType::STICKER: xc.type = XClipType::STICKER; break;
        default:                xc.type = XClipType::VIDEO;   break;
        }
        xc.url = c.url.toStdString();
        xc.source_start_ms = c.source_start_ms;
        xc.source_end_ms = c.source_end_ms;
        xc.pos_ms = c.pos_ms;
        xc.track_index = c.trackIndex;
        xc.rect = XClipRect{c.rect.x, c.rect.y, c.rect.w, c.rect.h};
        xc.alpha = c.alpha;
        xc.text = c.text.toStdString();
        total = std::max(total, c.pos_ms + c.duration_ms());
        xm.clips.push_back(xc);
    }

    bool ok = false;
    QString msg;
    XCompositeExporter ex;
    ex.setModel(xm);

    //视频输出参数: 以第一个视频片段的源分辨率/帧率为基准, 无视频则用默认值
    XCompositeExporter::VideoOpts vo;
    bool have_video = false;
    for (const Clip& c : model_snapshot) {
        if (c.type != ClipType::VIDEO)
            continue;
        XDemuxTask probe;
        if (probe.Open(c.url.toStdString())) {
            auto vp = probe.CopyVideoPara();
            if (vp && vp->para && vp->para->width > 0 && vp->para->height > 0) {
                vo.width = vp->para->width;
                vo.height = vp->para->height;
            }
            probe.Stop();
        }
        have_video = true;
        break;
    }
    ex.setVideoOpts(vo);

    //贴纸/字幕: 逐帧回调提供RGBA画面(与时间轴预览的合成逻辑一致)
    ex.setRgbaProvider([this, &model_snapshot](const XExportClip* xc, long long /*t*/) -> AVFrame* {
        const Clip* src = nullptr;
        for (const Clip& c : model_snapshot) {
            if (c.url.toStdString() == xc->url && c.trackIndex == xc->track_index) {
                src = &c;
                break;
            }
        }
        if (!src)
            return nullptr;
        QImage q;
        if (src->type == ClipType::STICKER) {
            q = QImage(src->url);
        } else if (src->type == ClipType::SUBTITLE) {
            q = QImage(640, 160, QImage::Format_RGBA8888);
            q.fill(Qt::transparent);
            QPainter p(&q);
            p.setRenderHint(QPainter::TextAntialiasing, true);
            p.setPen(Qt::white);
            QFont f("Arial", 48, QFont::Bold);
            p.setFont(f);
            p.drawText(q.rect(), Qt::AlignCenter | Qt::TextWordWrap, src->text);
            p.end();
        }
        if (q.isNull())
            return nullptr;
        QImage rgba = q.convertToFormat(QImage::Format_RGBA8888);
        AVFrame* f = av_frame_alloc();
        f->format = AV_PIX_FMT_RGBA;
        f->width = rgba.width();
        f->height = rgba.height();
        if (av_frame_get_buffer(f, 32) < 0) {
            av_frame_free(&f);
            return nullptr;
        }
        for (int y = 0; y < rgba.height(); ++y)
            memcpy(f->data[0] + (size_t)y * f->linesize[0],
                   rgba.constScanLine(y), (size_t)rgba.width() * 4);
        return f;
    });

    if (!ex.Start(out.toStdString())) {
        msg = QString::fromStdString(ex.GetError());
        if (msg.isEmpty())
            msg = "启动合成导出失败";
    } else {
        while (!ex.IsFinished())
            MSleep(50);
        ex.Wait();
        if (ex.HasError())
            msg = QString::fromStdString(ex.GetError());
        else
            ok = true;
        ex.Stop();
    }

    QMutexLocker locker(&export_mutex_);
    export_ok_ = ok;
    export_msg_ = msg;
    export_done_ = true;
}

void XVideoEditorWindow::updateLabels()
{
    if (!status_)
        return;
    status_->setText(msToMinSec(player_.pos_ms()) + " / " + msToMinSec(player_.total_ms()));
}

void XVideoEditorWindow::onFrameTimer()
{
    player_.Update();          // 高频拉取解码帧, 由控件去重并按解码节奏渲染
}

void XVideoEditorWindow::timerEvent(QTimerEvent* ev)
{
    if (ev->timerId() == frame_timer_) {
        onFrameTimer();
        return;
    }
    timeline_->setPlayheadMs(player_.pos_ms());
    updateLabels();
    //预览画面叠加播放时间水印(播放/拖动时实时更新)
    if (time_overlay_) {
        QString t = msToText(player_.pos_ms()) + " / " + msToText(player_.total_ms());
        if (time_overlay_->text() != t) {
            time_overlay_->setText(t);
            time_overlay_->adjustSize();
            time_overlay_->move(12, 10);
        }
    }

    if (exporting_ && export_done_) {
        QMutexLocker locker(&export_mutex_);
        exporting_ = false;
        bool ok = export_ok_;
        QString msg = export_msg_;
        export_done_ = false;
        locker.unlock();
        if (ok)
            QMessageBox::information(this, "导出完成", "视频导出成功");
        else
            QMessageBox::warning(this, "导出失败", msg.isEmpty() ? "未知错误" : msg);
    }
}