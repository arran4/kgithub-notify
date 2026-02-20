#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QListWidget>
#include <QMenu>
#include <QSet>
#include <QStackedWidget>
#include <QLabel>
#include <QPushButton>
#include "GitHubClient.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    void showTrayMessage(const QString &title, const QString &message);
    void setClient(GitHubClient *client);

    // For testing purposes
    QStackedWidget* getStackWidget() const { return stackWidget; }
    QWidget* getErrorPage() const { return errorPage; }
    QListWidget* getNotificationList() const { return notificationList; }
    QWidget* getLoginPage() const { return loginPage; }

public slots:
    void updateNotifications(const QList<Notification> &notifications);
    void showError(const QString &error);
    void onAuthError(const QString &message);

private slots:
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void onTrayMessageClicked();
    void onNotificationItemActivated(QListWidgetItem *item);
    void showSettings();
    void showContextMenu(const QPoint &pos);
    void dismissCurrentItem();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void createTrayIcon();
    void createErrorPage();
    void createLoginPage();

    QSystemTrayIcon *trayIcon;
    QMenu *trayIconMenu;
    QListWidget *notificationList;
    GitHubClient *client;
    QMenu *contextMenu;
    QAction *dismissAction;
    QSet<QString> knownNotificationIds;
    bool pendingAuthError;
    QString lastError;

    // New UI components
    QStackedWidget *stackWidget;
    QWidget *errorPage;
    QLabel *errorLabel;
    QPushButton *settingsButton;

    // Login page components
    QWidget *loginPage;
    QLabel *loginLabel;
    QPushButton *loginButton;
};

#endif // MAINWINDOW_H
