#pragma once

#include <QWidget>
#include "ui_xplayvideo.h"
#include "xplayer.h"

class XPlayVideo : public QWidget
{
    Q_OBJECT

public:
    XPlayVideo(QWidget *parent = Q_NULLPTR);
    ~XPlayVideo();
    bool Open(const char* url);
    
    void timerEvent(QTimerEvent* ev) override;
    void Close();
    void closeEvent(QCloseEvent* ev) override;
    bool gpu_decode() const { return gpu_decode_; }
    bool gpu_direct_render() const { return gpu_direct_render_; }

Q_SIGNALS:
    void ReOpen();

public slots:
    void SetSpeed();    //控制播放速度
    void PlayPos();     //控制播放进度 进度条松开
    void Pause();       //播放和暂停
    void Move();        //进度条拖动
    void EnableGPUDecode(int enable);
    void EnableGPUDirectRender(int enable);

private:
    void setPlayIcon(const QPixmap& pic);

    Ui::XPlayVideo ui;
    XPlayer player;
    bool moved_ = false;
    bool gpu_decode_ = true;
    bool gpu_direct_render_ = false;
};
