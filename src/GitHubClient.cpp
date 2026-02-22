#include "GitHubClient.h"
#include "GitHubResponseParser.h"
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QPixmap>

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

void GitHubClient::fetchNotificationDetails(const QString &url, const QString &notificationId) {
    if (m_token.isEmpty()) return;
    QUrl qUrl(url);
    QNetworkRequest request = createRequest(qUrl);
    QNetworkReply *reply = manager->get(request);
    reply->setProperty("type", "details");
    reply->setProperty("notificationId", notificationId);
}

void GitHubClient::fetchImage(const QString &imageUrl, const QString &notificationId) {
    QUrl qUrl(imageUrl);
    QNetworkRequest request(qUrl);
    // Images (avatars) are usually public, so no auth header needed.
    // Also, User-Agent is good practice.
    request.setRawHeader("User-Agent", "Kgithub-notify");

    QNetworkReply *reply = manager->get(request);
    reply->setProperty("type", "image");
    reply->setProperty("notificationId", notificationId);
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
    QString type = reply->property("type").toString();
    QString notificationId = reply->property("notificationId").toString();
    QByteArray data = reply->readAll();
    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QString errorString = reply->errorString();

    if (!type.isEmpty()) {
        if (reply->error() != QNetworkReply::NoError) {
            qDebug() << "Error fetching" << type << ":" << errorString;
            reply->deleteLater();
            return;
        }

        if (type == "details") {
            QString parseError;
            ParsedNotificationDetails details = GitHubResponseParser::parseDetails(data, parseError);
            if (parseError.isEmpty()) {
                emit detailsReceived(notificationId, details.authorName, details.avatarUrl, details.htmlUrl);
            }
        } else if (type == "image") {
            QPixmap pixmap;
            if (pixmap.loadFromData(data)) {
                emit imageReceived(notificationId, pixmap);
            }
        }

        reply->deleteLater();
        return;
    }

    // Check for verification request
    // This logic needs to be careful because "checkNotifications" doesn't have a distinct "type".
    // We infer it from the URL or lack of "type".
    // However, verifyToken calls /user, which we can distinguish.
    if (reply->url().toString().endsWith("/user")) {
        ParsedVerification result = GitHubResponseParser::parseUserVerification(data, statusCode, errorString);
        emit tokenVerified(result.valid, result.message);
        reply->deleteLater();
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        if (statusCode == 401) {
            emit authError("Invalid Token");
        } else {
            emit errorOccurred(errorString);
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

    // Parse notifications
    QString parseError;
    QList<Notification> notifications = GitHubResponseParser::parseNotifications(data, parseError);
    if (!parseError.isEmpty()) {
        emit errorOccurred(parseError);
    } else {
        emit notificationsReceived(notifications);
    }

    reply->deleteLater();
}
