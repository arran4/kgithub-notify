#ifndef GITHUBCLIENT_H
#define GITHUBCLIENT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>

struct Notification {
    QString id;
    QString title;
    QString type;
    QString repository;
    QString url; // API URL
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
    void checkNotifications();
    void verifyToken();
    void markAsRead(const QString &id);
    void fetchNotificationDetails(const QString &url, const QString &notificationId);
    void fetchImage(const QString &imageUrl, const QString &notificationId);

signals:
    void notificationsReceived(const QList<Notification> &notifications);
    void detailsReceived(const QString &notificationId, const QString &authorName, const QString &avatarUrl, const QString &htmlUrl);
    void imageReceived(const QString &notificationId, const QPixmap &avatar);
    void errorOccurred(const QString &error);
    void authError(const QString &message);
    void tokenVerified(bool valid, const QString &message);

private slots:
    void onReplyFinished(QNetworkReply *reply);

private:
    QNetworkAccessManager *manager;
    QString m_token;
    QString m_apiUrl;

    QNetworkRequest createRequest(const QUrl &url) const;
};

#endif // GITHUBCLIENT_H
