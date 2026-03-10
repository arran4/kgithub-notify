#ifndef NOTIFICATIONWINDOW_H
#define NOTIFICATIONWINDOW_H

#include <KXmlGuiWindow>
#include <QtGui/QAction>
#include <QMenu>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include "Notification.h"
#include "GitHubClient.h"

class NotificationWindow : public KXmlGuiWindow {
    Q_OBJECT
public:
    explicit NotificationWindow(const Notification &notification, GitHubClient *client, QWidget *parent = nullptr);

signals:
    void actionRequested(const QString &actionName, const QString &id, const QString &url);
    void debugApiRequested(const QString &apiUrl);

private slots:
    void onOpenUrl();
    void onCopyLink();
    void onMarkAsRead();
    void onMarkAsDone();
    void onCloseAndMarkAsRead();
    void onViewRawJson();
    void onViewPullRequest();
    void onViewActionJob();

private:
    Notification m_notification;
    GitHubClient *m_client;
};

#endif // NOTIFICATIONWINDOW_H
