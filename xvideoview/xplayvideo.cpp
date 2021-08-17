#include "xplayvideo.h"
#include <QDebug>
#include <QLayout>
#include <QSplitter>
#include <QCheckBox>


void XPlayVideo::timerEvent(QTimerEvent* ev) {
    if (player.is_pause()) {
        ui.play->setStyleSheet(
            "background: url(:/XViewer/img/play.png) no-repeat;");
    } else {
        ui.play->setStyleSheet(
            "background: url(:/XViewer/img/pause.png) no-repeat;");
    }
    if (player.is_pause())return;
    player.Update();
    auto pos = player.pos_ms();
    auto total = player.total_ms();
    ui.pos->setMaximum(total);

    if (!moved_) {
        ui.pos->setValue(pos);
    }
}
void XPlayVideo::Close() {
    player.Stop();
}
void XPlayVideo::Pause() {
    player.Pause(!player.is_pause());

}
void XPlayVideo::Move()        //进度条拖动
{
    player.Pause(true);
    moved_ = true;
}
void XPlayVideo::EnableGPUDecode(int enable) {
    gpu_decode_ = enable;
    emit ReOpen();
}
void XPlayVideo::EnableGPUDirectRender(int enable) {
    gpu_direct_render_ = enable;
    emit ReOpen();
}
void XPlayVideo::PlayPos()     //控制播放进度
{
    player.Seek(ui.pos->value());
    player.Pause(false);
    moved_ = false;
}
void XPlayVideo::SetSpeed() {
    float speed = 1;
    int s = ui.speed->value();
    if (s <= 10) {
        speed = (float)s / (float)10;
    } else {
        speed = s - 9;
    }
    ui.speedtxt->setText(QString::number(speed));
    player.SetSpeed(speed);
}
void XPlayVideo::closeEvent(QCloseEvent* ev) {
    Close();
}
bool XPlayVideo::Open(const char* url) {
    player.set_gpu_decode(gpu_decode_);
    player.set_gpu_direct_render_(gpu_direct_render_);

    if (!player.Open(url, (void*)ui.video->winId()))
        return false;
    player.Start();
    player.Pause(false);//播放状态
    startTimer(10);
    return true;
}

XPlayVideo::XPlayVideo(QWidget *parent)
    : QWidget(parent) {
    ui.setupUi(this);    
    auto main_layout = new QVBoxLayout(this);
    auto main_splitter = new QSplitter(Qt::Vertical, this);
    main_splitter->addWidget(ui.video);
    main_layout->addWidget(main_splitter);
    auto control_layout = new QSplitter(Qt::Horizontal, this);
    control_layout->addWidget(ui.play);
    control_layout->addWidget(ui.pos);
    control_layout->addWidget(ui.speed);
    control_layout->addWidget(ui.label);
    control_layout->addWidget(ui.speedtxt);
    auto gpu_decode_button = new QCheckBox(tr("GPU Decode"), this);
    gpu_decode_button->setChecked(gpu_decode_);
    connect(gpu_decode_button, &QCheckBox::stateChanged, this, &XPlayVideo::EnableGPUDecode);
    control_layout->addWidget(gpu_decode_button);
    auto gpu_direct_render_button = new QCheckBox(tr("GPU Render"), this);
    gpu_direct_render_button->setChecked(gpu_direct_render_);
    connect(gpu_direct_render_button, &QCheckBox::stateChanged, this, &XPlayVideo::EnableGPUDirectRender);
    control_layout->addWidget(gpu_direct_render_button);
    main_layout->addWidget(control_layout);
    setLayout(main_layout);
}

XPlayVideo::~XPlayVideo() {
    Close();
}
