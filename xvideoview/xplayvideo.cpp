#include "xplayvideo.h"
#include <QDebug>
#include <QLayout>
#include <QSplitter>


void XPlayVideo::timerEvent(QTimerEvent* ev) {
    if (player.is_pause()) {
        ui.play->setStyleSheet(
            "background-image: url(:/XViewer/img/play.png);");
    } else {
        ui.play->setStyleSheet(
            "background-image: url(:/XViewer/img/pause.png);");
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
    player.set_gpu_decode(true);

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
    auto control_layout = new QHBoxLayout(this);
    control_layout->addWidget(ui.play);
    control_layout->addWidget(ui.pos);
    main_layout->addLayout(control_layout);
    auto speed_layout = new QHBoxLayout(this);
    speed_layout->addWidget(ui.speed);
    speed_layout->addWidget(ui.label);
    speed_layout->addWidget(ui.speedtxt);
    main_layout->addLayout(speed_layout);
    setLayout(main_layout);
}

XPlayVideo::~XPlayVideo() {
    Close();
}
