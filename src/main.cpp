#include <QApplication>
#include <QCoreApplication>
#include <QSettings>
#include <QSurfaceFormat>
#include <QTimer>

#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QCoreApplication::setApplicationName("Mesen");
    QCoreApplication::setOrganizationName("Mesen");

    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    format.setSwapInterval(QSettings().value("settings/verticalSync", false).toBool() ? 1 : 0);
    QSurfaceFormat::setDefaultFormat(format);

    QApplication app(argc, argv);

    MainWindow window;
    window.show();
    if (argc > 1) {
        window.openGame(QString::fromLocal8Bit(argv[1]));
    }

    return app.exec();
}