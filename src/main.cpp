#include "app/MainWindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QIcon>
#include <QStyleFactory>

#ifndef ZSTREAMEYE_VERSION
#define ZSTREAMEYE_VERSION "0.1.10"
#endif

int main(int argc, char *argv[])
{
    QCoreApplication::setOrganizationName(QStringLiteral("ZStreamEye"));
    QCoreApplication::setApplicationName(QStringLiteral("ZStreamEye"));
    QCoreApplication::setApplicationVersion(QStringLiteral(ZSTREAMEYE_VERSION));

    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv);
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/zstreameye.png")));

    MainWindow window;
    window.show();

    return app.exec();
}
