#include "GitHubClient.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QNetworkRequest>
#include <QPixmap>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QUrl>
#include <QUrlQuery>

GitHubClient::GitHubClient(QObject* parent) : QObject(parent) {
    manager = new QNetworkAccessManager(this);
    connect(manager, &QNetworkAccessManager::finished, this, &GitHubClient::onReplyFinished);
    m_apiUrl = "https://api.github.com";
    m_pendingPatchRequests = 0;
    m_showAll = false;
    m_nextPageUrl = "";
}

QString GitHubClient::apiToHtmlUrl(const QString& apiUrl, const QString& notificationId) {
    QString htmlUrl = apiUrl;
    htmlUrl.replace("api.github.com/repos", "github.com");
    htmlUrl.replace("/pulls/", "/pull/");
    htmlUrl.replace("/commits/", "/commit/");

    if (!notificationId.isEmpty() && !htmlUrl.isEmpty()) {
        QUrl url(htmlUrl);
        QUrlQuery query(url.query());
        query.addQueryItem("notification_referrer_id", notificationId);
        url.setQuery(query);
        return url.toString();
    }
    return htmlUrl;
}

void GitHubClient::setToken(const QString& token) { m_token.set(token); }

void GitHubClient::setApiUrl(const QString& url) { m_apiUrl = url; }

void GitHubClient::setShowAll(bool all) { m_showAll = all; }

QUuid GitHubClient::checkNotifications(QUuid reqId) {
    if (reqId.isNull()) reqId = QUuid::createUuid();
    emit loadingStarted(reqId);

    if (m_token.isEmpty()) return reqId;

    QUrl url(m_apiUrl + "/notifications");
    QUrlQuery query;
    if (m_showAll) {
        query.addQueryItem("all", "true");
    }
    url.setQuery(query);

    QNetworkRequest request = createAuthenticatedRequest(url);

    QNetworkReply* reply = manager->get(request);
    reply->setProperty("reqId", reqId);
    reply->setProperty("type", "notifications");
    reply->setProperty("append", false);
    m_nextPageUrl.clear();
    return reqId;
}

QUuid GitHubClient::loadMore(QUuid reqId) {
    if (reqId.isNull()) reqId = QUuid::createUuid();
    if (m_nextPageUrl.isEmpty()) return reqId;

    emit loadingStarted(reqId);

    QUrl url(m_nextPageUrl);

    if (!isTrustedApiOrigin(url)) {
        m_nextPageUrl.clear();
        emit notificationsReceived(reqId, QList<Notification>(), true, false);
        emit errorOccurred(reqId, "Untrusted pagination URL rejected.");
        return reqId;
    }

    QNetworkRequest request = createAuthenticatedRequest(url);

    QNetworkReply* reply = manager->get(request);
    reply->setProperty("reqId", reqId);
    reply->setProperty("type", "notifications");
    reply->setProperty("append", true);
    return reqId;
}

QUuid GitHubClient::verifyToken(QUuid reqId) {
    if (reqId.isNull()) reqId = QUuid::createUuid();
    if (m_token.isEmpty()) {
        emit tokenVerified(reqId, false, "No token provided");
        return reqId;
    }

    QUrl url(m_apiUrl + "/user");
    QNetworkRequest request = createAuthenticatedRequest(url);

    QNetworkReply* reply = manager->get(request);
    reply->setProperty("reqId", reqId);
    reply->setProperty("type", "verification");
    return reqId;
}

QUuid GitHubClient::markAsRead(const QString& id, QUuid reqId) {
    if (reqId.isNull()) reqId = QUuid::createUuid();
    if (m_token.isEmpty()) return reqId;

    m_pendingPatchRequests++;

    QUrl url(m_apiUrl + "/notifications/threads/" + id);
    QNetworkRequest request = createAuthenticatedRequest(url);

    QNetworkReply* reply = manager->sendCustomRequest(request, "PATCH");
    reply->setProperty("type", "patch");
    return reqId;
}

