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
    m_notificationSessionId = reqId;
    m_nextPageUrl.clear();
    emit loadingStarted(reqId);

    if (m_token.isEmpty()) {
        emit authError(reqId, "No token provided");
        return reqId;
    }

    QUrl url(m_apiUrl + "/notifications");
    QUrlQuery query;
    // Include read notifications for client-side filtering.
    query.addQueryItem("all", "true");
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
    if (reqId.isNull()) reqId = m_notificationSessionId;
    if (reqId.isNull()) reqId = QUuid::createUuid();
    emit loadingStarted(reqId);
    if (reqId != m_notificationSessionId || m_nextPageUrl.isEmpty()) {
        emit errorOccurred(reqId, "No active notification page to load.");
        return reqId;
    }
    if (m_token.isEmpty()) {
        emit authError(reqId, "No token provided");
        return reqId;
    }

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
        emit tokenVerified(reqId, false, TokenCapabilities{}, "No token provided");
        return reqId;
    }

    VerificationSession* session = new VerificationSession{reqId, TokenCapabilities{}};

    QUrl url(m_apiUrl + "/user");
    QNetworkRequest request = createAuthenticatedRequest(url);

    QNetworkReply* reply = manager->get(request);
    reply->setProperty("type", "verification");
    reply->setProperty("reqId", reqId);

    connect(reply, &QNetworkReply::finished, this, [this, reply, session]() { onVerifyUserFinished(reply, session); });

    return reqId;
}

void GitHubClient::onVerifyUserFinished(QNetworkReply* reply, VerificationSession* session) {
    reply->deleteLater();

    if (reply->error() == QNetworkReply::NoError) {
        int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (statusCode == 200) {
            QJsonDocument json = QJsonDocument::fromJson(reply->readAll());
            if (json.isObject()) {
                session->capabilities.login = json.object().value("login").toString();
            }
            if (reply->hasRawHeader("X-OAuth-Scopes")) {
                QString scopes = QString::fromUtf8(reply->rawHeader("X-OAuth-Scopes"));
                if (scopes.split(", ").contains("notifications")) {
                    session->capabilities.hasNotifications = true;
                } else {
                    session->capabilities.hasNotifications = false;
                }
                if (scopes.split(", ").contains("repo")) {
                    session->capabilities.hasRepoMetadata = true;
                    session->capabilities.hasPrivateRepos = true;
                    session->capabilities.hasCreateIssues = true;
                    session->capabilities.hasPrComments = true;
                }
            }

            QUrl reposUrl(m_apiUrl + "/user/repos?per_page=1");
            QNetworkRequest reposRequest = createAuthenticatedRequest(reposUrl);
            QNetworkReply* reposReply = manager->get(reposRequest);
            reposReply->setProperty("type", "verification");
            connect(reposReply, &QNetworkReply::finished, this,
                    [this, reposReply, session]() { onVerifyReposFinished(reposReply, session); });
        } else {
            finalizeVerification(session, false, QString("HTTP Status: %1").arg(statusCode));
        }
    } else {
        QString errorMsg = reply->errorString();
        int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (statusCode > 0) {
            errorMsg = QString("HTTP %1: %2").arg(statusCode).arg(errorMsg);
        } else if (reply->error() == QNetworkReply::ConnectionRefusedError) {
            errorMsg = "Connection refused. Please check your network or proxy settings.";
        } else if (reply->error() == QNetworkReply::TimeoutError) {
            errorMsg = "Request timed out.";
        }
        finalizeVerification(session, false, errorMsg);
    }
}

