#include "XVideoEditorWindow.h"
#include "x_qt_opengl_widget.h"
#include "xdemux_task.h"
#include "xconvertor.h"
#include "xtools.h"
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QTimerEvent>
#include <algorithm>
#include <thread>

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
            [this]() { updateLabels(); });
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

long long XVideoEditorWindow::probeDurationMs(const QString& url)
{
    XDemuxTask t;
    if (!t.Open(url.toStdString()))
        return -1;
    long long d = 0;
    auto vp = t.CopyVideoPara();
    if (vp)
        d = std::max(d, vp->total_ms);
    auto ap = t.CopyAudioPara();
    if (ap)
        d = std::max(d, ap->total_ms);
    t.Stop();
    return d;
}

void XVideoEditorWindow::addClip(const QString& url)
{
    long long dur = probeDurationMs(url);
    if (dur <= 0)
        return;
    model_.addClip(url, 0, dur);
    timeline_->refresh();
    timeline_->setPlayheadMs(player_.pos_ms());
    updateLabels();
}

void XVideoEditorWindow::onAddClicked()
{
    QStringList files = QFileDialog::getOpenFileNames(
        this, "选择视频文件", QString(),
        "视频文件 (*.mp4 *.mov *.mkv *.avi *.ts);;所有文件 (*)");
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

    std::vector<Clip> sorted = model_snapshot;
    std::sort(sorted.begin(), sorted.end(),
              [](const Clip& a, const Clip& b) { return a.pos_ms < b.pos_ms; });

    std::vector<std::string> urls;
    std::vector<std::pair<long long, long long>> seg_trims;
    for (const Clip& c : sorted) {
        urls.push_back(c.url.toStdString());
        seg_trims.push_back({c.source_start_ms, c.source_end_ms});
    }

    bool ok = false;
    QString msg;
    XConvertor cvt;
    cvt.set_gpu_decode(false);
    cvt.set_gpu_encode(false);
    cvt.set_show_progress(false);
    if (!cvt.Open(urls[0].c_str())) {
        msg = "打开源文件失败";
    } else {
        auto vp = cvt.GetVideoCodec();
        auto ap = cvt.GetAudioCodec();
        std::map<std::string, std::string> video_opts;
        std::map<std::string, std::string> audio_opts;
        if (vp) {
            video_opts["crf"] = "18";
            video_opts["preset"] = "medium";
        }
        cvt.StartConcat(urls, out.toStdString().c_str(),
                        vp ? vp->para : nullptr, vp ? vp->time_base : nullptr,
                        ap ? ap->para : nullptr, ap ? ap->time_base : nullptr,
                        video_opts, audio_opts, seg_trims);
        while (!cvt.IsFinished()) {
            MSleep(50);
        }
        if (cvt.HasError())
            msg = QString::fromStdString(cvt.GetError());
        else
            ok = true;
        cvt.Stop();
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