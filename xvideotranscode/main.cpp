#include <QApplication>
#include "xvideotranscode.h"

int main(int argc, char* argv[]) {
  QApplication a(argc, argv);
  auto play_video = new XVideoTranscode();
  play_video->show();
  return a.exec();
}
