#include <QApplication>
#include <QTimer>
#include "MainWindow.h"
#include "GitHubClient.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setQuitOnLastWindowClosed(false);

    MainWindow window;
    GitHubClient client;

    window.setClient(&client);

    // Initial check
    client.checkNotifications();

    // Timer for polling
    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, &client, &GitHubClient::checkNotifications);
    timer.start(5 * 60 * 1000); // 5 minutes

    return app.exec();
}
