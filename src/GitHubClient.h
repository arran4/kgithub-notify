#ifndef GITHUBCLIENT_H
#define GITHUBCLIENT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QPixmap>

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
    void fetchSubjectDetails(const QString &url, const QString &notificationId);
    void fetchImage(const QString &url, const QString &id);

signals:
    void notificationsReceived(const QList<Notification> &notifications);
    void errorOccurred(const QString &error);
    void authError(const QString &message);
    void tokenVerified(bool valid, const QString &message);
    void subjectDetailsReceived(const QString &notificationId, const QString &authorName, const QString &avatarUrl);
    void imageReceived(const QString &id, const QPixmap &pixmap);

private slots:
    void onReplyFinished(QNetworkReply *reply);

private:
    QNetworkAccessManager *manager;
    QString m_token;
    QString m_apiUrl;

    QNetworkRequest createRequest(const QUrl &url) const;
};

#endif // GITHUBCLIENT_H
