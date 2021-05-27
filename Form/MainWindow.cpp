#include "MainWindow.h"
#include "ui_MainWindow.h"

#include <QPainter>
#include <QPaintEvent>
#include <QFileDialog>
#include <QDebug>
#include <QDesktopWidget>
#include <QFontDatabase>
#include <QMouseEvent>
#include <QMessageBox>
#include "AppConfig.h"
#include "Base/FunctionTransfer.h"
#include <mutex>
#ifdef WIN32
#include <windows.h>
#include <d3d9.h>
#endif
extern "C" { //指定函数是c语言函数，函数名不包含重载标注
             //引用ffmpeg头文件
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
}

#ifdef WIN32

struct DXVA2DevicePriv {
    HMODULE d3dlib;
    HMODULE dxva2lib;
    HANDLE device_handle;
    IDirect3D9* d3d9;
    IDirect3DDevice9* d3d9device;
};
void DrawFrame(AVFrame* frame, AVCodecContext* c) {
    static std::mutex mtx;
    std::unique_lock<std::mutex> lock(mtx);
    if (!frame->data[3] || !c)return;
    auto surface = (IDirect3DSurface9*)frame->data[3];
    auto ctx = (AVHWDeviceContext*)c->hw_device_ctx->data;
    auto priv = (DXVA2DevicePriv*)ctx->user_opaque;
    auto device = priv->d3d9device;
    static HWND hwnd = nullptr;
    static RECT viewport;
    if (!hwnd) {
        hwnd = CreateWindow(L"DX", L"Test DXVA", WS_OVERLAPPEDWINDOW,
                            200, 200, frame->width, frame->height, 0, 0, 0, 0);
        ShowWindow(hwnd, 1);
        UpdateWindow(hwnd);
        viewport.left = 0;
        viewport.right = frame->width;
        viewport.top = 0;
        viewport.bottom = frame->height;
    }
    //device->SetRenderState(D3DRS_LIGHTING, FALSE);
    //设置显示窗口句柄
    device->Present(&viewport, &viewport, hwnd, 0);
    //后台缓冲表面
    static IDirect3DSurface9* back = nullptr;
    if (!back)
        device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back);
    device->StretchRect(surface, 0, back, 0, D3DTEXF_LINEAR);
    av_frame_free(&frame);
}

void DrawFrameWithHandle(AVFrame* frame, AVCodecContext* c, void* hwnd, int width, int height) {    
    if (!frame->data[3] || !c || !hwnd)return;
    static std::mutex mtx;
    std::unique_lock<std::mutex> lock(mtx);
    auto surface = (IDirect3DSurface9*)frame->data[3];
    auto ctx = (AVHWDeviceContext*)c->hw_device_ctx->data;
    auto priv = (DXVA2DevicePriv*)ctx->user_opaque;
    auto device = priv->d3d9device;
    RECT viewport;
    viewport.left = 0;
    viewport.right = width;
    viewport.top = 0;
    viewport.bottom = height;
    //device->SetRenderState(D3DRS_LIGHTING, FALSE);
    //设置显示窗口句柄
    device->Present(&viewport, &viewport, (HWND)hwnd, 0);
    //后台缓冲表面
    static IDirect3DSurface9* back = nullptr;
    if (!back)
        device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back);
    device->StretchRect(surface, 0, back, 0, D3DTEXF_LINEAR);
    av_frame_free(&frame);
}
#endif

Q_DECLARE_METATYPE(VideoPlayerState)

