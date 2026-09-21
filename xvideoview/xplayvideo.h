#pragma once

#include <QWidget>
#include <QTimer>
#include "ui_xplayvideo.h"
#include "xplayer.h"

#ifndef WIN32
#include <SDL.h>
#endif

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
    void SetSpeed();    //���Ʋ����ٶ�
    void PlayPos();     //���Ʋ��Ž��� �������ɿ�
    void Pause();       //���ź���ͣ
    void Move();        //�������϶�
    void EnableGPUDecode(int enable);
    void EnableGPUDirectRender(int enable);

private:
    void setPlayIcon(const QPixmap& pic);
    void pumpSDLEvents();

    Ui::XPlayVideo ui;
    XPlayer player;
    bool moved_ = false;
    bool gpu_decode_ = true;
    bool gpu_direct_render_ = false;
    QTimer* sdl_timer_ = nullptr;
};
