#include "xplayvideo.h"
#include <QCheckBox>
#include <QDebug>
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
void XPlayVideo::closeEvent(QCloseEvent* ev) {
  Close();
}
bool XPlayVideo::Open(const char* url) {
  player.set_gpu_decode(gpu_decode_);
  player.set_gpu_direct_render_(gpu_direct_render_);

  // QtOpenGL 渲染直接绘制到控件，无需窗口句柄
  if (!player.Open(url, nullptr))
    return false;

  //用 XPlayer 创建的 XQtOpenGLWidget 替换占位控件 ui.video
  auto* video_widget = dynamic_cast<QWidget*>(player.view());
  if (video_widget && main_splitter_ && ui.video) {
    int index = main_splitter_->indexOf(ui.video);
    if (index >= 0) {
      main_splitter_->replaceWidget(index, video_widget);
      delete ui.video;
      ui.video = nullptr;
      video_widget->show();
    }
  }

  player.Start();
  player.Pause(false);  // ����״̬
  startTimer(10);
  return true;
}

XPlayVideo::XPlayVideo(QWidget* parent) : QWidget(parent) {
  ui.setupUi(this);
  auto main_layout = new QVBoxLayout(this);
  main_splitter_ = new QSplitter(Qt::Vertical, this);
  main_splitter_->addWidget(ui.video);
  main_layout->addWidget(main_splitter_);
  auto control_layout = new QSplitter(Qt::Horizontal, this);
  control_layout->addWidget(ui.play);
  control_layout->addWidget(ui.pos);
  control_layout->addWidget(ui.speed);
  control_layout->addWidget(ui.label);
  control_layout->addWidget(ui.speedtxt);
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
}

XPlayVideo::~XPlayVideo() {
  Close();
}
