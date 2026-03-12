#include <QPixmap>
#include "MockGitHubClient.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QDateTime>
#include <QTimer>

MockGitHubClient::MockGitHubClient(QObject *parent) : GitHubClient(parent) {
}

void MockGitHubClient::checkNotifications() {
    emit loadingStarted();
    QTimer::singleShot(100, this, [this]() {
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

        emit notificationsReceived(mockNotifications, false, false);
    });
}

void MockGitHubClient::loadMore() {
    // Mock no more data
}

void MockGitHubClient::verifyToken() {
    QTimer::singleShot(100, this, [this]() {
        emit tokenVerified(true, "Mock user");
    });
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
    QTimer::singleShot(100, this, [this, notificationId]() {
        emit detailsReceived(notificationId, "mock_author", "https://github.com/identicons/mock.png", "https://github.com/arran4/Kgithub-notify/pull/10");
    });
}

void MockGitHubClient::fetchImage(const QString &imageUrl, const QString &notificationId) {
    Q_UNUSED(imageUrl);
    QTimer::singleShot(100, this, [this, notificationId]() {
        QPixmap pm(32, 32);
        pm.fill(Qt::blue);
        emit imageReceived(notificationId, pm);
    });
}

void MockGitHubClient::requestRaw(const QString &endpoint, const QString &method, const QByteArray &body) {
    Q_UNUSED(method);
    Q_UNUSED(body);
    QTimer::singleShot(100, this, [this, endpoint]() {
        QJsonArray items;
        if (endpoint.contains("pulls")) {
            QJsonObject item;
            item["title"] = "Fix crash on startup";
            item["html_url"] = "https://github.com/arran4/Kgithub-notify/pull/10";
            item["created_at"] = QDateTime::currentDateTime().toString(Qt::ISODate);
            item["state"] = "open";
            QJsonObject user;
            user["login"] = "mock_author";
            item["user"] = user;
            items.append(item);
        } else if (endpoint.contains("issues")) {
            QJsonObject item;
            item["title"] = "Add feature X";
            item["html_url"] = "https://github.com/arran4/Kgithub-notify/issues/12";
            item["created_at"] = QDateTime::currentDateTime().toString(Qt::ISODate);
            item["state"] = "open";
            QJsonObject user;
            user["login"] = "mock_author";
            item["user"] = user;
            items.append(item);
        }

        QJsonObject root;
        root["items"] = items;

        QJsonDocument doc(root);
        emit rawDataReceived(doc.toJson());
    });
}

void MockGitHubClient::fetchUserRepos(const QString &pageUrl) {
    Q_UNUSED(pageUrl);
    QTimer::singleShot(100, this, [this]() {
        QJsonArray repos;
        QJsonObject repo;
        repo["full_name"] = "arran4/Kgithub-notify";
        repo["html_url"] = "https://github.com/arran4/Kgithub-notify";
        repo["description"] = "A sleek GitHub notification system tray application";
        repo["stargazers_count"] = 42;
        repos.append(repo);
        emit userReposReceived(repos, "");
    });
}

QNetworkRequest MockGitHubClient::createAuthenticatedRequest(const QUrl &url) const {
    // In the mock, we don't want real requests.
    // Return a dummy request to an invalid URL so it immediately fails or doesn't actually hit the network.
    // However, some code relies on the network reply.
    // We can just return a request for a known dummy URL or intercept it somehow.
    QNetworkRequest request;
    // We intentionally return a file url so we don't hit the real network and wait for timeouts.
    request.setUrl(QUrl("file:///dev/null"));
    return request;
}
