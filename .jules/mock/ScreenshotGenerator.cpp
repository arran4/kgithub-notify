#include <QApplication>
#include <KAboutData>
#include <QTimer>
#include <QDir>
#include <QDebug>
#include <KLocalizedString>
#include <QEventLoop>
#include <QPixmap>

#include "../../src/MainWindow.h"
#include "../../src/trending/TrendingWindow.h"
#include "../../src/RepoListWindow.h"
#include "../../src/WorkItemWindow.h"
#include "../../src/PullRequestWindow.h"
#include "../../src/ActionWindow.h"
#include "MockGitHubClient.h"

void saveScreenshot(QWidget* widget, const QString& filename) {
    widget->adjustSize();
    QEventLoop loop;
    QTimer::singleShot(2000, &loop, &QEventLoop::quit);
    loop.exec();

    QPixmap pixmap = widget->grab();
    QString path = QDir::currentPath() + "/docs/" + filename;
    QDir().mkpath(QDir::currentPath() + "/docs/");
    pixmap.save(path);
    qDebug() << "Saved screenshot to:" << path;
}

int main(int argc, char *argv[]) {
    QCoreApplication::setOrganizationName("arran4");
    QCoreApplication::setOrganizationDomain("arran4.com");
    QCoreApplication::setApplicationName("kgithub-notify-mock");
    QApplication app(argc, argv);
    KLocalizedString::setApplicationDomain("kgithub-notify");

    KAboutData aboutData(QStringLiteral("kgithub-notify-mock"), QStringLiteral("KGitHub Notify Mock"), QStringLiteral("0.1.0"));
    KAboutData::setApplicationData(aboutData);

    MockGitHubClient mockClient;

    // MainWindow
    MainWindow mainWindow;
    mainWindow.setClient(&mockClient);
    mainWindow.show();
    mockClient.checkNotifications(); // Populate mock notifications
    saveScreenshot(&mainWindow, "mainwindow.png");

    // TrendingWindow
    TrendingWindow trendingWindow(&mockClient);
    trendingWindow.show();
    saveScreenshot(&trendingWindow, "trendingwindow.png");

    // RepoListWindow
    RepoListWindow repoListWindow(&mockClient);
    repoListWindow.show();
    saveScreenshot(&repoListWindow, "repolistwindow.png");

    // WorkItemWindow
    WorkItemWindow workItemWindow(&mockClient, "Open Issues", WorkItemWindow::EndpointIssues, "is:open is:issue");
    workItemWindow.show();
    saveScreenshot(&workItemWindow, "workitemwindow.png");

    // We need dummy notifications for the detail windows
    Notification dummyPRNotification;
    dummyPRNotification.id = "10";
    dummyPRNotification.title = "Fix crash on startup";
    dummyPRNotification.type = "PullRequest";
    dummyPRNotification.url = "https://api.github.com/repos/arran4/Kgithub-notify/pulls/10";

    Notification dummyActionNotification;
    dummyActionNotification.id = "11";
    dummyActionNotification.title = "CI Build Failed";
    dummyActionNotification.type = "CheckSuite";
    dummyActionNotification.url = "https://api.github.com/repos/arran4/Kgithub-notify/actions/runs/123";

    // PullRequestWindow
    PullRequestWindow prWindow(dummyPRNotification, &mockClient);
    prWindow.show();
    saveScreenshot(&prWindow, "pullrequestwindow.png");

    // ActionWindow
    ActionWindow actionWindow(dummyActionNotification, &mockClient);
    actionWindow.show();
    saveScreenshot(&actionWindow, "actionwindow.png");

    qDebug() << "All screenshots generated successfully.";

    return 0;
}
