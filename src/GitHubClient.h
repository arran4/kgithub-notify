#ifndef GITHUBCLIENT_H
#define GITHUBCLIENT_H

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>

#include "SecureString.h"

struct Notification {
    QString id;
    QString title;
    QString type;
    QString repository;
    QString url;      // API URL
    QString htmlUrl;  // HTML URL (cached)
    QString updatedAt;
    bool unread;
};

class GitHubClient : public QObject {
    Q_OBJECT
   public:
    explicit GitHubClient(QObject *parent = nullptr);
    static QString apiToHtmlUrl(const QString &apiUrl, const QString &notificationId = "");
    void setToken(const QString &token);
    void setApiUrl(const QString &url);
    void setShowAll(bool all);
    void checkNotifications();
    void verifyToken();
    void markAsRead(const QString &id);
    void fetchNotificationDetails(const QString &url, const QString &notificationId);
    void fetchImage(const QString &imageUrl, const QString &notificationId);

   signals:
    void loadingStarted();
    void notificationsReceived(const QList<Notification> &notifications);
    void detailsReceived(const QString &notificationId, const QString &authorName, const QString &avatarUrl,
                         const QString &htmlUrl);
    void imageReceived(const QString &notificationId, const QPixmap &avatar);
    void errorOccurred(const QString &error);
    void authError(const QString &message);
    void tokenVerified(bool valid, const QString &message);

   private slots:
    void onReplyFinished(QNetworkReply *reply);

   private:
    friend class TestGitHubClient;

    void handleDetailsReply(QNetworkReply *reply);
    void handleImageReply(QNetworkReply *reply);
    void handleVerificationReply(QNetworkReply *reply);
    void handlePatchReply(QNetworkReply *reply);
    void handleNotificationsReply(QNetworkReply *reply);

    QNetworkAccessManager *manager;
    SecureString m_token;
    QString m_apiUrl;
    bool m_showAll;
    int m_pendingPatchRequests;

    QNetworkRequest createRequest(const QUrl &url) const;
};

#endif  // GITHUBCLIENT_H