MainWindow::MainWindow(QWidget *parent):
    DragAbleWidget(parent),
    ui_(new Ui::MainWindow) {
    ui_->setupUi(this->getContainWidget());
    FunctionTransfer::init(QThread::currentThreadId());
    ///初始化播放器
    setWindowFlags(Qt::FramelessWindowHint);//|Qt::WindowStaysOnTopHint);  //使窗口的标题栏隐藏
    //因为VideoPlayer::PlayerState是自定义的类型 要跨线程传递需要先注册一下
    qRegisterMetaType<VideoPlayerState>();
    connect(ui_->pushButton_open, &QPushButton::clicked, this, &MainWindow::slotBtnClick);
    connect(ui_->toolButton_open, &QPushButton::clicked, this, &MainWindow::slotBtnClick);
    connect(ui_->pushButton_play, &QPushButton::clicked, this, &MainWindow::slotBtnClick);
    connect(ui_->pushButton_pause, &QPushButton::clicked, this, &MainWindow::slotBtnClick);
    connect(ui_->pushButton_stop, &QPushButton::clicked, this, &MainWindow::slotBtnClick);
    connect(ui_->pushButton_volume, &QPushButton::clicked, this, &MainWindow::slotBtnClick);
    connect(ui_->horizontalSlider, SIGNAL(sig_valueChanged(int)), this, SLOT(slotSliderMoved(int)));
    connect(ui_->horizontalSlider_volume, SIGNAL(valueChanged(int)), this, SLOT(slotSliderMoved(int)));
    ui_->page_video->setMouseTracking(true);
    ui_->page_video->installEventFilter(this);
    ui_->widget_container->installEventFilter(this);
    mPlayer = CreateVideoPlayer();
    mPlayer->setVideoPlayerCallBack(this);
    mTimer = new QTimer; //定时器-获取当前视频时间
    connect(mTimer, SIGNAL(timeout()), this, SLOT(slotTimerTimeOut()));
    mTimer->setInterval(500);
    mTimer_CheckControlWidget = new QTimer; //用于控制控制界面的出现和隐藏
    connect(mTimer_CheckControlWidget, SIGNAL(timeout()), this, SLOT(slotTimerTimeOut()));
    mTimer_CheckControlWidget->setInterval(3000);
    mAnimation_ControlWidget = new QPropertyAnimation(ui_->widget_controller, "geometry");
    ui_->stackedWidget->setCurrentWidget(ui_->page_open);
    ui_->pushButton_pause->hide();
    resize(1024, 768);
    setTitle(QStringLiteral("VideoProc-V%1").arg(AppConfig::VERSION_NAME));
    mVolume = mPlayer->getVolume();
}

MainWindow::~MainWindow() {
    ReleaseVideoPlayer(mPlayer);
    AppConfig::saveConfigInfoToFile();
    AppConfig::removeDirectory(AppConfig::AppDataPath_Tmp);
    delete ui_;
}

void MainWindow::showOutControlWidget() {
    mAnimation_ControlWidget->setDuration(800);
    int w = ui_->widget_controller->width();
    int h = ui_->widget_controller->height();
    int x = 0;
    int y = ui_->widget_container->height() - ui_->widget_controller->height();

    if (ui_->widget_controller->isHidden()) {
        ui_->widget_controller->show();
        mAnimation_ControlWidget->setStartValue(ui_->widget_controller->geometry());
    } else {
        mAnimation_ControlWidget->setStartValue(ui_->widget_controller->geometry());
    }
    mAnimation_ControlWidget->setEndValue(QRect(x, y, w, h));
    mAnimation_ControlWidget->setEasingCurve(QEasingCurve::Linear); //设置动画效果
    mAnimation_ControlWidget->start();
}

void MainWindow::hideControlWidget() {
    mAnimation_ControlWidget->setTargetObject(ui_->widget_controller);
    mAnimation_ControlWidget->setDuration(300);
    int w = ui_->widget_controller->width();
    int h = ui_->widget_controller->height();
    int x = 0;
    int y = ui_->widget_container->height() + h;
    mAnimation_ControlWidget->setStartValue(ui_->widget_controller->geometry());
    mAnimation_ControlWidget->setEndValue(QRect(x, y, w, h));
    mAnimation_ControlWidget->setEasingCurve(QEasingCurve::Linear); //设置动画效果
    mAnimation_ControlWidget->start();
}

