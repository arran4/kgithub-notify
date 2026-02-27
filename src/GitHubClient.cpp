#include "GitHubClient.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QNetworkRequest>
#include <QPixmap>
#include <QUrl>
#include <QUrlQuery>
#include <QRegularExpression>

GitHubClient::GitHubClient(QObject *parent) : QObject(parent) {
    manager = new QNetworkAccessManager(this);
    connect(manager, &QNetworkAccessManager::finished, this, &GitHubClient::onReplyFinished);
    m_apiUrl = "https://api.github.com";
    m_pendingPatchRequests = 0;
    m_showAll = false;
    m_nextPageUrl = "";
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

void GitHubClient::setToken(const QString &token) { m_token.set(token); }

void GitHubClient::setApiUrl(const QString &url) { m_apiUrl = url; }

void GitHubClient::setShowAll(bool all) { m_showAll = all; }

void GitHubClient::checkNotifications() {
    emit loadingStarted();

    m_nextPageUrl.clear();

    if (m_token.isEmpty()) {
        emit authError("No token provided");
        return;
    }

    QUrl url(m_apiUrl + "/notifications");
    // Always fetch all notifications (including read ones) to support client-side filtering
    QUrlQuery query;
    query.addQueryItem("all", "true");
    url.setQuery(query);

    QNetworkRequest request = createRequest(url);

    if (m_activeNotificationReply) {
        m_activeNotificationReply->abort();
    }

    QNetworkReply *reply = manager->get(request);
    reply->setProperty("type", "notifications");
    reply->setProperty("append", false);
    m_activeNotificationReply = reply;
}

void GitHubClient::loadMore() {
    if (m_nextPageUrl.isEmpty()) return;

    emit loadingStarted();

    if (m_activeNotificationReply) {
        m_activeNotificationReply->abort();
    }

    QUrl url(m_nextPageUrl);
    QNetworkRequest request = createRequest(url);

    QNetworkReply *reply = manager->get(request);
    reply->setProperty("type", "notifications");
    reply->setProperty("append", true);
    m_activeNotificationReply = reply;
}

void GitHubClient::verifyToken() {
    if (m_token.isEmpty()) {
        emit tokenVerified(false, "No token provided");
        return;
    }

    QUrl url(m_apiUrl + "/user");
    QNetworkRequest request = createRequest(url);

    QNetworkReply *reply = manager->get(request);
    reply->setProperty("type", "verification");
}

void GitHubClient::markAsRead(const QString &id) {
    if (m_token.isEmpty()) return;

    m_pendingPatchRequests++;

    QUrl url(m_apiUrl + "/notifications/threads/" + id);
    QNetworkRequest request = createRequest(url);

    QNetworkReply *reply = manager->sendCustomRequest(request, "PATCH");
    reply->setProperty("type", "patch");
}

void GitHubClient::markAsDone(const QString &id) {
    if (m_token.isEmpty()) return;

    m_pendingPatchRequests++;

    QUrl url(m_apiUrl + "/notifications/threads/" + id);
    QNetworkRequest request = createRequest(url);

    QNetworkReply *reply = manager->deleteResource(request);
    reply->setProperty("type", "delete");
}

void GitHubClient::markAsReadAndDone(const QString &id) {
    if (m_token.isEmpty()) return;

    m_pendingPatchRequests++;

    QUrl url(m_apiUrl + "/notifications/threads/" + id);
    QNetworkRequest request = createRequest(url);

    QNetworkReply *reply = manager->sendCustomRequest(request, "PATCH");
    reply->setProperty("type", "read_and_done");
    reply->setProperty("notificationId", id);
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

void GitHubClient::requestRaw(const QString &endpoint, const QString &method, const QByteArray &body) {
    if (m_token.isEmpty()) return;
    QString urlStr = endpoint.startsWith("http") ? endpoint : m_apiUrl + endpoint;
    QUrl url(urlStr);
    QNetworkRequest request = createRequest(url);

    if (!body.isEmpty()) {
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    }

    QNetworkReply *reply = nullptr;
    QString m = method.toUpper();

    if (m == "GET") {
        reply = manager->get(request);
    } else if (m == "POST") {
        reply = manager->post(request, body);
    } else if (m == "PUT") {
        reply = manager->put(request, body);
    } else if (m == "DELETE") {
        reply = manager->deleteResource(request);
    } else {
        reply = manager->sendCustomRequest(request, method.toUtf8(), body);
    }

    if (reply) {
        reply->setProperty("type", "raw");
    }
}

QNetworkRequest GitHubClient::createAuthenticatedRequest(const QUrl &url) const {
    return createRequest(url);
}

QNetworkRequest GitHubClient::createRequest(const QUrl &url) const {
    QNetworkRequest request(url);

    // Add Authorization header
    QByteArray authHeader = "token ";
    QByteArray tokenBytes = m_token.toQByteArray();
    authHeader.append(tokenBytes);

    request.setRawHeader("Authorization", authHeader);
    request.setRawHeader("Accept", "application/vnd.github.v3+json");

    // Explicitly zero out sensitive data
    if (!tokenBytes.isEmpty()) {
        volatile char *p = tokenBytes.data();
        size_t s = tokenBytes.size();
        while (s--) *p++ = 0;
    }
    if (!authHeader.isEmpty()) {
        volatile char *p = authHeader.data();
        size_t s = authHeader.size();
        while (s--) *p++ = 0;
    }

    // Add user-agent header as required by GitHub API
    request.setRawHeader("User-Agent", "Kgithub-notify");

    return request;
}

void GitHubClient::onReplyFinished(QNetworkReply *reply) {
    if (reply == m_activeNotificationReply) {
        m_activeNotificationReply = nullptr;
    }

    if (reply->error() == QNetworkReply::OperationCanceledError) {
        reply->deleteLater();
        return;
    }

    QString type = reply->property("type").toString();

    if (type == "details") {
        handleDetailsReply(reply);
    } else if (type == "image") {
        handleImageReply(reply);
    } else if (type == "verification") {
        handleVerificationReply(reply);
    } else if (type == "raw") {
        if (reply->error() == QNetworkReply::NoError) {
            emit rawDataReceived(reply->readAll());
        } else {
            emit rawDataReceived(reply->errorString().toUtf8());
        }
    } else if (type == "patch" || type == "delete") {
        handlePatchReply(reply);
    } else if (type == "read_and_done") {
        QString id = reply->property("notificationId").toString();
        m_pendingPatchRequests--;
        if (m_pendingPatchRequests < 0) m_pendingPatchRequests = 0;

        if (reply->error() == QNetworkReply::NoError) {
            markAsDone(id);
        } else {
            if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 401) {
                emit authError("Invalid Token");
            } else {
                emit errorOccurred(reply->errorString());
            }
        }

        if (m_pendingPatchRequests == 0) {
            checkNotifications();
        }
    } else if (type == "notifications") {
        handleNotificationsReply(reply);
    } else {
        qDebug() << "Unknown reply type:" << type;
    }

    reply->deleteLater();
}

void GitHubClient::handleDetailsReply(QNetworkReply *reply) {
    QString notificationId = reply->property("notificationId").toString();
    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "Error fetching details:" << reply->errorString();
        emit detailsError(notificationId, reply->errorString());
        return;
    }

    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isObject()) {
        QJsonObject obj = doc.object();
        QString authorName;
        QString avatarUrl;

        if (obj.contains("user")) {
            QJsonObject user = obj["user"].toObject();
            authorName = user["login"].toString();
            avatarUrl = user["avatar_url"].toString();
        } else if (obj.contains("author")) {
            QJsonObject author = obj["author"].toObject();
            authorName = author["login"].toString();
            avatarUrl = author["avatar_url"].toString();
        }

        QString htmlUrl = obj["html_url"].toString();

        emit detailsReceived(notificationId, authorName, avatarUrl, htmlUrl);
    }
}

