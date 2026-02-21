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
#include <QToolBar>
#include <QStatusBar>
#include <QTimer>
#include "GitHubClient.h"
#include "AuthErrorNotification.h"
#include "NotificationPopup.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void showTrayMessage(const QString &title, const QString &message);
    void setClient(GitHubClient *client);

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
    void openCurrentItem();
    void copyLinkCurrentItem();
    void onAuthNotificationSettingsClicked();
    void dismissAllNotifications();

    // Toolbar slots
    void onRefreshClicked();
    void updateStatusBar();
    void onSelectAllClicked();
    void onSelectNoneClicked();
    void onSelectTop10Clicked();
    void onDismissSelectedClicked();
    void onOpenSelectedClicked();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void createTrayIcon();
    void updateTrayMenu();
    void positionPopup(QWidget *popup);
    void createErrorPage();
    void createLoginPage();
    void createEmptyStatePage();

    QSystemTrayIcon *trayIcon;
    QMenu *trayIconMenu;
    QListWidget *notificationList;
    GitHubClient *client;
    QMenu *contextMenu;
    QAction *dismissAction;
    QAction *openAction;
    QAction *copyLinkAction;
    QSet<QString> knownNotificationIds;
    bool pendingAuthError;
    QString lastError;

    // Toolbar
    QToolBar *toolbar;
    QAction *refreshAction;
    QAction *selectAllAction;
    QAction *selectNoneAction;
    QAction *selectTop10Action;
    QAction *dismissSelectedAction;
    QAction *openSelectedAction;

    // New UI components
    QStackedWidget *stackWidget;
    QWidget *errorPage;
    QLabel *errorLabel;
    QPushButton *settingsButton;

    // Login page components
    QWidget *loginPage;
    QLabel *loginLabel;
    QPushButton *loginButton;

    // Empty state components
    QWidget *emptyStatePage;
    QLabel *emptyStateLabel;

    // Custom notification
    AuthErrorNotification *authNotification;
    NotificationPopup *notificationPopup;

    // Status Bar
    QStatusBar *statusBar;
    QLabel *countLabel;
    QLabel *timerLabel;
    QTimer *refreshTimer;
    QTimer *countdownTimer;
};

#endif // MAINWINDOW_H
