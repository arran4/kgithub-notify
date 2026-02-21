#include "GitHubClient.h"
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>

GitHubClient::GitHubClient(QObject *parent) : QObject(parent) {
    manager = new QNetworkAccessManager(this);
    connect(manager, &QNetworkAccessManager::finished, this, &GitHubClient::onReplyFinished);
    m_apiUrl = "https://api.github.com";
}

QString GitHubClient::apiToHtmlUrl(const QString &apiUrl, const QString &notificationId) {
    QString htmlUrl = apiUrl;
    htmlUrl.replace("api.github.com/repos", "github.com");
    htmlUrl.replace("/pulls/", "/pull/");
    htmlUrl.replace("/commits/", "/commit/");

    if (!notificationId.isEmpty()) {
        QUrl url(htmlUrl);
        QUrlQuery query(url.query());
        query.addQueryItem("notification_referrer_id", notificationId);
        url.setQuery(query);
        return url.toString();
    }
    return htmlUrl;
}

void GitHubClient::setToken(const QString &token) {
    m_token = token;
}

void GitHubClient::setApiUrl(const QString &url) {
    m_apiUrl = url;
}

void GitHubClient::checkNotifications() {
    if (m_token.isEmpty()) {
        emit authError("No token provided");
        return;
    }

    QUrl url(m_apiUrl + "/notifications");
    QNetworkRequest request = createRequest(url);

    manager->get(request);
}

void GitHubClient::verifyToken() {
    if (m_token.isEmpty()) {
        emit tokenVerified(false, "No token provided");
        return;
    }

    QUrl url(m_apiUrl + "/user");
    QNetworkRequest request = createRequest(url);

    manager->get(request);
}

void GitHubClient::markAsRead(const QString &id) {
    if (m_token.isEmpty()) return;

    QUrl url(m_apiUrl + "/notifications/threads/" + id);
    QNetworkRequest request = createRequest(url);

    manager->sendCustomRequest(request, "PATCH");
}

QNetworkRequest GitHubClient::createRequest(const QUrl &url) const {
    QNetworkRequest request(url);

    // Add Authorization header
    QString authHeader = "token " + m_token;
    request.setRawHeader("Authorization", authHeader.toUtf8());
    request.setRawHeader("Accept", "application/vnd.github.v3+json");

    // Add user-agent header as required by GitHub API
    request.setRawHeader("User-Agent", "Kgithub-notify");

    return request;
}

void GitHubClient::onReplyFinished(QNetworkReply *reply) {
    // Check for verification request
    if (reply->url().toString().endsWith("/user")) {
        if (reply->error() == QNetworkReply::NoError) {
             QByteArray data = reply->readAll();
             QJsonDocument doc = QJsonDocument::fromJson(data);
             if (doc.isObject()) {
                 QJsonObject obj = doc.object();
                 QString login = obj["login"].toString();
                 emit tokenVerified(true, "Token valid for user: " + login);
             } else {
                 emit tokenVerified(true, "Token valid");
             }
        } else if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 401) {
            emit tokenVerified(false, "Invalid Token");
        } else {
            emit tokenVerified(false, reply->errorString());
        }
        reply->deleteLater();
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 401) {
            emit authError("Invalid Token");
        } else {
            emit errorOccurred(reply->errorString());
        }
        reply->deleteLater();
        return;
    }

    // Check if this was a PATCH request (mark as read)
    if (reply->operation() == QNetworkAccessManager::CustomOperation && reply->request().attribute(QNetworkRequest::CustomVerbAttribute).toString() == "PATCH") {
        checkNotifications();
        reply->deleteLater();
        return;
    }

    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);

    if (!doc.isArray()) {
        emit errorOccurred("Invalid JSON response (expected array)");
        reply->deleteLater();
        return;
    }

    QJsonArray array = doc.array();

    QList<Notification> notifications;
    for (const QJsonValue &value : array) {
        if (!value.isObject()) continue;

        QJsonObject obj = value.toObject();
        Notification n;
        n.id = obj["id"].toString();

        QJsonObject subject = obj["subject"].toObject();
        n.title = subject["title"].toString();
        n.type = subject["type"].toString();
        n.url = subject["url"].toString(); // API URL
        n.htmlUrl = GitHubClient::apiToHtmlUrl(n.url);

        QJsonObject repo = obj["repository"].toObject();
        n.repository = repo["full_name"].toString();

        n.updatedAt = obj["updated_at"].toString();
        n.unread = obj["unread"].toBool();

        notifications.append(n);
    }

    emit notificationsReceived(notifications);
    reply->deleteLater();
}
