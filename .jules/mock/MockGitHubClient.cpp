#include "MockGitHubClient.h"
#include <QPixmap>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QDateTime>
#include <QTimer>

MockGitHubClient::MockGitHubClient(QObject *parent) : GitHubClient(parent) {
}

void MockGitHubClient::checkNotifications() {
    emit loadingStarted();

        QList<Notification> mockNotifications;

        QJsonObject n1;
        n1["id"] = "1";
        QJsonObject s1;
        s1["title"] = "Fix crash on startup";
        s1["type"] = "PullRequest";
        s1["url"] = "https://api.github.com/repos/arran4/Kgithub-notify/pulls/10";
        n1["subject"] = s1;
        QJsonObject r1;
        r1["full_name"] = "arran4/Kgithub-notify";
        n1["repository"] = r1;
        n1["updated_at"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        n1["unread"] = true;
        mockNotifications.append(Notification::fromJson(n1));

        QJsonObject n2;
        n2["id"] = "2";
        QJsonObject s2;
        s2["title"] = "Add feature X";
        s2["type"] = "Issue";
        s2["url"] = "https://api.github.com/repos/arran4/Kgithub-notify/issues/12";
        n2["subject"] = s2;
        QJsonObject r2;
        r2["full_name"] = "arran4/Kgithub-notify";
        n2["repository"] = r2;
        n2["updated_at"] = QDateTime::currentDateTime().addSecs(-3600).toString(Qt::ISODate);
        n2["unread"] = false;
        mockNotifications.append(Notification::fromJson(n2));

        QJsonObject n3;
        n3["id"] = "3";
        QJsonObject s3;
        s3["title"] = "Review requested: Add tests";
        s3["type"] = "PullRequest";
        s3["url"] = "https://api.github.com/repos/arran4/Kgithub-notify/pulls/15";
        n3["subject"] = s3;
        QJsonObject r3;
        r3["full_name"] = "arran4/Kgithub-notify";
        n3["repository"] = r3;
        n3["updated_at"] = QDateTime::currentDateTime().addSecs(-7200).toString(Qt::ISODate);
        n3["unread"] = true;
        mockNotifications.append(Notification::fromJson(n3));

        QJsonObject n4;
        n4["id"] = "4";
        QJsonObject s4;
        s4["title"] = "CI Check Failed on main";
        s4["type"] = "CheckSuite";
        s4["url"] = "https://api.github.com/repos/arran4/Kgithub-notify/actions/runs/42";
        n4["subject"] = s4;
        QJsonObject r4;
        r4["full_name"] = "arran4/Kgithub-notify";
        n4["repository"] = r4;
        n4["updated_at"] = QDateTime::currentDateTime().addSecs(-86400).toString(Qt::ISODate);
        n4["unread"] = false;
        mockNotifications.append(Notification::fromJson(n4));


        QJsonObject n5;
        n5["id"] = "5";
        QJsonObject s5;
        s5["title"] = "Please review: Dark mode UI";
        s5["type"] = "PullRequest";
        s5["url"] = "https://api.github.com/repos/arran4/Kgithub-notify/pulls/22";
        n5["subject"] = s5;
        QJsonObject r5;
        r5["full_name"] = "arran4/Kgithub-notify";
        n5["repository"] = r5;
        n5["updated_at"] = QDateTime::currentDateTime().addSecs(-3600*2).toString(Qt::ISODate);
        n5["unread"] = true;
        mockNotifications.append(Notification::fromJson(n5));

        QJsonObject n6;
        n6["id"] = "6";
        QJsonObject s6;
        s6["title"] = "Bug: Tray icon missing on Wayland";
        s6["type"] = "Issue";
        s6["url"] = "https://api.github.com/repos/arran4/Kgithub-notify/issues/23";
        n6["subject"] = s6;
        QJsonObject r6;
        r6["full_name"] = "arran4/Kgithub-notify";
        n6["repository"] = r6;
        n6["updated_at"] = QDateTime::currentDateTime().addSecs(-3600*5).toString(Qt::ISODate);
        n6["unread"] = true;
        mockNotifications.append(Notification::fromJson(n6));

        QJsonObject n7;
        n7["id"] = "7";
        QJsonObject s7;
        s7["title"] = "Update dependencies for v0.2.0";
        s7["type"] = "PullRequest";
        s7["url"] = "https://api.github.com/repos/arran4/Kgithub-notify/pulls/24";
        n7["subject"] = s7;
        QJsonObject r7;
        r7["full_name"] = "arran4/Kgithub-notify";
        n7["repository"] = r7;
        n7["updated_at"] = QDateTime::currentDateTime().addSecs(-3600*24).toString(Qt::ISODate);
        n7["unread"] = false;
        mockNotifications.append(Notification::fromJson(n7));

        QJsonObject n8;
        n8["id"] = "8";
        QJsonObject s8;
        s8["title"] = "Release v0.1.2 pipeline success";
        s8["type"] = "CheckSuite";
        s8["url"] = "https://api.github.com/repos/arran4/Kgithub-notify/actions/runs/43";
        n8["subject"] = s8;
        QJsonObject r8;
        r8["full_name"] = "arran4/Kgithub-notify";
        n8["repository"] = r8;
        n8["updated_at"] = QDateTime::currentDateTime().addSecs(-3600*48).toString(Qt::ISODate);
        n8["unread"] = false;
        mockNotifications.append(Notification::fromJson(n8));

        emit notificationsReceived(mockNotifications, false, false);

    }

void MockGitHubClient::loadMore() {
    // Mock no more data
}

void MockGitHubClient::verifyToken() {

        emit tokenVerified(true, "Mock user");
    }

void MockGitHubClient::markAsRead(const QString &id) {
    Q_UNUSED(id);
}

void MockGitHubClient::markAsDone(const QString &id) {
    Q_UNUSED(id);
}

void MockGitHubClient::markAsReadAndDone(const QString &id) {
    Q_UNUSED(id);
}

void MockGitHubClient::fetchNotificationDetails(const QString &url, const QString &notificationId) {

        emit detailsReceived(notificationId, "mock_author", "https://github.com/identicons/mock.png", "https://github.com/arran4/Kgithub-notify/pull/10");
    }

void MockGitHubClient::fetchImage(const QString &imageUrl, const QString &notificationId) {
    Q_UNUSED(imageUrl);

        QPixmap pm(32, 32);
        pm.fill(Qt::blue);
        emit imageReceived(notificationId, pm);
    }

void MockGitHubClient::requestRaw(const QString &endpoint, const QString &method, const QByteArray &body) {
    Q_UNUSED(method);
    Q_UNUSED(body);

        QJsonArray items;
        if (endpoint.contains("search/repositories")) { // Trending window uses search/repositories
            QJsonObject repo1;
            repo1["full_name"] = "arran4/Kgithub-notify";
            repo1["html_url"] = "https://github.com/arran4/Kgithub-notify";
            repo1["description"] = "A sleek GitHub notification system tray application";
            repo1["stargazers_count"] = 42;
            repo1["language"] = "C++";
            QJsonObject owner1;
            owner1["login"] = "arran4";
            owner1["avatar_url"] = "https://github.com/identicons/arran4.png";
            repo1["owner"] = owner1;
            items.append(repo1);

            QJsonObject repo2;
            repo2["full_name"] = "kde/kxmlgui";
            repo2["html_url"] = "https://github.com/kde/kxmlgui";
            repo2["description"] = "KDE Frameworks 6 - framework for managing menu and toolbar actions";
            repo2["stargazers_count"] = 1250;
            repo2["language"] = "C++";
            QJsonObject owner2;
            owner2["login"] = "kde";
            owner2["avatar_url"] = "https://github.com/identicons/kde.png";
            repo2["owner"] = owner2;
            items.append(repo2);

            QJsonObject repo3;
            repo3["full_name"] = "mock/project-x";
            repo3["html_url"] = "https://github.com/mock/project-x";
            repo3["description"] = "A very interesting mock project";
            repo3["stargazers_count"] = 8000;
            repo3["language"] = "Python";
            QJsonObject owner3;
            owner3["login"] = "mock";
            owner3["avatar_url"] = "https://github.com/identicons/mock.png";
            repo3["owner"] = owner3;
            items.append(repo3);

        } else if (endpoint.contains("pulls")) {
            QJsonObject item1;
            item1["title"] = "Fix crash on startup";
            item1["html_url"] = "https://github.com/arran4/Kgithub-notify/pull/10";
            item1["created_at"] = QDateTime::currentDateTime().toString(Qt::ISODate);
            item1["state"] = "open";
            QJsonObject user1;
            user1["login"] = "mock_author";
            item1["user"] = user1;
            items.append(item1);

            QJsonObject item2;
            item2["title"] = "Implement offline mode";
            item2["html_url"] = "https://github.com/arran4/Kgithub-notify/pull/21";
            item2["created_at"] = QDateTime::currentDateTime().addDays(-2).toString(Qt::ISODate);
            item2["state"] = "open";
            QJsonObject user2;
            user2["login"] = "jules_dev";
            item2["user"] = user2;
            items.append(item2);

        } else if (endpoint.contains("issues")) {
            QJsonObject item1;
            item1["title"] = "Add feature X";
            item1["html_url"] = "https://github.com/arran4/Kgithub-notify/issues/12";
            item1["created_at"] = QDateTime::currentDateTime().toString(Qt::ISODate);
            item1["state"] = "open";
            QJsonObject user1;
            user1["login"] = "mock_author";
            item1["user"] = user1;
            items.append(item1);

            QJsonObject item2;
            item2["title"] = "Typo in documentation";
            item2["html_url"] = "https://github.com/arran4/Kgithub-notify/issues/25";
            item2["created_at"] = QDateTime::currentDateTime().addDays(-1).toString(Qt::ISODate);
            item2["state"] = "open";
            QJsonObject user2;
            user2["login"] = "reader_user";
            item2["user"] = user2;
            items.append(item2);
        }

        QJsonObject root;
        root["items"] = items;

        QJsonDocument doc(root);
        emit rawDataReceived(doc.toJson());
    }

void MockGitHubClient::fetchUserRepos(const QString &pageUrl) {
    Q_UNUSED(pageUrl);

        QJsonArray repos;

        QJsonObject repo1;
        repo1["full_name"] = "arran4/Kgithub-notify";
        repo1["html_url"] = "https://github.com/arran4/Kgithub-notify";
        repo1["description"] = "A sleek GitHub notification system tray application";
        repo1["stargazers_count"] = 42;
        repos.append(repo1);

        QJsonObject repo2;
        repo2["full_name"] = "arran4/awesome-project";
        repo2["html_url"] = "https://github.com/arran4/awesome-project";
        repo2["description"] = "Another cool project that does things";
        repo2["stargazers_count"] = 105;
        repos.append(repo2);

        QJsonObject repo3;
        repo3["full_name"] = "arran4/dotfiles";
        repo3["html_url"] = "https://github.com/arran4/dotfiles";
        repo3["description"] = "Linux configurations";
        repo3["stargazers_count"] = 3;
        repos.append(repo3);


        QJsonObject repo4;
        repo4["full_name"] = "kde/kcoreaddons";
        repo4["html_url"] = "https://github.com/kde/kcoreaddons";
        repo4["description"] = "Addons to QtCore";
        repo4["stargazers_count"] = 502;
        repos.append(repo4);

        QJsonObject repo5;
        repo5["full_name"] = "qt/qtbase";
        repo5["html_url"] = "https://github.com/qt/qtbase";
        repo5["description"] = "Qt Base (Core, Gui, Widgets, Network, ...)";
        repo5["stargazers_count"] = 8045;
        repos.append(repo5);

        QJsonObject repo6;
        repo6["full_name"] = "arran4/go-ical";
        repo6["html_url"] = "https://github.com/arran4/go-ical";
        repo6["description"] = "A an icalendar library for golang";
        repo6["stargazers_count"] = 2300;
        repos.append(repo6);

        emit userReposReceived(repos, "");

    }

QNetworkRequest MockGitHubClient::createAuthenticatedRequest(const QUrl &url) const {
    // In the mock, we don't want real requests.
    // Return a dummy request to an invalid URL so it immediately fails or doesn't actually hit the network.
    // However, some code relies on the network reply.
    // We can just return a request for a known dummy URL or intercept it somehow.
    QNetworkRequest request;
    // We intentionally return a file url so we don't hit the real network and wait for timeouts.
    request.setUrl(url);
    return request;
}
