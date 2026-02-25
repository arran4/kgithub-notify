#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <KNotification>
#include <QDateTime>
#include <QFutureWatcher>
#include <QIcon>
#include <QLabel>
#include <QListWidget>
#include <QMainWindow>
#include <QMenu>
#include <QPushButton>
#include <QSet>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStringList>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QToolBar>

#include "AuthErrorNotification.h"
#include "GitHubClient.h"

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
    void onSelectionChanged(int index);
    void onDismissSelectedClicked();
    void onOpenSelectedClicked();
    void onFilterChanged(int index);
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
    QIcon loadSvgIcon(const QString &path);

    QSystemTrayIcon *trayIcon;
    QMenu *trayIconMenu;
    QListWidget *notificationList;
    QListWidgetItem *loadMoreItem;
    GitHubClient *client;
    QMenu *contextMenu;
    QAction *dismissAction;
    QAction *openAction;
    QAction *copyLinkAction;
    QAction *markAsReadAction;
    QAction *markAsDoneAction;
    QAction *saveAction;
    QAction *unsaveAction;
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

    NotificationItemWidget *findNotificationWidget(const QString &id);

    // Toolbar
    QToolBar *toolbar;
    QAction *refreshAction;
    QComboBox *filterComboBox;
    QAction *selectAllAction;
    QAction *selectNoneAction;
    QComboBox *selectionComboBox;
    QAction *dismissSelectedAction;
    QAction *openSelectedAction;

    void updateSelectionComboBox();

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

    QList<Notification> m_savedNotifications;
    void loadSavedNotifications();
    void saveSavedNotifications();
    void saveNotification(const Notification &n);
    void unsaveNotification(const QString &id);
    bool isNotificationSaved(const QString &id) const;

    QList<Notification> m_doneNotifications;
    void loadDoneNotifications();
    void saveDoneNotifications();
    void saveDoneNotification(const Notification &n);
    bool isNotificationDone(const QString &id) const;
    void addNotificationItem(const Notification &n);
    static const int MAX_DONE_NOTIFICATIONS = 100;

    QDateTime m_lastCheckTime;
    void updateTrayToolTip();
};

#endif  // MAINWINDOW_H