QUuid GitHubClient::markAsDone(const QString& id, QUuid reqId) {
    if (reqId.isNull()) reqId = QUuid::createUuid();
    if (m_token.isEmpty()) return reqId;

    m_pendingPatchRequests++;

    QUrl url(m_apiUrl + "/notifications/threads/" + id);
    QNetworkRequest request = createAuthenticatedRequest(url);

    QNetworkReply* reply = manager->deleteResource(request);
    reply->setProperty("type", "delete");
    return reqId;
}

QUuid GitHubClient::markAsReadAndDone(const QString& id, QUuid reqId) {
    if (reqId.isNull()) reqId = QUuid::createUuid();
    if (m_token.isEmpty()) return reqId;

    m_pendingPatchRequests++;

    QUrl url(m_apiUrl + "/notifications/threads/" + id);
    QNetworkRequest request = createAuthenticatedRequest(url);

    QNetworkReply* reply = manager->sendCustomRequest(request, "PATCH");
    reply->setProperty("type", "read_and_done");
    reply->setProperty("notificationId", id);
    return reqId;
}

QUuid GitHubClient::fetchNotificationDetails(const QString& url, const QString& notificationId, QUuid reqId) {
    if (m_token.isEmpty() || url.isEmpty()) return reqId;
    QUrl qUrl(url);
    if (!qUrl.isValid()) return reqId;

    if (!isTrustedApiOrigin(qUrl)) {
        emit errorOccurred(reqId, "Untrusted notification details URL rejected.");
        return reqId;
    }

    QNetworkRequest request = createRequest(qUrl);
    QNetworkReply* reply = manager->get(request);
    reply->setProperty("type", "details");
    reply->setProperty("notificationId", notificationId);
    return reqId;
}

QUuid GitHubClient::fetchImage(const QString& imageUrl, const QString& notificationId, QUuid reqId) {
    if (reqId.isNull()) reqId = QUuid::createUuid();
    if (imageUrl.isEmpty()) return reqId;

    QUrl url(imageUrl);
    QNetworkRequest request = createRequest(url);
    QNetworkReply* reply = manager->get(request);
    reply->setProperty("reqId", reqId);
    reply->setProperty("type", "image");
    reply->setProperty("notificationId", notificationId);
    return reqId;
}

QUuid GitHubClient::requestRaw(const QString& endpoint, const QString& method, const QByteArray& body, QUuid reqId) {
    if (reqId.isNull()) reqId = QUuid::createUuid();
    if (m_token.isEmpty()) return reqId;

    QString urlStr = endpoint;
    if (!urlStr.startsWith("http")) {
        if (!urlStr.startsWith("/")) urlStr = "/" + urlStr;
        urlStr = m_apiUrl + urlStr;
    }

    QUrl url(urlStr);
    if (!isTrustedApiOrigin(url)) {
        emit rawDataReceived(reqId, "Error: Untrusted external destination for authenticated request.");
        return reqId;
    }

    QNetworkRequest request = createAuthenticatedRequest(url);
    QNetworkReply* reply = nullptr;

    if (method.toUpper() == "GET") {
        reply = manager->get(request);
    } else if (method.toUpper() == "POST") {
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        reply = manager->post(request, body);
    } else if (method.toUpper() == "PATCH") {
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        reply = manager->sendCustomRequest(request, "PATCH", body);
    } else if (method.toUpper() == "PUT") {
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        reply = manager->put(request, body);
    } else if (method.toUpper() == "DELETE") {
        reply = manager->deleteResource(request);
    }

    if (reply) {
        reply->setProperty("reqId", reqId);
        reply->setProperty("type", "raw");
    }
    return reqId;
}

