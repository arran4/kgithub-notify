#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QTimer>

#include "GitHubClient.h"
#include "MainWindow.h"

#ifndef KGHN_APP_VERSION
#define KGHN_APP_VERSION "dev"
#endif

int main(int argc, char *argv[]) {
    QCoreApplication::setOrganizationName("Kgithub-notify");
    QCoreApplication::setApplicationName("kgithub-notify");
    QCoreApplication::setApplicationVersion(QStringLiteral(KGHN_APP_VERSION));
    QGuiApplication::setDesktopFileName("org.kgithub_notify");
    QApplication::setQuitOnLastWindowClosed(false);

    QApplication app(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription("GitHub Notification System Tray");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption backgroundOption(
        QStringList() << "b" << "background",
        QCoreApplication::translate("main", "Start in the background (system tray only)."));
    parser.addOption(backgroundOption);

    parser.process(app);

    MainWindow window;
    GitHubClient client;

    window.setClient(&client);

    if (!parser.isSet(backgroundOption)) {
        window.show();
    }

    return app.exec();
}
