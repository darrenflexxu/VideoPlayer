#include <QApplication>
#include <QDebug>
#include <QFileDialog>
#include "xplayer.h"
#include "xplayvideo.h"

int main(int argc, char* argv[]) {
  QApplication a(argc, argv);
  auto arg_list = a.arguments();
  auto play_video = new XPlayVideo();
  QString s;
  int count = arg_list.size();

  if (count > 1) {
    s = arg_list[1];
  } else {
    s = QFileDialog::getOpenFileName(
        nullptr, QStringLiteral("选择要播放的文件"),
        "",  // 初始目录
        QStringLiteral("视频文件 (*.flv *.rmvb *.avi *.MP4 *.mkv);;") +
            QStringLiteral("音频文件 (*.mp3 *.wma *.wav);;") +
            QStringLiteral("所有文件 (*.*)"));
  }
  play_video->Open(s.toStdString().c_str());
  play_video->show();

  return a.exec();
}