QUuid GitHubClient::fetchUserRepos(const QString& pageUrl, QUuid reqId) {
    if (reqId.isNull()) reqId = QUuid::createUuid();
    if (m_token.isEmpty()) return reqId;

    QUrl url;
    if (pageUrl.isEmpty()) {
        url = QUrl(m_apiUrl + "/user/repos");
        QUrlQuery query;
        query.addQueryItem("per_page", "100");
        query.addQueryItem("sort", "updated");
        url.setQuery(query);
    } else {
        url = QUrl(pageUrl);
    }

    if (!isTrustedApiOrigin(url)) {
        emit errorOccurred(reqId, "Untrusted repository pagination URL rejected.");
        return reqId;
    }

    QNetworkRequest request = createAuthenticatedRequest(url);
    QNetworkReply* reply = manager->get(request);
    reply->setProperty("type", "repos");
    return reqId;
}

bool GitHubClient::isTrustedApiOrigin(const QUrl& url) const {
    QUrl apiOrigin(m_apiUrl);

    // Compare scheme
    if (url.scheme().compare(apiOrigin.scheme(), Qt::CaseInsensitive) != 0) {
        return false;
    }

    // Compare host
    if (url.host().compare(apiOrigin.host(), Qt::CaseInsensitive) != 0) {
        return false;
    }

    // Compare port
    int urlPort = url.port(url.scheme() == "https" ? 443 : 80);
    int apiPort = apiOrigin.port(apiOrigin.scheme() == "https" ? 443 : 80);

    return urlPort == apiPort;
}

QNetworkRequest GitHubClient::createAuthenticatedRequest(const QUrl& url) const { return createRequest(url); }

QNetworkRequest GitHubClient::createRequest(const QUrl& url) const {
    QNetworkRequest request(url);
    request.setTransferTimeout(30000);

    if (isTrustedApiOrigin(url)) {
        // Add Authorization header only for trusted origins
        QByteArray authHeader = "token ";
        QByteArray tokenBytes = m_token.toQByteArray();
        authHeader.append(tokenBytes);

        request.setRawHeader("Authorization", authHeader);

        // Explicitly zero out sensitive data
        if (!tokenBytes.isEmpty()) {
            volatile char* p = tokenBytes.data();
            size_t s = tokenBytes.size();
            while (s--) *p++ = 0;
        }
        if (!authHeader.isEmpty()) {
            volatile char* p = authHeader.data();
            size_t s = authHeader.size();
            while (s--) *p++ = 0;
        }
    }

    request.setRawHeader("Accept", "application/vnd.github.v3+json");

    // Add user-agent header as required by GitHub API
    request.setRawHeader("User-Agent", "Kgithub-notify");

    // Check redirect behavior so credentials cannot leak to a different origin
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::SameOriginRedirectPolicy);

    return request;
}

void GitHubClient::onReplyFinished(QNetworkReply* reply) {
    if (reply->error() == QNetworkReply::OperationCanceledError) {
        reply->deleteLater();
        return;
    }

    QString type = reply->property("type").toString();
    QUuid reqId = reply->property("reqId").toUuid();

    if (type == "details") {
        handleDetailsReply(reply);
    } else if (type == "image") {
        handleImageReply(reply);
    } else if (type == "verification") {
        handleVerificationReply(reply);
    } else if (type == "repos") {
        handleUserReposReply(reply);
    } else if (type == "verifyRepo") {
        handleRepoVerifyReply(reply);
    } else if (type == "createIssue") {
        if (reply->error() == QNetworkReply::NoError) {
            emit issueCreated(reqId, reply->readAll());
        } else {
            emit issueCreated(reqId, reply->readAll());
        }
    } else if (type == "raw") {
        if (reply->error() == QNetworkReply::NoError) {
            emit rawDataReceived(reqId, reply->readAll());
        } else {
            emit rawDataReceived(reqId, reply->errorString().toUtf8());
        }
    } else if (type == "patch" || type == "delete") {
        handlePatchReply(reply);
    } else if (type == "read_and_done") {
        QString id = reply->property("notificationId").toString();
        m_pendingPatchRequests--;
        if (m_pendingPatchRequests < 0) m_pendingPatchRequests = 0;

        if (reply->error() == QNetworkReply::NoError) {
            markAsDone(id, reqId);
        } else {
            if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 401) {
                emit authError(reqId, "Invalid Token");
            } else {
                emit errorOccurred(reqId, reply->errorString());
            }
        }

        if (m_pendingPatchRequests == 0) {
            checkNotifications(reqId);
        }
    } else if (type == "notifications") {
        handleNotificationsReply(reply);
    } else {
        qDebug() << "Unknown reply type:" << type;
    }

    reply->deleteLater();
}