void GitHubClient::onVerifyReposFinished(QNetworkReply* reply, VerificationSession* session) {
    reply->deleteLater();

    if (reply->error() == QNetworkReply::NoError) {
        int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (statusCode == 200) {
            if (!session->capabilities.hasRepoMetadata.has_value()) {
                session->capabilities.hasRepoMetadata = true;
            }
        }
    } else {
        // Leave as Unknown (std::nullopt) on network/rate limit errors to avoid false negative.
        if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 403 ||
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 401 ||
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 404) {
            if (!session->capabilities.hasRepoMetadata.has_value()) {
                session->capabilities.hasRepoMetadata = false;
            }
        }
    }

    QUrl notifUrl(m_apiUrl + "/notifications?per_page=1");
    QNetworkRequest notifRequest = createAuthenticatedRequest(notifUrl);
    QNetworkReply* notifReply = manager->get(notifRequest);
    notifReply->setProperty("type", "verification");
    connect(notifReply, &QNetworkReply::finished, this,
            [this, notifReply, session]() { onVerifyNotificationsFinished(notifReply, session); });
}

void GitHubClient::onVerifyNotificationsFinished(QNetworkReply* reply, VerificationSession* session) {
    reply->deleteLater();
    if (reply->error() == QNetworkReply::NoError &&
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200) {
        session->capabilities.hasNotifications = true;
    }
    finalizeVerification(session, true);
}

void GitHubClient::finalizeVerification(VerificationSession* session, bool isValid, const QString& error) {
    emit tokenVerified(session->uuid, isValid, session->capabilities, error);
    delete session;
}

QString GitHubClient::getPermissionGuidance() {
    return "Ensure your token has the correct scopes.\n"
           "For Classic Tokens: 'repo' and 'notifications'.\n"
           "For Fine-grained Tokens: Read-only for Metadata and Notifications, Read/Write for Issues and Pull "
           "Requests.";
}

QUuid GitHubClient::markAsRead(const QString& id, QUuid reqId) {
    if (reqId.isNull()) reqId = QUuid::createUuid();
    if (m_token.isEmpty()) {
        emit errorOccurred(reqId, "No token provided");
        return reqId;
    }

    m_pendingPatchRequests++;

    QUrl url(m_apiUrl + "/notifications/threads/" + id);
    QNetworkRequest request = createAuthenticatedRequest(url);

    QNetworkReply* reply = manager->sendCustomRequest(request, "PATCH");
    reply->setProperty("reqId", reqId);
    reply->setProperty("type", "patch");
    return reqId;
}

QUuid GitHubClient::markAsDone(const QString& id, QUuid reqId) {
    if (reqId.isNull()) reqId = QUuid::createUuid();
    if (m_token.isEmpty()) {
        emit errorOccurred(reqId, "No token provided");
        return reqId;
    }

    m_pendingPatchRequests++;

    QUrl url(m_apiUrl + "/notifications/threads/" + id);
    QNetworkRequest request = createAuthenticatedRequest(url);

    QNetworkReply* reply = manager->deleteResource(request);
    reply->setProperty("reqId", reqId);
    reply->setProperty("type", "delete");
    return reqId;
}

QUuid GitHubClient::markAsReadAndDone(const QString& id, QUuid reqId) {
    if (reqId.isNull()) reqId = QUuid::createUuid();
    if (m_token.isEmpty()) {
        emit errorOccurred(reqId, "No token provided");
        return reqId;
    }

    m_pendingPatchRequests++;

    QUrl url(m_apiUrl + "/notifications/threads/" + id);
    QNetworkRequest request = createAuthenticatedRequest(url);

    QNetworkReply* reply = manager->sendCustomRequest(request, "PATCH");
    reply->setProperty("reqId", reqId);
    reply->setProperty("type", "read_and_done_stage1");
    reply->setProperty("notificationId", id);

    m_pendingReadAndDone.insert(reqId, {id});
    return reqId;
}