void MainWindow::slotSliderMoved(int value) {
    if (QObject::sender() == ui_->horizontalSlider) {
        mPlayer->seek((qint64)value * 1000000);
    } else if (QObject::sender() == ui_->horizontalSlider_volume) {
        mPlayer->setVolume(value / 100.0);
        ui_->label_volume->setText(QString("%1").arg(value));
    }
}

void MainWindow::slotTimerTimeOut() {
    if (QObject::sender() == mTimer) {
        qint64 Sec = mPlayer->getCurrentTime();
        ui_->horizontalSlider->setValue(Sec);
        QString curTime;
        QString hStr = QString("0%1").arg(Sec / 3600);
        QString mStr = QString("0%1").arg(Sec / 60 % 60);
        QString sStr = QString("0%1").arg(Sec % 60);
        if (hStr == "00") {
            curTime = QString("%1:%2").arg(mStr.right(2)).arg(sStr.right(2));
        } else {
            curTime = QString("%1:%2:%3").arg(hStr).arg(mStr.right(2)).arg(sStr.right(2));
        }
        ui_->label_currenttime->setText(curTime);
    } else if (QObject::sender() == mTimer_CheckControlWidget) {
        mTimer_CheckControlWidget->stop();
        hideControlWidget();
    }
}

void MainWindow::slotBtnClick(bool isChecked) {
    if (QObject::sender() == ui_->pushButton_play) {
        mPlayer->play();
    } else if (QObject::sender() == ui_->pushButton_pause) {
        mPlayer->pause();
    } else if (QObject::sender() == ui_->pushButton_stop) {
        mPlayer->stop(true);
    } else if (QObject::sender() == ui_->pushButton_open || QObject::sender() == ui_->toolButton_open) {
        QString s = QFileDialog::getOpenFileName(
            this, QStringLiteral("选择要播放的文件"),
            AppConfig::gVideoFilePath,//初始目录
            QStringLiteral("视频文件 (*.flv *.rmvb *.avi *.MP4 *.mkv);;")
            + QStringLiteral("音频文件 (*.mp3 *.wma *.wav);;")
            + QStringLiteral("所有文件 (*.*)"));
        if (!s.isEmpty()) {
            mPlayer->stop(true); //如果在播放则先停止
            mPlayer->startPlay(s.toStdString().c_str());
            AppConfig::gVideoFilePath = s;
            AppConfig::saveConfigInfoToFile();
        }
    } else if (QObject::sender() == ui_->pushButton_volume) {
        qDebug() << isChecked;
        bool isMute = isChecked;
        mPlayer->setMute(isMute);
        if (isMute) {
            mVolume = mPlayer->getVolume();
            ui_->horizontalSlider_volume->setValue(0);
            ui_->horizontalSlider_volume->setEnabled(false);
            ui_->label_volume->setText(QString("%1").arg(0));
        } else {
            int volume = mVolume * 100.0;
            ui_->horizontalSlider_volume->setValue(volume);
            ui_->horizontalSlider_volume->setEnabled(true);
            ui_->label_volume->setText(QString("%1").arg(volume));
        }
    }
}

///打开文件失败
void MainWindow::onOpenVideoFileFailed(const int &code) {
    FunctionTransfer::runInMainThread([=] () {
        QMessageBox::critical(NULL, "tips", QString("open file failed %1").arg(code));
    });
}

///打开SDL失败的时候回调此函数
void MainWindow::onOpenSdlFailed(const int &code) {
    FunctionTransfer::runInMainThread([=] () {
        QMessageBox::critical(NULL, "tips", QString("open Sdl failed %1").arg(code));
    });
}

