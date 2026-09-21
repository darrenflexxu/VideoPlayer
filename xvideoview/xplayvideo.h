#pragma once

#include <QWidget>
#include <QTimer>

class QSplitter;
class XQtOpenGLWidget;

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
    bool eventFilter(QObject* o, QEvent* ev) override;
    void keyPressEvent(QKeyEvent* ev) override;
    bool gpu_decode() const { return gpu_decode_; }
    bool gpu_direct_render() const { return gpu_direct_render_; }

Q_SIGNALS:
    void ReOpen();

public slots:
    void SetSpeed();    //���Ʋ����ٶ�
    void SetVolume();   //���Ʋ���������
    void PlayPos();     //���Ʋ��Ž��� �������ɿ�
    void Pause();       //���ź���ͣ
    void Move();        //�������϶�
    void EnableGPUDecode(int enable);
    void EnableGPUDirectRender(int enable);
    void ToggleFullScreen();   //ȫ��/�˳�ȫ��
    void HideControls();       //ȫ��ʱ�Զ�����Ѳ���

private:
    void setPlayIcon(const QPixmap& pic);
    void pumpSDLEvents();

    Ui::XPlayVideo ui;
    XPlayer player;
    QSplitter* main_splitter_ = nullptr;
    QSplitter* control_layout_ = nullptr;   //全屏时控制栏隐藏/显示
    XQtOpenGLWidget* video_widget_ = nullptr;   //UI 层渲染控件
    bool moved_ = false;
    bool gpu_decode_ = true;
    bool gpu_direct_render_ = false;
    QTimer* sdl_timer_ = nullptr;
    QTimer* hide_timer_ = nullptr;          //全屏时控制栏自动隐藏定时器
};
