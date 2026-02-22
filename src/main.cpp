#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QTimer>

#include "GitHubClient.h"
#include "MainWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setOrganizationName("Kgithub-notify");
    QApplication::setApplicationName("Kgithub-notify");
    QApplication::setQuitOnLastWindowClosed(false);

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

    // Initial check
    client.checkNotifications();

    return app.exec();
}
