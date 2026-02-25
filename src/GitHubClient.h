#ifndef GITHUBCLIENT_H
#define GITHUBCLIENT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include "SecureString.h"

struct Notification {
    QString id;
    QString title;
    QString type;
    QString repository;
    QString url; // API URL
    QString htmlUrl; // HTML URL (cached)
    QString updatedAt;
    bool unread;
};

class GitHubClient : public QObject {
    Q_OBJECT
    friend class TestGitHubClient;

public:
    explicit GitHubClient(QObject *parent = nullptr);
    static QString apiToHtmlUrl(const QString &apiUrl, const QString &notificationId = "");
    void setToken(const QString &token);
    void setApiUrl(const QString &url);
    void setShowAll(bool all);
    void checkNotifications();
    void loadMore();
    void verifyToken();
    void markAsRead(const QString &id);
    void fetchNotificationDetails(const QString &url, const QString &notificationId);
    void fetchImage(const QString &imageUrl, const QString &notificationId);

signals:
    void loadingStarted();
    void notificationsReceived(const QList<Notification> &notifications, bool append, bool hasMore);
    void detailsReceived(const QString &notificationId, const QString &authorName, const QString &avatarUrl, const QString &htmlUrl);
    void detailsError(const QString &notificationId, const QString &error);
    void imageReceived(const QString &notificationId, const QPixmap &avatar);
    void errorOccurred(const QString &error);
    void authError(const QString &message);
    void tokenVerified(bool valid, const QString &message);

private slots:
    void onReplyFinished(QNetworkReply *reply);

private:
    QNetworkAccessManager *manager;
    SecureString m_token;
    QString m_apiUrl;
    bool m_showAll;
    int m_pendingPatchRequests;
    QString m_nextPageUrl;

    QNetworkRequest createRequest(const QUrl &url) const;

    void handleDetailsReply(QNetworkReply *reply);
    void handleImageReply(QNetworkReply *reply);
    void handleVerificationReply(QNetworkReply *reply);
    void handlePatchReply(QNetworkReply *reply);
    void handleNotificationsReply(QNetworkReply *reply);
};

#endif // GITHUBCLIENT_H
