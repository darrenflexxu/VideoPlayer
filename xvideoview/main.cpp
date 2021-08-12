#include <QApplication>
#include <QWidget>
#include <QDebug>
#include <QLayout>
#include <QLabel>
#include "xcodec/xplayer.h"
#include "xplayvideo.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    XPlayVideo play_video;
    play_video.Open("C:/xdf/国际汉语录制视频/邱蕾 421302199004175182+北京的气候.mp4");
    play_video.show();
    
    return a.exec();
}

