#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <KNotification>
#include <QDateTime>
#include <QFutureWatcher>
#include <QIcon>
#include <QLabel>
#include <QListWidget>
#include <QMainWindow>
#include <QMap>
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
    void setClient(GitHubClient *client);
    void showTrayMessage(const QString &title, const QString &message);

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
    NotificationItemWidget *findNotificationWidget(const QString &id);
    void loadSavedNotifications();
    void saveSavedNotifications();
    void saveNotification(const Notification &n);
    void unsaveNotification(const QString &id);
    bool isNotificationSaved(const QString &id) const;
    void loadDoneNotifications();
    void saveDoneNotifications();
    void saveDoneNotification(const Notification &n);
    bool isNotificationDone(const QString &id) const;
    void addNotificationItem(const Notification &n);
    void focusNotification(const QString &id);
    void updateSelectionComboBox();
    void processNewNotifications(const QList<Notification> &notifications, int &unreadCount, int &newNotifications,
                                 QList<Notification> &newlyAddedNotifications);
    void updateTrayIconState(int unreadCount, int newNotifications, const QList<Notification> &newlyAddedNotifications);
    void updateListWidget(const QList<Notification> &notifications, bool append, bool hasMore);

    // Member Variables
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

    // Toolbar
    QToolBar *toolbar;
    QAction *refreshAction;
    QComboBox *filterComboBox;
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

    bool m_isManualRefresh;
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
    QMap<int, QDateTime> lastRefreshTime;

    QList<Notification> m_savedNotifications;
    QList<Notification> m_doneNotifications;

    static const int MAX_DONE_NOTIFICATIONS = 100;
    QDateTime m_lastCheckTime;
};

#endif  // MAINWINDOW_H
