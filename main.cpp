#include <QtGui>
#include <QtWidgets>
#include <QtQml>
#include <QTranslator>
#include "MainWindow.h"
#include "WindowFramelessHelper.h"

int main(int argc, char *argv[]) {
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication a(argc, argv);
    QTranslator translator;

    if (translator.load(":/translate/lang_zh.qm")) {
        a.installTranslator(&translator);
    }
#if 0
    qmlRegisterType<WindowFramelessHelper>("QtShark.Window", 1, 0, "FramelessHelper");
    QQmlApplicationEngine engine;
    engine.load(QUrl(QStringLiteral(":/qml/main.qml")));
    if (engine.rootObjects().isEmpty())
        return -1;
#endif
    MainWindow w;
    w.show();
    return a.exec();
}
