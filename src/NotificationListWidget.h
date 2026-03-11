#ifndef NOTIFICATIONLISTWIDGET_H
#define NOTIFICATIONLISTWIDGET_H

#include <QList>
#include <QListWidget>
#include <QMap>
#include <QMenu>
#include <QPixmap>
#include <QSet>
#include <QUrl>
#include <QWidget>

#include "Notification.h"
#include "SettingsDialog.h"

class NotificationItemWidget;

class NotificationListWidget : public QWidget {
    Q_OBJECT
   public:
    explicit NotificationListWidget(QWidget *parent = nullptr);

    void setClient(GitHubClient *client) { m_client = client; }
    void setNotifications(const QList<Notification> &notifications, bool append, bool hasMore);
    void setFilterMode(int mode);  // 0: Inbox, 1: Unread, 2: Read
    void setSortMode(int mode);
    void setRepoFilter(const QString &repo);
    void setSearchFilter(const QString &text);

    enum SortMode {
        SortDefault = 0,
        SortUpdatedDesc,
        SortUpdatedAsc,
        SortRepoAsc,
        SortRepoDesc,
        SortTitleAsc,
        SortTitleDesc,
        SortTypeAsc,
        SortTypeDesc,
        SortLastReadDesc,
        SortLastReadAsc
    };

    void selectAll();
    void selectNone();
    void selectTop(int n);
    void dismissSelected();
    void openSelected();
    void focusNotification(const QString &id);

    // Getters for filters
    QStringList getAvailableRepos() const;
    int count() const;
    QList<Notification> getUnreadNotifications(int limit = 5) const;

   protected:
    void resizeEvent(QResizeEvent *event) override;

   public slots:
    void updateDetails(const QString &id, const QString &author, const QString &avatarUrl, const QString &htmlUrl);
    void updateImage(const QString &id, const QPixmap &pixmap);
    void updateError(const QString &id, const QString &error);
    void resetLoadMoreState();

   signals:
    void countsChanged(int total, int unread, int newCount, const QList<Notification> &newItems);
    void statusMessage(const QString &message);
    void linkActivated(const QUrl &url);
    void refreshRequested();
    void markAsRead(const QString &id);
    void markAsDone(const QString &id);
    void loadMoreRequested();
    void notificationActivated(const QString &id);
    void requestDetails(const QString &url, const QString &id);
    void requestImage(const QString &url, const QString &id);
    void requestDebugApi(const QString &url);

   private slots:
    void onListContextMenu(const QPoint &pos);
    void onItemActivated(QListWidgetItem *item);
    void onLoadMoreClicked();
    void handleLoadMoreStrategy();

   private:
    void triggerLoadMore();

    struct NotificationDetails {
        QString author;
        QString avatarUrl;
        QString htmlUrl;
        QPixmap avatar;
        bool hasDetails = false;
        bool hasImage = false;
    };

    void insertNotificationItem(int row, const Notification &n);
    void updateList();
    void applyClientFilters();
    NotificationItemWidget *findNotificationWidget(const QString &id);
    void dismissCurrentItem();
    void openUrlCurrentItem();
    void openWindowCurrentItem();
    void openUrlForItem(QListWidgetItem *item);
    void openWindowForItem(QListWidgetItem *item);
    void copyLinkCurrentItem();
    void markAsReadAndRemoveItem(QListWidgetItem *item);

    QListWidget *listWidget;
    QList<Notification> m_allNotifications;
    QMap<QString, NotificationDetails> detailsCache;
    QSet<QString> knownNotificationIds;
    QListWidgetItem *loadMoreItem;

    // Filters
    int m_filterMode;
    SortMode m_sortMode;
    QString m_repoFilter;
    QString m_searchFilter;
    bool m_hasMore;

    // Context Menu
    GitHubClient *m_client;

    QMenu *contextMenu;
    QAction *openWindowAction;
    QAction *openUrlAction;
    QAction *copyLinkAction;
    QAction *markAsReadAction;
    QAction *markAsDoneAction;
    QAction *viewRawAction;
};

#endif  // NOTIFICATIONLISTWIDGET_H
