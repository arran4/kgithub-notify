#include <QApplication>
#include <QTimer>

#include "GitHubClient.h"
#include "MainWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setOrganizationName("Kgithub-notify");
    QApplication::setApplicationName("Kgithub-notify");
    QApplication::setQuitOnLastWindowClosed(false);

    MainWindow window;
    GitHubClient client;

    window.setClient(&client);

    // Initial check
    client.checkNotifications();

    return app.exec();
}
