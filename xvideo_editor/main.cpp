#include <QApplication>
#include <QSurfaceFormat>
#include <QStringList>
#include "XVideoEditorWindow.h"

int main(int argc, char* argv[])
{
    //视频着色器使用 GLSL 330 core, 需请求 OpenGL 3.3+ 核心模式(macOS最高4.1)
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setVersion(4, 1);
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    QSurfaceFormat::setDefaultFormat(format);

    QApplication a(argc, argv);
    XVideoEditorWindow w;
    w.importFiles(a.arguments().mid(1));
    w.show();
    return a.exec();
}