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
  void Pause();  // ≤•∑≈∫Õ‘›Õ£

 private:
  Ui::XVideoTranscode ui;
  std::shared_ptr<XConvertor> player;
  QString inputURL;
  QString outputURL;
};
