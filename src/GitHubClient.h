#ifndef GITHUBCLIENT_H
#define GITHUBCLIENT_H

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QTimer>
#include <QUuid>

#include "Notification.h"
#include "SecureString.h"

class GitHubClient : public QObject {
    Q_OBJECT
    friend class TestGitHubClient;

   public:
    explicit GitHubClient(QObject* parent = nullptr);
    static QString apiToHtmlUrl(const QString& apiUrl, const QString& notificationId = "");
    void setToken(const QString& token);
    void setApiUrl(const QString& url);
    void setShowAll(bool all);
    QUuid checkNotifications(QUuid reqId = QUuid());
    QUuid loadMore(QUuid reqId = QUuid());
    QUuid verifyToken(QUuid reqId = QUuid());
    QUuid markAsRead(const QString& id, QUuid reqId = QUuid());
    QUuid markAsDone(const QString& id, QUuid reqId = QUuid());
    QUuid markAsReadAndDone(const QString& id, QUuid reqId = QUuid());
    QUuid fetchNotificationDetails(const QString& url, const QString& notificationId, QUuid reqId = QUuid());
    QUuid fetchImage(const QString& imageUrl, const QString& notificationId, QUuid reqId = QUuid());
    QUuid requestRaw(const QString& endpoint, const QString& method = "GET", const QByteArray& body = QByteArray(),
                     QUuid reqId = QUuid());
    QUuid fetchUserRepos(const QString& pageUrl = QString(), QUuid reqId = QUuid());
    QUuid verifyRepo(const QString& repoFullName, QUuid reqId = QUuid());
    QUuid createIssue(const QString& repoFullName, const QString& title, const QString& body,
                      const QString& assignee = "", QUuid reqId = QUuid());
    QNetworkRequest createAuthenticatedRequest(const QUrl& url) const;

   signals:
    void loadingStarted(const QUuid& reqId);
    void notificationsReceived(const QUuid& reqId, const QList<Notification>& notifications, bool append, bool hasMore);
    void detailsReceived(const QUuid& reqId, const QString& notificationId, const QString& authorName,
                         const QString& avatarUrl, const QString& htmlUrl);
    void detailsError(const QUuid& reqId, const QString& notificationId, const QString& error);
    void imageReceived(const QUuid& reqId, const QString& notificationId, const QPixmap& avatar);
    void rawDataReceived(const QUuid& reqId, const QByteArray& data);
    void userReposReceived(const QUuid& reqId, const QJsonArray& repos, const QString& nextPageUrl);
    void errorOccurred(const QUuid& reqId, const QString& error);
    void authError(const QUuid& reqId, const QString& message);
    void tokenVerified(const QUuid& reqId, bool valid, const QString& message);
    void repoVerified(const QUuid& reqId, const QString& repoFullName, bool exists);
    void issueCreated(const QUuid& reqId, const QByteArray& data);

   private slots:
    void onReplyFinished(QNetworkReply* reply);

   private:
    QNetworkAccessManager* manager;
    SecureString m_token;
    QString m_apiUrl;
    bool m_showAll;
    int m_pendingPatchRequests;
    QString m_nextPageUrl;

    bool isTrustedApiOrigin(const QUrl& url) const;
    QNetworkRequest createRequest(const QUrl& url) const;

    void handleDetailsReply(QNetworkReply* reply);
    void handleImageReply(QNetworkReply* reply);
    void handleVerificationReply(QNetworkReply* reply);
    void handleUserReposReply(QNetworkReply* reply);
    void handleRepoVerifyReply(QNetworkReply* reply);
    void handlePatchReply(QNetworkReply* reply);
    void handleNotificationsReply(QNetworkReply* reply);
};

#endif  // GITHUBCLIENT_H
