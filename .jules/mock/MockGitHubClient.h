#ifndef MOCKGITHUBCLIENT_H
#define MOCKGITHUBCLIENT_H

#include "../../src/GitHubClient.h"
#include <QTimer>

class MockGitHubClient : public GitHubClient {
    Q_OBJECT

public:
    explicit MockGitHubClient(QObject *parent = nullptr);

    void checkNotifications() override;
    void loadMore() override;
    void verifyToken() override;
    void markAsRead(const QString &id) override;
    void markAsDone(const QString &id) override;
    void markAsReadAndDone(const QString &id) override;
    void fetchNotificationDetails(const QString &url, const QString &notificationId) override;
    void fetchImage(const QString &imageUrl, const QString &notificationId) override;
    void requestRaw(const QString &endpoint, const QString &method = "GET", const QByteArray &body = QByteArray()) override;
    void fetchUserRepos(const QString &pageUrl = QString()) override;
    QNetworkRequest createAuthenticatedRequest(const QUrl &url) const override;
};

#endif // MOCKGITHUBCLIENT_H
