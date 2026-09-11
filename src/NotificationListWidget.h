#ifndef NOTIFICATIONLISTWIDGET_H
#define NOTIFICATIONLISTWIDGET_H

#include <QList>
#include <QListWidget>
#include <QMap>
#include <QMenu>
#include <QPixmap>
#include <QSet>
#include <QUrl>
#include <QUuid>
#include <QWidget>
#include <QtGui/QAction>

#include "Notification.h"
#include "SettingsDialog.h"

class NotificationItemWidget;

class NotificationListWidget : public QWidget {
    Q_OBJECT
    friend class TestRequestConsumers;

    struct PendingMutation {
        QString id;
        QString action;
        bool isChild = false;
        QString childId;
    };

   public:
    explicit NotificationListWidget(QWidget* parent = nullptr);

    void setClient(GitHubClient* client);
    void setNotifications(const QList<Notification>& notifications, bool append, bool hasMore);
    void setFilterMode(int mode);  // 0: Inbox, 1: Unread, 2: Read
    void setSortMode(int mode);
    void setRepoFilter(const QString& repo);
    void setSearchFilter(const QString& text);

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
    void focusNotification(const QString& id);

    // Getters for filters
    QStringList getAvailableRepos() const;
    int count() const;
    QList<Notification> getUnreadNotifications(int limit = 5) const;

    void requestMarkAsRead(const QString& id);
    void requestMarkAsDone(const QString& id);
    void requestMarkAsReadAndDone(const QString& id);
    void requestChildMarkAsRead(const QString& parentId, const QString& childId);
    void requestChildMarkAsDone(const QString& parentId, const QString& childId);
    bool willLoadMore() const;
    bool hasMore() const { return m_hasMore; }

   protected:
    void resizeEvent(QResizeEvent* event) override;

   public slots:
    void updateDetails(const QString& id, const QString& author, const QString& avatarUrl, const QString& htmlUrl);
    void updateImage(const QString& id, const QPixmap& pixmap);
    void updateError(const QString& id, const QString& error);
    void resetLoadMoreState();
    void onMutationSucceeded(const QUuid& reqId);
    void onPartialMutationSucceeded(const QUuid& reqId, const QString& action);
    void onMutationError(const QUuid& reqId, const QString& error);

   signals:
    void countsChanged(int total, int unread, int newCount, const QList<Notification>& newItems);
    void statusMessage(const QString& message);
    void linkActivated(const QUrl& url);
    void refreshRequested();
    void loadMoreRequested();
    void notificationActivated(const QString& id);
    void requestDetails(const QString& url, const QString& id);
    void requestImage(const QString& url, const QString& id);
    void requestDebugApi(const QString& url);

   private slots:
    void onListContextMenu(const QPoint& pos);
    void onItemActivated(QListWidgetItem* item);
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

    void insertNotificationItem(int row, const Notification& n);
    void updateList();
    void applyClientFilters();
    NotificationItemWidget* findNotificationWidget(const QString& id);
    void dismissCurrentItem();
    void openUrlCurrentItem();
    void openWindowCurrentItem();
    void openUrlForItem(QListWidgetItem* item);
    void openWindowForItem(QListWidgetItem* item);
    void copyLinkCurrentItem();
    void markAsReadAndRemoveItem(QListWidgetItem* item);
    void applyReadToModel(const QString& id, bool isChild = false, const QString& childId = QString());
    void applyDoneToModel(const QString& id, bool isChild = false, const QString& childId = QString());
    void updateItemReadUi(const QString& id, bool isChild = false, const QString& childId = QString());
    void updateItemDoneUi(const QString& id, bool isChild = false, const QString& childId = QString());

    QListWidget* listWidget;
    QList<Notification> m_allNotifications;
    QMap<QString, NotificationDetails> detailsCache;
    QSet<QString> knownNotificationIds;
    void loadKnownNotifications();
    void addKnownNotification(const QString& id);
    void removeKnownNotification(const QString& id);
    QListWidgetItem* loadMoreItem;

    // Filters
    int m_filterMode;
    SortMode m_sortMode;
    QString m_repoFilter;
    QString m_searchFilter;
    bool m_hasMore;
    int m_pendingNewNotifications;
    QList<Notification> m_pendingNewlyAddedNotifications;
    bool m_countsDirty;

    QMap<QUuid, PendingMutation> m_pendingMutations;
    QMap<QString, QString> m_mutationErrors;

    // Context Menu
    GitHubClient* m_client;

    QMenu* contextMenu;
    QAction* openWindowAction;
    QAction* openUrlAction;
    QAction* copyLinkAction;
    QAction* markAsReadAction;
    QAction* markAsDoneAction;
    QAction* viewRawAction;
};

#endif  // NOTIFICATIONLISTWIDGET_H
