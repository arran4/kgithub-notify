#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QListWidget>
#include <QMenu>
#include "GitHubClient.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    void showTrayMessage(const QString &title, const QString &message);
    void setClient(GitHubClient *client);

public slots:
    void updateNotifications(const QList<Notification> &notifications);

private slots:
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void onNotificationItemActivated(QListWidgetItem *item);
    void showSettings();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void createTrayIcon();
    void createActions();

    QSystemTrayIcon *trayIcon;
    QMenu *trayIconMenu;
    QListWidget *notificationList;
    GitHubClient *client;
    QMenu *contextMenu;
    QAction *dismissAction;

private slots:
    void showContextMenu(const QPoint &pos);
    void dismissCurrentItem();
};

#endif // MAINWINDOW_H