QUuid GitHubClient::fetchNotificationDetails(const QString& url, const QString& notificationId, QUuid reqId) {
    if (reqId.isNull()) reqId = QUuid::createUuid();
    QUrl qUrl(url);
    if (m_token.isEmpty() || url.isEmpty() || !qUrl.isValid()) {
        emit detailsError(reqId, notificationId, m_token.isEmpty() ? "No token provided" : "Invalid details URL");
        return reqId;
    }

    if (!isTrustedApiOrigin(qUrl)) {
        emit detailsError(reqId, notificationId, "Untrusted notification details URL rejected.");
        emit errorOccurred(reqId, "Untrusted notification details URL rejected.");
        return reqId;
    }

    QNetworkRequest request = createRequest(qUrl);
    QNetworkReply* reply = manager->get(request);
    reply->setProperty("reqId", reqId);
    reply->setProperty("type", "details");
    reply->setProperty("notificationId", notificationId);
    return reqId;
}

QUuid GitHubClient::fetchImage(const QString& imageUrl, const QString& notificationId, QUuid reqId) {
    if (reqId.isNull()) reqId = QUuid::createUuid();
    if (imageUrl.isEmpty() || !QUrl(imageUrl).isValid()) {
        emit detailsError(reqId, notificationId, "Invalid image URL");
        return reqId;
    }

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
    if (m_token.isEmpty()) {
        emit errorOccurred(reqId, "No token provided");
        return reqId;
    }

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
    } else {
        reply = manager->sendCustomRequest(request, method.toUtf8(), body);
    }

    if (reply) {
        reply->setProperty("reqId", reqId);
        reply->setProperty("type", "raw");
    }
    return reqId;
}

QUuid GitHubClient::fetchUserRepos(const QString& pageUrl, QUuid reqId) {
    if (reqId.isNull()) reqId = QUuid::createUuid();
    if (m_token.isEmpty()) {
        emit errorOccurred(reqId, "No token provided");
        return reqId;
    }

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
    reply->setProperty("reqId", reqId);
    reply->setProperty("type", "repos");
    return reqId;
}

QUuid GitHubClient::verifyRepo(const QString& repoFullName, QUuid reqId) {
    if (reqId.isNull()) reqId = QUuid::createUuid();
    if (m_token.isEmpty()) {
        emit errorOccurred(reqId, "No token provided");
        return reqId;
    }

    QUrl url(m_apiUrl + "/repos/" + repoFullName);
    QNetworkRequest request = createAuthenticatedRequest(url);
    QNetworkReply* reply = manager->get(request);
    reply->setProperty("reqId", reqId);
    reply->setProperty("type", "verifyRepo");
    reply->setProperty("repoFullName", repoFullName);
    return reqId;
}

QUuid GitHubClient::createIssue(const QString& repoFullName, const QString& title, const QString& body,
                                const QString& assignee, QUuid reqId) {
    if (reqId.isNull()) reqId = QUuid::createUuid();
    if (m_token.isEmpty()) {
        emit errorOccurred(reqId, "No token provided");
        return reqId;
    }

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

QNetworkRequest GitHubClient::createAuthenticatedRequest(const QUrl& url) const { return createRequest(url); }

void GitHubClient::onReplyFinished(QNetworkReply* reply) {
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
            QByteArray errorData = reply->readAll();
            QString errorString = reply->errorString();

            QJsonDocument doc = QJsonDocument::fromJson(errorData);
            if (doc.isObject() && doc.object().contains("message")) {
                errorString += " - " + doc.object()["message"].toString();
            }
            emit errorOccurred(reqId, errorString);
        }
    } else if (type == "raw") {
        if (reply->error() == QNetworkReply::NoError) {
            emit rawDataReceived(reqId, reply->readAll());
        } else {
            emit errorOccurred(reqId, reply->errorString());
        }
    } else if (type == "patch" || type == "delete" || type == "read_and_done_stage1" ||
               type == "read_and_done_stage2") {
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
            emit notificationsChanged();
        }
    } else if (type == "notifications") {
        handleNotificationsReply(reply);
    } else {
        qDebug() << "Unknown reply type:" << type;
    }

    reply->deleteLater();
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
    } else {
        emit detailsError(reqId, notificationId, "Invalid details response");
    }
}

