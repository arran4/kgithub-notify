#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <KNotification>
#include <QDateTime>
#include <QFutureWatcher>
#include <QIcon>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QPushButton>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStringList>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QToolBar>

#include "AuthErrorNotification.h"
#include "GitHubClient.h"
#include "Notification.h"
#include "NotificationListWidget.h"

class NotificationItemWidget;
class QSpinBox;
class QComboBox;
class QLineEdit;
class DebugWindow;

class MainWindow : public QMainWindow {
    Q_OBJECT
   public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void setClient(GitHubClient *client);
    void showTrayMessage(const QString &title, const QString &message);

   public slots:
    void updateNotifications(const QList<Notification> &notifications, bool append, bool hasMore);
    void showError(const QString &error);
    void onAuthError(const QString &message);

   private slots:
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void onTrayMessageClicked();
    void showSettings();
    void onLoadingStarted();
    void onAuthNotificationSettingsClicked();
    void dismissAllNotifications();
    void onTokenLoaded();

    // Toolbar slots
    void onRefreshClicked();
    void updateStatusBar();
    void onSelectAllClicked();
    void onSelectNoneClicked();
    void onSelectionChanged(int index);
    void onDismissSelectedClicked();
    void onOpenSelectedClicked();
    void onFilterChanged(int index);
    void showAboutDialog();
    void openKdeNotificationSettings();
    void showDebugWindow();

    // From ListWidget
    void onListCountsChanged(int total, int unread, int newCount, const QList<Notification>& newItems);
    void onListStatusMessage(const QString &message);

   protected:
    void closeEvent(QCloseEvent *event) override;

   private:
    // Helpers
    void createTrayIcon();
    void updateTrayMenu();
    void updateTrayToolTip();
    void positionPopup(QWidget *popup);
    void createErrorPage();
    void createLoginPage();
    void createEmptyStatePage();
    void createLoadingPage();
    void ensureWindowActive();
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
    void sendNotification(const Notification &n);
    void sendSummaryNotification(int count, const QList<Notification> &notifications);
    void updateSelectionComboBox();
    void updateTrayIconState(int unreadCount, int newNotifications, const QList<Notification> &newlyAddedNotifications);

    // Member Variables
    DebugWindow *debugWindow;
    QSystemTrayIcon *trayIcon;
    QMenu *trayIconMenu;
    NotificationListWidget *notificationListWidget;
    GitHubClient *client;

    bool pendingAuthError;
    QString lastError;

    // Toolbar
    QToolBar *toolbar;
    QAction *refreshAction;
    QComboBox *filterComboBox;
    QComboBox *repoFilterComboBox;
    QLineEdit *searchLineEdit;
    QAction *selectAllAction;
    QAction *selectNoneAction;
    QComboBox *selectionComboBox;
    QAction *dismissSelectedAction;
    QAction *openSelectedAction;

    // UI components
    QStackedWidget *stackWidget;
    QWidget *errorPage;
    QLabel *errorLabel;
    QPushButton *settingsButton;
    QWidget *loginPage;
    QLabel *loginLabel;
    QPushButton *loginButton;
    QWidget *emptyStatePage;
    QLabel *emptyStateLabel;
    QWidget *loadingPage;
    QLabel *loadingLabel;

    AuthErrorNotification *authNotification;

    // Status Bar
    QStatusBar *statusBar;
    QLabel *countLabel;
    QLabel *timerLabel;
    QTimer *refreshTimer;
    QTimer *countdownTimer;
    QLabel *statusLabel;

    QFutureWatcher<QString> *tokenWatcher;
    QString m_loadedToken;

    QDateTime m_lastCheckTime;

    // Cache for tray menu
    int m_lastUnreadCount;
    QList<Notification> m_lastUnreadNotifications; // Only for tray menu display if needed, or rely on widget
};

#endif  // MAINWINDOW_H