void GitHubClient::handleImageReply(QNetworkReply *reply) {
    QString notificationId = reply->property("notificationId").toString();
    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "Error fetching image:" << reply->errorString();
        return;
    }

    QByteArray data = reply->readAll();
    QPixmap pixmap;
    if (pixmap.loadFromData(data)) {
        emit imageReceived(notificationId, pixmap);
    }
}

void GitHubClient::handleVerificationReply(QNetworkReply *reply) {
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
}

void GitHubClient::handlePatchReply(QNetworkReply *reply) {
    m_pendingPatchRequests--;
    if (m_pendingPatchRequests < 0) {
        m_pendingPatchRequests = 0;
    }

    if (reply->error() != QNetworkReply::NoError) {
        if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 401) {
            emit authError("Invalid Token");
        } else {
            emit errorOccurred(reply->errorString());
        }
        return;
    }

    if (m_pendingPatchRequests == 0) {
        checkNotifications();
    }
}

void GitHubClient::handleNotificationsReply(QNetworkReply *reply) {
    if (reply->error() != QNetworkReply::NoError) {
        if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 401) {
            emit authError("Invalid Token");
        } else {
            emit errorOccurred(reply->errorString());
        }
        return;
    }

    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);

    if (!doc.isArray()) {
        emit errorOccurred("Invalid JSON response (expected array)");
        return;
    }

    QJsonArray array = doc.array();

    QList<Notification> notifications;
    for (const QJsonValue &value : array) {
        if (!value.isObject()) continue;

        QJsonObject obj = value.toObject();
        Notification n;
        n.id = obj["id"].toVariant().toString();

        QJsonObject subject = obj["subject"].toObject();
        n.title = subject["title"].toString();
        n.type = subject["type"].toString();
        n.url = subject["url"].toString();  // API URL
        n.htmlUrl = GitHubClient::apiToHtmlUrl(n.url);

        QJsonObject repo = obj["repository"].toObject();
        n.repository = repo["full_name"].toString();

        n.updatedAt = obj["updated_at"].toString();
        n.lastReadAt = obj["last_read_at"].toString();
        n.inInbox = obj["unread"].toBool();

        if (n.lastReadAt.isEmpty()) {
            n.unread = true;
        } else {
            QDateTime updated = QDateTime::fromString(n.updatedAt, Qt::ISODate);
            QDateTime lastRead = QDateTime::fromString(n.lastReadAt, Qt::ISODate);
            if (updated > lastRead) {
                n.unread = true;
            } else {
                n.unread = false;
            }
        }

        notifications.append(n);
    }

    // Parse Link header
    m_nextPageUrl.clear();
    if (reply->hasRawHeader("Link")) {
        QString linkHeader = reply->rawHeader("Link");
        // Example: <https://api.github.com/resource?page=2>; rel="next", <https://api.github.com/resource?page=5>; rel="last"
        QRegularExpression re("<([^>]+)>;\\s*rel=\"next\"");
        QRegularExpressionMatch match = re.match(linkHeader);
        if (match.hasMatch()) {
            m_nextPageUrl = match.captured(1);
        }
    }

    bool append = reply->property("append").toBool();
    emit notificationsReceived(notifications, append, !m_nextPageUrl.isEmpty());
}
