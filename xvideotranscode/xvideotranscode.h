#pragma once

#include <QWidget>
#include "ui_xvideotranscode.h"
#include "xconvertor.h"

class XVideoTranscode : public QWidget {
  Q_OBJECT

 public:
  XVideoTranscode(QWidget* parent = Q_NULLPTR);
  ~XVideoTranscode();

  void timerEvent(QTimerEvent* ev) override;
  void Close();
  void closeEvent(QCloseEvent* ev) override;

 public slots:
  void Pause();  // ���ź���ͣ

 private:
  void SetInputVideoInfo(const std::shared_ptr<XPara>& video_para);
  void SetOutputVideoInfo(const std::shared_ptr<XPara>& video_para);
  void SetInputAudioInfo(const std::shared_ptr<XPara>& audio_para);
  void SetOutputAudioInfo(const std::shared_ptr<XPara>& audio_para);

  Ui::XVideoTranscode ui;
  std::shared_ptr<XConvertor> player;
  QString inputURL;
  std::shared_ptr<XPara> inputVideoPara;
  std::shared_ptr<XPara> inputAudioPara;
  QString outputURL;
  bool running_ = false;
  int timer_id_ = 0;
};
