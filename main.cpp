#include "mainwindow.h"

#include <QApplication>
#include <QSurfaceFormat>

int main(int argc, char *argv[])
{
    // Alpha must be requested before the native window/context is created.
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setAlphaBufferSize(8);
    format.setDepthBufferSize(24);
    format.setSamples(4);
    QSurfaceFormat::setDefaultFormat(format);

    QApplication a(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("RemoteHub"));
    QCoreApplication::setOrganizationName(QStringLiteral("RemoteHub"));
    MainWindow w;
    w.show();
    return QApplication::exec();
}