void GitHubClient::handleDetailsReply(QNetworkReply* reply) {
    QString notificationId = reply->property("notificationId").toString();
    QUuid reqId = reply->property("reqId").toUuid();
    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "Error fetching details:" << reply->errorString();
        emit detailsError(reqId, notificationId, reply->errorString());
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

        emit detailsReceived(reqId, notificationId, authorName, avatarUrl, htmlUrl);
    }
}

void GitHubClient::handleImageReply(QNetworkReply* reply) {
    QString notificationId = reply->property("notificationId").toString();
    QUuid reqId = reply->property("reqId").toUuid();
    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "Error fetching image:" << reply->errorString();
        return;
    }

    QByteArray data = reply->readAll();
    QPixmap pixmap;
    if (pixmap.loadFromData(data)) {
        emit imageReceived(reqId, notificationId, pixmap);
    }
}

void GitHubClient::handleVerificationReply(QNetworkReply* reply) {
    QUuid reqId = reply->property("reqId").toUuid();
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            QString login = obj["login"].toString();
            emit tokenVerified(reqId, true, "Token valid for user: " + login);
        } else {
            emit tokenVerified(reqId, true, "Token valid");
        }
    } else if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 401) {
        emit tokenVerified(reqId, false, "Invalid Token");
    } else {
        emit tokenVerified(reqId, false, reply->errorString());
    }
}

void GitHubClient::handleUserReposReply(QNetworkReply* reply) {
    QUuid reqId = reply->property("reqId").toUuid();
    if (reply->error() != QNetworkReply::NoError) {
        if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 401) {
            emit authError(reqId, "Invalid Token");
        } else {
            emit errorOccurred(reqId, reply->errorString());
        }
        return;
    }

    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);

    if (!doc.isArray()) {
        emit errorOccurred(reqId, "Invalid JSON response (expected array)");
        return;
    }

    QString nextPageUrl;
    bool linkRejected = false;
    if (reply->hasRawHeader("Link")) {
        QString linkHeader = reply->rawHeader("Link");
        QRegularExpression re("<([^>]+)>;\\s*rel=\"next\"");
        QRegularExpressionMatch match = re.match(linkHeader);
        if (match.hasMatch()) {
            nextPageUrl = match.captured(1);
            if (!isTrustedApiOrigin(QUrl(nextPageUrl))) {
                nextPageUrl.clear();
                linkRejected = true;
            }
        }
    }

    emit userReposReceived(reqId, doc.array(), nextPageUrl);
    if (linkRejected) {
        emit errorOccurred(reqId, "Untrusted repository pagination URL rejected.");
    }
}

void GitHubClient::handlePatchReply(QNetworkReply* reply) {
    QUuid reqId = reply->property("reqId").toUuid();
    m_pendingPatchRequests--;
    if (m_pendingPatchRequests < 0) {
        m_pendingPatchRequests = 0;
    }

    if (reply->error() != QNetworkReply::NoError) {
        if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 401) {
            emit authError(reqId, "Invalid Token");
        } else {
            emit errorOccurred(reqId, reply->errorString());
        }
        return;
    }

    if (m_pendingPatchRequests == 0) {
        checkNotifications(reqId);
    }
}

