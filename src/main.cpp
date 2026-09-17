#include <KAboutData>
#include <KLocalizedString>
#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMessageBox>
#include <QStandardPaths>
#include <QTimer>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusError>

#include "GitHubClient.h"
#include "MainWindow.h"
#include "utils/DesktopEntryHelper.h"

#ifndef KGHN_APP_VERSION
#define KGHN_APP_VERSION "dev"
#endif

int main(int argc, char* argv[]) {
    QCoreApplication::setOrganizationName("arran4");
    QCoreApplication::setOrganizationDomain("arran4.com");
    QCoreApplication::setApplicationName("kgithub-notify");
    QCoreApplication::setApplicationVersion(QStringLiteral(KGHN_APP_VERSION));
    QGuiApplication::setDesktopFileName("com.arran4.kgithub_notify");
    QApplication::setQuitOnLastWindowClosed(false);

    QApplication app(argc, argv);
    QApplication::setWindowIcon(QIcon::fromTheme("kgithub-notify", QIcon(":/assets/icon.png")));

    KLocalizedString::setApplicationDomain("kgithub-notify");

    KAboutData aboutData(QStringLiteral("kgithub-notify"), QStringLiteral("KGitHub Notify"),
                         QStringLiteral(KGHN_APP_VERSION));
    aboutData.setDesktopFileName("com.arran4.kgithub_notify");
    KAboutData::setApplicationData(aboutData);
    QGuiApplication::setDesktopFileName("com.arran4.kgithub_notify");

    QCommandLineParser parser;
    parser.setApplicationDescription("GitHub Notification System Tray");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption backgroundOption(
        QStringList() << "b" << "background",
        QCoreApplication::translate("main", "Start in the background (system tray only)."));
    parser.addOption(backgroundOption);

    QCommandLineOption diagnoseOption(QStringList() << QStringLiteral("diagnose"),
                                      QCoreApplication::translate("main", "Run self-diagnostics and exit."));
    parser.addOption(diagnoseOption);

    QCommandLineOption registerOption(QStringList() << QStringLiteral("register-desktop"),
                                      QCoreApplication::translate("main", "Register desktop entry for current executable."));
    parser.addOption(registerOption);

    parser.process(app);

    // Check for desktop file to warn about potential portal issues
    QString desktopFileName = QGuiApplication::desktopFileName() + QStringLiteral(".desktop");
    QStringList appPaths = QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation);

    if (parser.isSet(registerOption)) {
        QString err;
        if (!DesktopEntryHelper::registerDesktopEntry(QString(), true /* overwrite */, &err)) {
            qCritical() << "Failed to register desktop file:" << err;
            return 1;
        }
        qInfo() << "Successfully registered desktop file for current executable.";
        return 0;
    }

    if (parser.isSet(diagnoseOption)) {
        qDebug() << "=== KGitHub Notify Diagnostics ===";
        qDebug() << "App Name:" << QCoreApplication::applicationName();
        qDebug() << "Desktop File Name:" << QGuiApplication::desktopFileName();
        qDebug() << "Expected Executable:" << QCoreApplication::applicationFilePath();
        qDebug() << "Standard Applications Paths:" << appPaths;

        DesktopEntryDiagnosis diag = DesktopEntryHelper::diagnose(desktopFileName, appPaths);
        if (diag.status == DesktopEntryStatus::ValidUsable) {
            qDebug() << "Desktop File Status: [FOUND - VALID]";
            qDebug() << "Path:" << diag.foundPath;
            qDebug() << "Usable: YES";
        } else if (diag.status == DesktopEntryStatus::PresentMismatched) {
            qDebug() << "Desktop File Status: [FOUND - MISMATCHED]";
            qDebug() << "Path:" << diag.foundPath;
            qDebug() << "Current Exec:" << diag.currentExecInEntry;
            qDebug() << "Reason:" << diag.reason;
            qDebug() << "Usable: YES";
        } else if (diag.status == DesktopEntryStatus::PresentUnusable) {
            qDebug() << "Desktop File Status: [FOUND - UNUSABLE]";
            qDebug() << "Path:" << diag.foundPath;
            qDebug() << "Reason:" << diag.reason;
            qDebug() << "Usable: NO";
        } else {
            qDebug() << "Desktop File Status: [MISSING]";
            qDebug() << "Reason:" << diag.reason;
            qDebug() << "Usable: NO";
            qDebug() << "  -> Ensure" << desktopFileName
                     << "is installed to one of the above paths or run with --register-desktop.";
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

    MainWindow window;
    GitHubClient client;

    window.setClient(&client);

    DesktopEntryDiagnosis diag = DesktopEntryHelper::diagnose(desktopFileName, appPaths);
    if (!diag.isUsable) {
        qWarning() << "Warning: Desktop file" << desktopFileName << "not found or not usable in standard locations.";
        qWarning() << "Reason:" << diag.reason;
        qWarning() << "System tray and notifications may not work correctly with portals.";

        window.showDesktopFileWarning(desktopFileName, appPaths);
    }

    if (!parser.isSet(backgroundOption)) {
        window.show();
    }

    return app.exec();
}
