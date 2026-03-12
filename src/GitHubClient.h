#ifndef GITHUBCLIENT_H
#define GITHUBCLIENT_H

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QPointer>
#include <QTimer>

#include "Notification.h"
#include "SecureString.h"

class GitHubClient : public QObject {
    Q_OBJECT
    friend class TestGitHubClient;

   public:
    explicit GitHubClient(QObject *parent = nullptr);
    static QString apiToHtmlUrl(const QString &apiUrl, const QString &notificationId = "");
    void setToken(const QString &token);
    void setApiUrl(const QString &url);
    void setShowAll(bool all);
    virtual void checkNotifications();
    virtual void loadMore();
    virtual void verifyToken();
    virtual void markAsRead(const QString &id);
    virtual void markAsDone(const QString &id);
    virtual void markAsReadAndDone(const QString &id);
    virtual void fetchNotificationDetails(const QString &url, const QString &notificationId);
    virtual void fetchImage(const QString &imageUrl, const QString &notificationId);
    virtual void requestRaw(const QString &endpoint, const QString &method = "GET", const QByteArray &body = QByteArray());
    virtual void fetchUserRepos(const QString &pageUrl = QString());
    virtual QNetworkRequest createAuthenticatedRequest(const QUrl &url) const;

   signals:
    void loadingStarted();
    void notificationsReceived(const QList<Notification> &notifications, bool append, bool hasMore);
    void detailsReceived(const QString &notificationId, const QString &authorName, const QString &avatarUrl,
                         const QString &htmlUrl);
    void detailsError(const QString &notificationId, const QString &error);
    void imageReceived(const QString &notificationId, const QPixmap &avatar);
    void rawDataReceived(const QByteArray &data);
    void userReposReceived(const QJsonArray &repos, const QString &nextPageUrl);
    void errorOccurred(const QString &error);
    void authError(const QString &message);
    void tokenVerified(bool valid, const QString &message);

   private slots:
    void onReplyFinished(QNetworkReply *reply);
    void onRequestTimeout();

   private:
    QNetworkAccessManager *manager;
    SecureString m_token;
    QString m_apiUrl;
    bool m_showAll;
    int m_pendingPatchRequests;
    QString m_nextPageUrl;
    QPointer<QNetworkReply> m_activeNotificationReply;
    QTimer *m_requestTimeoutTimer;

    QNetworkRequest createRequest(const QUrl &url) const;

    void handleDetailsReply(QNetworkReply *reply);
    void handleImageReply(QNetworkReply *reply);
    void handleVerificationReply(QNetworkReply *reply);
    void handleUserReposReply(QNetworkReply *reply);
    void handlePatchReply(QNetworkReply *reply);
    void handleNotificationsReply(QNetworkReply *reply);
};

#endif  // GITHUBCLIENT_H