void GitHubClient::handleNotificationsReply(QNetworkReply* reply) {
    QUuid reqId = reply->property("reqId").toUuid();
    if (reply->error() != QNetworkReply::NoError) {
        if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 401) {
            emit authError(reqId, "Invalid Token");
        } else {
            emit errorOccurred(reqId, reply->errorString());
        }
        return;
    }

    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);

    if (!doc.isArray()) {
        emit errorOccurred(reqId, "Invalid JSON response (expected array)");
        return;
    }

    QJsonArray array = doc.array();

    QList<Notification> notifications;
    for (const QJsonValue& value : array) {
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
        n.reason = obj["reason"].toString();
        n.unread = obj["unread"].toBool();
        n.rawJson = obj;

        notifications.append(n);
    }

    // Group Action Results with Pull Requests
    for (int i = 0; i < notifications.size(); ++i) {
        if (notifications[i].type == "PullRequest") {
            for (int j = notifications.size() - 1; j >= 0; --j) {
                if (i != j && notifications[j].repository == notifications[i].repository &&
                    (notifications[j].type == "CheckSuite" || notifications[j].type == "WorkflowRun")) {
                    notifications[i].groupedNotifications.append(notifications[j]);
                    notifications.removeAt(j);
                    if (j < i) {
                        --i;  // Adjust index if a preceding element was removed
                    }
                }
            }
        }
    }

    // Parse Link header
    m_nextPageUrl.clear();
    bool linkRejected = false;
    if (reply->hasRawHeader("Link")) {
        QString linkHeader = reply->rawHeader("Link");
        // Example: <https://api.github.com/resource?page=2>; rel="next", <https://api.github.com/resource?page=5>;
        // rel="last"
        QRegularExpression re("<([^>]+)>;\\s*rel=\"next\"");
        QRegularExpressionMatch match = re.match(linkHeader);
        if (match.hasMatch()) {
            m_nextPageUrl = match.captured(1);
            if (!isTrustedApiOrigin(QUrl(m_nextPageUrl))) {
                m_nextPageUrl.clear();
                linkRejected = true;
            }
        }
    }

    bool append = reply->property("append").toBool();
    emit notificationsReceived(reqId, notifications, append, !m_nextPageUrl.isEmpty());
    if (linkRejected) {
        emit errorOccurred(reqId, "Untrusted pagination URL rejected.");
    }
}

QUuid GitHubClient::verifyRepo(const QString& repoFullName, QUuid reqId) {
    if (reqId.isNull()) reqId = QUuid::createUuid();
    if (m_token.isEmpty()) return reqId;

    QUrl url(m_apiUrl + "/repos/" + repoFullName);
    QNetworkRequest request = createAuthenticatedRequest(url);
    QNetworkReply* reply = manager->get(request);
    reply->setProperty("reqId", reqId);
    reply->setProperty("type", "verifyRepo");
    reply->setProperty("repoFullName", repoFullName);
    return reqId;
}

void GitHubClient::handleRepoVerifyReply(QNetworkReply* reply) {
    QUuid reqId = reply->property("reqId").toUuid();
    QString repoFullName = reply->property("repoFullName").toString();
    if (reply->error() == QNetworkReply::NoError) {
        emit repoVerified(reqId, repoFullName, true);
    } else {
        emit repoVerified(reqId, repoFullName, false);
    }
}

QUuid GitHubClient::createIssue(const QString& repoFullName, const QString& title, const QString& body,
                                const QString& assignee, QUuid reqId) {
    if (reqId.isNull()) reqId = QUuid::createUuid();
    if (m_token.isEmpty()) return reqId;

    QUrl url(m_apiUrl + "/repos/" + repoFullName + "/issues");
    QNetworkRequest request = createAuthenticatedRequest(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject obj;
    obj["title"] = title;
    if (!body.isEmpty()) {
        obj["body"] = body;
    }
    if (!assignee.isEmpty()) {
        QJsonArray assignees;
        assignees.append(assignee);
        obj["assignees"] = assignees;
    }

    QJsonDocument doc(obj);
    QByteArray postData = doc.toJson(QJsonDocument::Compact);

    QNetworkReply* reply = manager->post(request, postData);
    reply->setProperty("reqId", reqId);
    reply->setProperty("type", "createIssue");
    return reqId;
}
