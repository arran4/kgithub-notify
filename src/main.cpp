#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDebug>
#include <QFileInfo>
#include <QGuiApplication>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusError>
#include <QStandardPaths>
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

    QCommandLineOption diagnoseOption(
        QStringList() << "diagnose",
        QCoreApplication::translate("main", "Run self-diagnostics and exit."));
    parser.addOption(diagnoseOption);

    parser.process(app);

    // Check for desktop file to warn about potential portal issues
    QString desktopFileName = QGuiApplication::desktopFileName() + ".desktop";
    bool desktopFileFound = false;
    QStringList appPaths = QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation);
    for (const QString &path : appPaths) {
        if (QFileInfo::exists(path + "/" + desktopFileName)) {
            desktopFileFound = true;
            break;
        }
    }

    if (parser.isSet(diagnoseOption)) {
        qDebug() << "=== KGitHub Notify Diagnostics ===";
        qDebug() << "App Name:" << QCoreApplication::applicationName();
        qDebug() << "Desktop File Name:" << QGuiApplication::desktopFileName();
        qDebug() << "Looking for Desktop File:" << desktopFileName;

        qDebug() << "Standard Applications Paths:" << appPaths;

        if (desktopFileFound) {
            qDebug() << "Desktop File Status: [FOUND]";
        } else {
            qDebug() << "Desktop File Status: [MISSING]";
            qDebug() << "  -> Ensure" << desktopFileName << "is installed to one of the above paths.";
        }

        if (QDBusConnection::sessionBus().isConnected()) {
            qDebug() << "DBus Session Bus: [CONNECTED]";
            qDebug() << "DBus Unique Name:" << QDBusConnection::sessionBus().baseService();
        } else {
             qDebug() << "DBus Session Bus: [DISCONNECTED]";
             qDebug() << "  -> Error:" << QDBusConnection::sessionBus().lastError().message();
        }

        return 0;
    }

    if (!desktopFileFound) {
        qWarning() << "Warning: Desktop file" << desktopFileName << "not found in standard locations.";
        qWarning() << "System tray and notifications may not work correctly with portals.";
        qWarning() << "Ensure" << desktopFileName << "is installed to" << appPaths.first();
    }

    MainWindow window;
    GitHubClient client;

    window.setClient(&client);

    if (!parser.isSet(backgroundOption)) {
        window.show();
    }

    return app.exec();
}