void GitHubClient::handleImageReply(QNetworkReply* reply) {
    QString notificationId = reply->property("notificationId").toString();
    QUuid reqId = reply->property("reqId").toUuid();
    if (reply->error() != QNetworkReply::NoError) {
        emit detailsError(reqId, notificationId, reply->errorString());
        return;
    }

    QByteArray data = reply->readAll();
    QPixmap pixmap;
    if (pixmap.loadFromData(data)) {
        emit imageReceived(reqId, notificationId, pixmap);
    } else {
        emit detailsError(reqId, notificationId, "Invalid image response");
    }
}

void GitHubClient::handleVerificationReply(QNetworkReply* reply) {
    // Deprecated
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
    QString type = reply->property("type").toString();

    if (type == "read_and_done_stage1") {
        if (reply->error() != QNetworkReply::NoError) {
            m_pendingPatchRequests--;
            if (m_pendingPatchRequests < 0) m_pendingPatchRequests = 0;

            m_pendingReadAndDone.remove(reqId);
            if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 401 ||
                reply->error() == QNetworkReply::AuthenticationRequiredError) {
                emit authError(reqId, "Invalid Token");
            } else {
                emit errorOccurred(reqId, reply->errorString());
            }
            return;
        }

        PendingReadAndDone pending = m_pendingReadAndDone.take(reqId);

        QUrl url(m_apiUrl + "/notifications/threads/" + pending.id);
        QNetworkRequest request = createAuthenticatedRequest(url);
        QNetworkReply* deleteReply = manager->sendCustomRequest(request, "DELETE");
        deleteReply->setProperty("reqId", reqId);
        deleteReply->setProperty("type", "read_and_done_stage2");
        deleteReply->setProperty("notificationId", pending.id);
        return;
    }

    if (type == "read_and_done_stage2") {
        m_pendingPatchRequests--;
        if (m_pendingPatchRequests < 0) m_pendingPatchRequests = 0;

        if (reply->error() != QNetworkReply::NoError) {
            emit partialMutationSucceeded(reqId, "read");
            if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 401 ||
                reply->error() == QNetworkReply::AuthenticationRequiredError) {
                emit authError(reqId, "Invalid Token");
            } else {
                emit errorOccurred(reqId, reply->errorString());
            }
            return;
        }

        emit mutationSucceeded(reqId);
        if (m_pendingPatchRequests == 0) {
            emit notificationsChanged();
        }
        return;
    }

    m_pendingPatchRequests--;
    if (m_pendingPatchRequests < 0) {
        m_pendingPatchRequests = 0;
    }

    if (reply->error() != QNetworkReply::NoError) {
        if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 401 ||
            reply->error() == QNetworkReply::AuthenticationRequiredError) {
            emit authError(reqId, "Invalid Token");
        } else {
            emit errorOccurred(reqId, reply->errorString());
        }
        return;
    }

    emit mutationSucceeded(reqId);
    if (m_pendingPatchRequests == 0) {
        emit notificationsChanged();
    }
}

void GitHubClient::handleNotificationsReply(QNetworkReply* reply) {
    QUuid reqId = reply->property("reqId").toUuid();
    if (reqId != m_notificationSessionId) return;
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

        if (obj.contains("groupedNotifications")) {
            QJsonArray grouped = obj["groupedNotifications"].toArray();
            for (const QJsonValue& v : grouped) {
                n.groupedNotifications.append(Notification::fromJson(v.toObject()));
            }
        }

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

void GitHubClient::handleRepoVerifyReply(QNetworkReply* reply) {
    QUuid reqId = reply->property("reqId").toUuid();
    QString repoFullName = reply->property("repoFullName").toString();
    if (reply->error() == QNetworkReply::NoError) {
        emit repoVerified(reqId, repoFullName, true);
    } else if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 404) {
        emit repoVerified(reqId, repoFullName, false);
    } else {
        emit errorOccurred(reqId, reply->errorString());
    }
}