///获取到视频时长的时候调用此函数
void MainWindow::onTotalTimeChanged(const int64_t &uSec) {
    FunctionTransfer::runInMainThread([=] () {
        qint64 Sec = uSec / 1000000;
        ui_->horizontalSlider->setRange(0, Sec);
        QString totalTime;
        QString hStr = QString("0%1").arg(Sec / 3600);
        QString mStr = QString("0%1").arg(Sec / 60 % 60);
        QString sStr = QString("0%1").arg(Sec % 60);
        if (hStr == "00") {
            totalTime = QString("%1:%2").arg(mStr.right(2)).arg(sStr.right(2));
        } else {
            totalTime = QString("%1:%2:%3").arg(hStr).arg(mStr.right(2)).arg(sStr.right(2));
        }
        ui_->label_totaltime->setText(totalTime);
    });
}

///播放器状态改变的时候回调此函数
void MainWindow::onPlayerStateChanged(const VideoPlayerState &state, const bool &hasVideo, const bool &hasAudio) {
    FunctionTransfer::runInMainThread([=] () {
        if (state == VideoPlayer_Stop) {
            ui_->stackedWidget->setCurrentWidget(ui_->page_open);
            ui_->pushButton_pause->hide();
            ui_->widget_videoPlayer->clear();
            ui_->horizontalSlider->setValue(0);
            ui_->label_currenttime->setText("00:00");
            ui_->label_totaltime->setText("00:00");
            mTimer->stop();
        } else if (state == VideoPlayer_Playing) {
            if (hasVideo) {
                ui_->stackedWidget->setCurrentWidget(ui_->page_video);
            } else {
                ui_->stackedWidget->setCurrentWidget(ui_->page_audio);
            }
            ui_->pushButton_play->hide();
            ui_->pushButton_pause->show();
            mTimer->start();
        } else if (state == VideoPlayer_Pause) {
            ui_->pushButton_pause->hide();
            ui_->pushButton_play->show();
        }
    });
}

///显示视频数据，此函数不宜做耗时操作，否则会影响播放的流畅性。
void MainWindow::onDisplayVideo(IVideoFrame* videoFrame) {
    ui_->widget_videoPlayer->inputOneFrame(videoFrame);
}

bool MainWindow::OnEnableGPUDecode() {
    return AppConfig::gVideoHardDecoder;
}

bool MainWindow::OnRenderGPUNoCopy() {
    return true;
}

void MainWindow::onDisplayVideo(AVFrame * frame, AVCodecContext * codec_ctx) {
    auto new_frame = av_frame_clone(frame);
    FunctionTransfer::runInMainThread([=] () {
        ui_->stackedWidget->setCurrentWidget(ui_->page_audio);
        ui_->label->resize(new_frame->width, new_frame->height);
        DrawFrameWithHandle(new_frame, codec_ctx, (void*)ui_->label->winId(), ui_->label->width(), ui_->label->height());
    });
}

//图片显示部件时间过滤器处理
bool MainWindow::eventFilter(QObject *target, QEvent *event) {
    if (target == ui_->widget_container) {
        if (event->type() == QEvent::Resize) {
            ///停止动画，防止此时刚好开始动画，导致位置出错
            mAnimation_ControlWidget->stop();
            QResizeEvent * e = (QResizeEvent*)event;
            int w = e->size().width();
            int h = e->size().height();
            ui_->stackedWidget->move(0, 0);
            ui_->stackedWidget->resize(w, h);
            int x = 0;
            int y = h - ui_->widget_controller->height();
            ui_->widget_controller->move(x, y);
            ui_->widget_controller->resize(w, ui_->widget_controller->height());
        }
    } else if (target == ui_->page_video) {
        if (event->type() == QEvent::Enter || event->type() == QEvent::MouseButtonPress) {
            if (!mTimer_CheckControlWidget->isActive()) {
                showOutControlWidget();
                mTimer_CheckControlWidget->start();
            }
        } else if (event->type() == QEvent::Leave) {
            mTimer_CheckControlWidget->stop();
            mTimer_CheckControlWidget->start();
        }
    }
    //其它部件产生的事件则交给基类处理
    return DragAbleWidget::eventFilter(target, event);
}
