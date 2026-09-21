#include "xplayvideo.h"
#include "x_qt_opengl_widget.h"
#include <QCheckBox>
#include <QDebug>
#include <QKeyEvent>
#include <QLayout>
#include <QSplitter>

#ifndef WIN32
#include <SDL.h>
#endif

void XPlayVideo::pumpSDLEvents() {
#ifndef WIN32
    for (int i = 0; i < 5; ++i) {
        SDL_PumpEvents();
    }
#endif
}

//毫秒转播放时间文本, 超1小时显示 h:mm:ss
static QString msToTime(long long ms) {
  if (ms < 0) ms = 0;
  long long total_s = ms / 1000;
  long long h = total_s / 3600;
  long long m = (total_s % 3600) / 60;
  long long s = total_s % 60;
  if (h > 0)
    return QString("%1:%2:%3")
        .arg(h)
        .arg(m, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0'));
  return QString("%1:%2")
      .arg(m, 2, 10, QChar('0'))
      .arg(s, 2, 10, QChar('0'));
}

void XPlayVideo::timerEvent(QTimerEvent* ev) {
  if (player.is_pause()) {
    setPlayIcon(QPixmap(":/XViewer/img/play.png"));
  } else {
    setPlayIcon(QPixmap(":/XViewer/img/pause.png"));
  }
  if (player.is_pause())
    return;
  player.Update();
  auto pos = player.pos_ms();
  auto total = player.total_ms();
  ui.pos->setMaximum(total);
  ui.timelabel->setText(msToTime(pos) + " / " + msToTime(total));

  if (!moved_) {
    ui.pos->setValue(pos);
  }

  if (player.IsFinish()) {
    player.Pause(true);
    player.Seek(0);
    ui.pos->setValue(0);
    player.ClearFinish();
  }
  
  pumpSDLEvents();
}
void XPlayVideo::Close() {
  player.Stop();
}
void XPlayVideo::Pause() {
  player.Pause(!player.is_pause());
}
void XPlayVideo::Move()  // �������϶�
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

void XPlayVideo::setPlayIcon(const QPixmap& pic) {
  auto icon = pic.scaled(ui.play->size());
  ui.play->setIcon(icon);
  ui.play->setIconSize(ui.play->size());
}

void XPlayVideo::PlayPos()  // ���Ʋ��Ž���
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
void XPlayVideo::SetVolume() {
  float vol = ui.volume->value() / 100.0f;
  player.SetVolume(vol);
}
void XPlayVideo::closeEvent(QCloseEvent* ev) {
  Close();
}
void XPlayVideo::ToggleFullScreen() {
  if (isFullScreen()) {
    showNormal();
    if (control_layout_) control_layout_->show();
  } else {
    showFullScreen();
    //进入全屏后先保留控制栏, 无操作时自动隐藏
    if (control_layout_) control_layout_->show();
    if (hide_timer_) hide_timer_->start();
  }
}
void XPlayVideo::HideControls() {
  if (isFullScreen() && control_layout_ && control_layout_->isVisible())
    control_layout_->hide();
}
bool XPlayVideo::eventFilter(QObject* o, QEvent* ev) {
  if (o == video_widget_) {
    //双击视频区域切换全屏
    if (ev->type() == QEvent::MouseButtonDblClick) {
      ToggleFullScreen();
      return true;
    }
    //全屏下移动鼠标唤起控制栏并重新计时
    if (ev->type() == QEvent::MouseMove && isFullScreen()) {
      if (control_layout_ && !control_layout_->isVisible())
        control_layout_->show();
      if (hide_timer_) hide_timer_->start();
    }
  }
  return QWidget::eventFilter(o, ev);
}
void XPlayVideo::keyPressEvent(QKeyEvent* ev) {
  if (ev->key() == Qt::Key_Escape) {
    if (isFullScreen()) {
      showNormal();
      if (control_layout_) control_layout_->show();
      ev->accept();
      return;
    }
  } else if (ev->key() == Qt::Key_F) {
    ToggleFullScreen();
    ev->accept();
    return;
  }
  QWidget::keyPressEvent(ev);
}
bool XPlayVideo::Open(const char* url) {
  player.set_gpu_decode(gpu_decode_);
  player.set_gpu_direct_render_(gpu_direct_render_);

  //注入 UI 层的 QOpenGLWidget 作为渲染对象(Qt 实现不进 XCodec)
  player.set_view(video_widget_);

  // QOpenGLWidget 直接绘制到控件, 无需窗口句柄
  if (!player.Open(url, nullptr))
    return false;

  player.Start();
  player.Pause(false);  // ����״̬
  startTimer(10);
  return true;
}

XPlayVideo::XPlayVideo(QWidget* parent) : QWidget(parent) {
  ui.setupUi(this);
  auto main_layout = new QVBoxLayout(this);
  main_splitter_ = new QSplitter(Qt::Vertical, this);
  //用 UI 层渲染控件替换占位控件 ui.video
  video_widget_ = new XQtOpenGLWidget(main_splitter_);
  main_splitter_->addWidget(video_widget_);
  //双击视频切换全屏, 全屏时鼠标移动(重新)唤起控制栏
  video_widget_->installEventFilter(this);
  video_widget_->setMouseTracking(true);
  delete ui.video;
  ui.video = nullptr;
  main_layout->addWidget(main_splitter_);
  auto control_layout = new QSplitter(Qt::Horizontal, this);
  control_layout_ = control_layout;
  control_layout->addWidget(ui.play);
  control_layout->addWidget(ui.pos);
  control_layout->addWidget(ui.speed);
  control_layout->addWidget(ui.label);
  control_layout->addWidget(ui.speedtxt);
  control_layout->addWidget(ui.vollabel);
  control_layout->addWidget(ui.volume);
  control_layout->addWidget(ui.timelabel);
  connect(ui.volume, &QSlider::valueChanged, this, &XPlayVideo::SetVolume);
  auto gpu_decode_button = new QCheckBox(tr("GPU Decode"), this);
  gpu_decode_button->setChecked(gpu_decode_);
  gpu_decode_button->setEnabled(false);
  connect(gpu_decode_button, &QCheckBox::stateChanged, this,
          &XPlayVideo::EnableGPUDecode);
  control_layout->addWidget(gpu_decode_button);
  auto gpu_direct_render_button = new QCheckBox(tr("GPU Direct"), this);
  gpu_direct_render_button->setChecked(gpu_direct_render_);
  gpu_direct_render_button->setEnabled(false);
  connect(gpu_direct_render_button, &QCheckBox::stateChanged, this,
          &XPlayVideo::EnableGPUDirectRender);
  control_layout->addWidget(gpu_direct_render_button);
  main_layout->addWidget(control_layout);
  ui.play->setFixedSize(30, 30);
  setPlayIcon(QPixmap(":/XViewer/img/play.png"));
  setLayout(main_layout);

  //全屏时控制栏自动隐藏(鼠标移动会重新唤起)
  hide_timer_ = new QTimer(this);
  hide_timer_->setSingleShot(true);
  hide_timer_->setInterval(3000);
  connect(hide_timer_, &QTimer::timeout, this, &XPlayVideo::HideControls);
}

XPlayVideo::~XPlayVideo() {
  Close();
}
