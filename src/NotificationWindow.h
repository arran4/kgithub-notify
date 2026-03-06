#ifndef NOTIFICATIONWINDOW_H
#define NOTIFICATIONWINDOW_H

#include <QMainWindow>
#include <QAction>
#include <QMenu>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include "Notification.h"

class NotificationWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit NotificationWindow(const Notification &notification, QWidget *parent = nullptr);

signals:
    void actionRequested(const QString &actionName, const QString &id, const QString &url);
    void debugApiRequested(const QString &apiUrl);

private slots:
    void onOpenUrl();
    void onCopyLink();
    void onMarkAsRead();
    void onMarkAsDone();
    void onCloseAndMarkAsRead();

private:
    Notification m_notification;
};

#endif // NOTIFICATIONWINDOW_H
