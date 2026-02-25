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
#include <QFutureWatcher>
#include <QIcon>
#include <QStyle>
#include <QStringList>
#include "GitHubClient.h"
#include "AuthErrorNotification.h"
#include <KNotification>

class NotificationItemWidget;
class QSpinBox;
class QComboBox;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void showTrayMessage(const QString &title, const QString &message);
    void setClient(GitHubClient *client);

public slots:
    void updateNotifications(const QList<Notification> &notifications, bool append, bool hasMore);
    void showError(const QString &error);
    void onAuthError(const QString &message);

private slots:
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void onTrayMessageClicked();
    void onNotificationItemActivated(QListWidgetItem *item);
    void showSettings();
    void onLoadingStarted();
    void showContextMenu(const QPoint &pos);
    void dismissCurrentItem();
    void openCurrentItem();
    void copyLinkCurrentItem();
    void onAuthNotificationSettingsClicked();
    void dismissAllNotifications();
    void onTokenLoaded();

    // Toolbar slots
    void onRefreshClicked();
    void updateStatusBar();
    void onSelectAllClicked();
    void onSelectNoneClicked();
    void onSelectTop10Clicked();
    void onDismissSelectedClicked();
    void onOpenSelectedClicked();
    void onOpenFirstNClicked();
    void onFilterChanged(int index);
    void onLoadMoreClicked();
    void showAboutDialog();
    void openKdeNotificationSettings();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void createTrayIcon();
    void updateTrayMenu();
    void positionPopup(QWidget *popup);
    void createErrorPage();
    void createLoginPage();
    void createEmptyStatePage();
    void createLoadingPage();

    void setupWindow();
    void setupCentralWidget();
    void setupNotificationList();
    void setupToolbar();
    void setupPages();
    void setupMenus();
    void setupStatusBar();
    void loadToken();
    QIcon themedIcon(const QStringList &names, const QString &fallbackResource = QString(),
                     QStyle::StandardPixmap fallbackPixmap = QStyle::SP_FileIcon) const;

    QSystemTrayIcon *trayIcon;
    QMenu *trayIconMenu;
    QListWidget *notificationList;
    QPushButton *loadMoreButton;
    GitHubClient *client;
    QMenu *contextMenu;
    QAction *dismissAction;
    QAction *openAction;
    QAction *copyLinkAction;
    QSet<QString> knownNotificationIds;
    bool pendingAuthError;
    QString lastError;

    struct NotificationDetails {
        QString author;
        QString avatarUrl;
        QString htmlUrl;
        QPixmap avatar;
        bool hasDetails = false;
        bool hasImage = false;
    };
    QMap<QString, NotificationDetails> detailsCache;

    NotificationItemWidget* findNotificationWidget(const QString &id);

    // Toolbar
    QToolBar *toolbar;
    QAction *refreshAction;
    QComboBox *filterComboBox;
    QAction *selectAllAction;
    QAction *selectNoneAction;
    QAction *selectTop10Action;
    QAction *dismissSelectedAction;
    QAction *openSelectedAction;
    QSpinBox *limitSpinBox;
    QAction *openFirstNAction;

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

    // Loading page components
    QWidget *loadingPage;
    QLabel *loadingLabel;
    bool m_isManualRefresh;

    // Custom notification
    AuthErrorNotification *authNotification;

    void sendNotification(const Notification &n);
    void sendSummaryNotification(int count, const QList<Notification> &notifications);

    // Status Bar
    QStatusBar *statusBar;
    QLabel *countLabel;
    QLabel *timerLabel;
    QTimer *refreshTimer;
    QTimer *countdownTimer;

    QFutureWatcher<QString> *tokenWatcher;
    QString m_loadedToken;
};

#endif // MAINWINDOW_H
