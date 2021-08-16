#pragma once

#include <QWidget>
#include "ui_xplayvideo.h"
#include "xcodec/xplayer.h"

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
public slots:
    void SetSpeed();    //控制播放速度
    void PlayPos();     //控制播放进度 进度条松开
    void Pause();       //播放和暂停
    void Move();        //进度条拖动
private:
    Ui::XPlayVideo ui;
    XPlayer player;
    bool moved_ = false;
};
