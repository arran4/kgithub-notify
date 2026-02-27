#include "NotificationListWidget.h"
#include "NotificationItemWidget.h"
#include "GitHubClient.h"

#include <QVBoxLayout>
#include <QListWidgetItem>
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QPushButton>
#include <QScrollBar>
#include <QTimer>
#include <QResizeEvent>

NotificationListWidget::NotificationListWidget(QWidget *parent)
    : QWidget(parent),
      loadMoreItem(nullptr),
      m_filterMode(0),
      m_hasMore(false) {

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    listWidget = new QListWidget(this);
    listWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);

    connect(listWidget, &QListWidget::itemActivated, this, &NotificationListWidget::onItemActivated);

    listWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(listWidget, &QListWidget::customContextMenuRequested, this, &NotificationListWidget::onListContextMenu);
    connect(listWidget->verticalScrollBar(), &QScrollBar::valueChanged, this, [this](){ handleLoadMoreStrategy(); });

    layout->addWidget(listWidget);

    // Setup Context Menu
    contextMenu = new QMenu(this);

    openAction = new QAction(tr("Open"), this);
    connect(openAction, &QAction::triggered, this, &NotificationListWidget::openCurrentItem);
    contextMenu->addAction(openAction);

    copyLinkAction = new QAction(tr("Copy Link"), this);
    connect(copyLinkAction, &QAction::triggered, this, &NotificationListWidget::copyLinkCurrentItem);
    contextMenu->addAction(copyLinkAction);

    markAsReadAction = new QAction(tr("Mark as Read"), this);
    connect(markAsReadAction, &QAction::triggered, this, [this]() {
        QListWidgetItem *item = listWidget->currentItem();
        if (!item) return;

        NotificationItemWidget *widget = qobject_cast<NotificationItemWidget *>(listWidget->itemWidget(item));
        if (widget && widget->isLoading()) return;
        if (widget) widget->setLoading(true);

        QString id = item->data(Qt::UserRole + 1).toString();
        emit markAsRead(id);

        if (widget) {
            widget->setRead(true);
        }

        QJsonObject json = item->data(Qt::UserRole + 4).toJsonObject();
        Notification n = Notification::fromJson(json);
        n.unread = false;
        n.inInbox = false;
        item->setData(Qt::UserRole + 4, n.toJson());

        QFont font = item->font();
        font.setBold(false);
        item->setFont(font);

        // If filtering by unread, remove it
        if (m_filterMode == 0 || m_filterMode == 1) {
            delete listWidget->takeItem(listWidget->row(item));
        }
    });
    contextMenu->addAction(markAsReadAction);

    markAsDoneAction = new QAction(tr("Mark as Done"), this);
    connect(markAsDoneAction, &QAction::triggered, this, &NotificationListWidget::dismissCurrentItem);
    contextMenu->addAction(markAsDoneAction);

    contextMenu->addSeparator();

    dismissAction = new QAction(tr("Dismiss"), this);
    connect(dismissAction, &QAction::triggered, this, &NotificationListWidget::dismissCurrentItem);
    contextMenu->addAction(dismissAction);
}

void NotificationListWidget::setNotifications(const QList<Notification> &notifications, bool append, bool hasMore) {
    if (!append) {
        m_allNotifications = notifications;
    } else {
        m_allNotifications.append(notifications);
    }
    m_hasMore = hasMore;

    int unreadCount = 0;
    int newNotifications = 0;
    QList<Notification> newlyAddedNotifications;

    for (const Notification &n : notifications) {
        if (n.unread) {
            unreadCount++;
            if (!knownNotificationIds.contains(n.id)) {
                newNotifications++;
                knownNotificationIds.insert(n.id);
                newlyAddedNotifications.append(n);
            }
        }
    }

    // Calculate total unread from m_allNotifications
    int totalUnread = 0;
    for(const auto& n : m_allNotifications) {
        if(n.inInbox && n.unread) totalUnread++;
    }

    emit countsChanged(m_allNotifications.count(), totalUnread, newNotifications, newlyAddedNotifications);
    updateList();
}

void NotificationListWidget::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    handleLoadMoreStrategy();
}

void NotificationListWidget::setFilterMode(int mode) {
    if (m_filterMode == mode) return;
    m_filterMode = mode;
    updateList();
}

void NotificationListWidget::setRepoFilter(const QString &repo) {
    if (m_repoFilter == repo) return;
    m_repoFilter = repo;
    applyClientFilters();
}

void NotificationListWidget::setSearchFilter(const QString &text) {
    if (m_searchFilter == text) return;
    m_searchFilter = text;
    applyClientFilters();
}

void NotificationListWidget::selectAll() {
    listWidget->selectAll();
}

void NotificationListWidget::selectNone() {
    listWidget->clearSelection();
}

void NotificationListWidget::selectTop(int n) {
    listWidget->clearSelection();
    int count = listWidget->count();
    int limit = qMin(n, count);

    for (int i = 0; i < limit; ++i) {
        QListWidgetItem *item = listWidget->item(i);
        if (item) item->setSelected(true);
    }
}

void NotificationListWidget::dismissSelected() {
    QList<QListWidgetItem *> items = listWidget->selectedItems();
    for (auto item : items) {
        NotificationItemWidget *widget = qobject_cast<NotificationItemWidget *>(listWidget->itemWidget(item));
        if (widget && !widget->isLoading()) {
            widget->setLoading(true);

            QString id = item->data(Qt::UserRole + 1).toString();
            QJsonObject json = item->data(Qt::UserRole + 4).toJsonObject();
            Notification n = Notification::fromJson(json);

            n.unread = false;
            n.inInbox = false;
            // Update item data
            item->setData(Qt::UserRole + 4, n.toJson());
            QFont font = item->font();
            font.setBold(false);
            item->setFont(font);

            emit markAsDone(id); // Effectively mark as read and done

            // Remove item from list
            delete listWidget->takeItem(listWidget->row(item));
        }
    }
}

void NotificationListWidget::openSelected() {
    QList<QListWidgetItem *> items = listWidget->selectedItems();
    for (auto item : items) {
        QString apiUrl = item->data(Qt::UserRole).toString();
        QString id = item->data(Qt::UserRole + 1).toString();
        QString htmlUrl = GitHubClient::apiToHtmlUrl(apiUrl, id);
        QDesktopServices::openUrl(QUrl(htmlUrl));
    }
}

void NotificationListWidget::handleLoadMoreStrategy() {
    if (!loadMoreItem) return;

    // If already loading or not valid, return
    QPushButton *btn = qobject_cast<QPushButton *>(listWidget->itemWidget(loadMoreItem));
    if (!btn || !btn->isEnabled()) return;

    SettingsDialog::GetDataOption option = SettingsDialog::getGetDataOption();

    if (option == SettingsDialog::Manual) {
        return;
    }

    if (option == SettingsDialog::FillScreen) {
        // Trigger if scrollbar range is 0 (content fits in viewport)
        if (listWidget->verticalScrollBar()->maximum() <= 0) {
            triggerLoadMore();
        }
        return;
    }

    if (option == SettingsDialog::GetAll) {
        triggerLoadMore();
        return;
    }

    if (option == SettingsDialog::Infinite) {
        QRect itemRect = listWidget->visualItemRect(loadMoreItem);
        QRect viewportRect = listWidget->viewport()->rect();

        if (viewportRect.intersects(itemRect)) {
            triggerLoadMore();
        }
        return;
    }
}

void NotificationListWidget::focusNotification(const QString &id) {
    for (int i = 0; i < listWidget->count(); ++i) {
        QListWidgetItem *item = listWidget->item(i);
        if (item->data(Qt::UserRole + 1).toString() == id) {
            listWidget->scrollToItem(item);
            listWidget->setCurrentItem(item);
            emit notificationActivated(id);
            break;
        }
    }
}

QStringList NotificationListWidget::getAvailableRepos() const {
    QSet<QString> repos;
    // Iterate over visible items or all items?
    // Usually repo filter is based on currently loaded items
    for (int i = 0; i < listWidget->count(); ++i) {
        QListWidgetItem *item = listWidget->item(i);
        if (item == loadMoreItem) continue;
        QString repo = item->data(Qt::UserRole + 3).toString();
        if (!repo.isEmpty()) {
            repos.insert(repo);
        }
    }
    QStringList repoList = repos.values();
    repoList.sort();
    return repoList;
}

int NotificationListWidget::count() const {
    return listWidget->count();
}

void NotificationListWidget::updateDetails(const QString &id, const QString &author, const QString &avatarUrl, const QString &htmlUrl) {
    NotificationDetails &details = detailsCache[id];
    details.author = author;
    details.avatarUrl = avatarUrl;
    details.htmlUrl = htmlUrl;
    details.hasDetails = true;

    NotificationItemWidget *widget = findNotificationWidget(id);
    if (widget) {
        widget->setAuthor(author, details.avatar);
        widget->setHtmlUrl(htmlUrl);
    }

    if (!details.hasImage && !avatarUrl.isEmpty()) {
        emit requestImage(avatarUrl, id);
    }
}

void NotificationListWidget::updateImage(const QString &id, const QPixmap &pixmap) {
    NotificationDetails &details = detailsCache[id];
    details.avatar = pixmap;
    details.hasImage = true;

    NotificationItemWidget *widget = findNotificationWidget(id);
    if (widget) {
        widget->setAuthor(details.author, pixmap);
    }
}

void NotificationListWidget::updateError(const QString &id, const QString &error) {
    NotificationItemWidget *widget = findNotificationWidget(id);
    if (widget) {
        widget->setError(error);
    }
}

void NotificationListWidget::onListContextMenu(const QPoint &pos) {
    QListWidgetItem *item = listWidget->itemAt(pos);
    if (item) {
        NotificationItemWidget *widget = qobject_cast<NotificationItemWidget *>(listWidget->itemWidget(item));
        if (widget && widget->isLoading()) return;

        listWidget->setCurrentItem(item);

        QJsonObject json = item->data(Qt::UserRole + 4).toJsonObject();
        Notification n = Notification::fromJson(json);

        if (markAsReadAction) markAsReadAction->setVisible(n.unread);

        contextMenu->exec(listWidget->mapToGlobal(pos));
    }
}

void NotificationListWidget::onItemActivated(QListWidgetItem *item) {
    NotificationItemWidget *widget = qobject_cast<NotificationItemWidget *>(listWidget->itemWidget(item));
    if (widget && widget->isLoading()) return;

    QString apiUrl = item->data(Qt::UserRole).toString();
    QString id = item->data(Qt::UserRole + 1).toString();

    emit markAsRead(id);

    if (widget) {
        widget->setRead(true);
    }

    QJsonObject json = item->data(Qt::UserRole + 4).toJsonObject();
    Notification n = Notification::fromJson(json);
    n.unread = false;
    n.inInbox = false;
    item->setData(Qt::UserRole + 4, n.toJson());

    QFont font = item->font();
    font.setBold(false);
    item->setFont(font);

    QString htmlUrl = GitHubClient::apiToHtmlUrl(apiUrl, id);
    emit linkActivated(QUrl(htmlUrl));
    QDesktopServices::openUrl(QUrl(htmlUrl));

    if (m_filterMode == 0 || m_filterMode == 1) {
        delete listWidget->takeItem(listWidget->row(item));
    }
}

void NotificationListWidget::triggerLoadMore() {
    if (!loadMoreItem) return;

    QPushButton *btn = qobject_cast<QPushButton *>(listWidget->itemWidget(loadMoreItem));
    if (btn && btn->isEnabled()) {
        btn->setEnabled(false);
        btn->setText(tr("Loading..."));
        emit loadMoreRequested();
    }
}

void NotificationListWidget::onLoadMoreClicked() {
    triggerLoadMore();
}

void NotificationListWidget::insertNotificationItem(int row, const Notification &n) {
    QListWidgetItem *item = new QListWidgetItem();
    NotificationItemWidget *widget = new NotificationItemWidget(n);

    if (detailsCache.contains(n.id)) {
        const NotificationDetails &details = detailsCache[n.id];
        if (details.hasDetails) {
            widget->setAuthor(details.author, details.avatar);
            widget->setHtmlUrl(details.htmlUrl);
        }
    } else {
        emit requestDetails(n.url, n.id);
    }

    item->setData(Qt::UserRole, n.url);
    item->setData(Qt::UserRole + 1, n.id);
    item->setData(Qt::UserRole + 2, n.title);
    item->setData(Qt::UserRole + 3, n.repository);
    item->setData(Qt::UserRole + 4, n.toJson());

    QSize hint = widget->sizeHint();
    if (hint.height() < 60) hint.setHeight(60);
    item->setSizeHint(hint);

    widget->setRead(!n.unread);

    QPointer<NotificationItemWidget> safeWidget(widget);
    connect(widget, &NotificationItemWidget::openClicked, this, [this, item]() {
        onItemActivated(item);
    });

    connect(widget, &NotificationItemWidget::doneClicked, this, [this, item, safeWidget]() {
        if (!safeWidget) return;
        listWidget->setCurrentItem(item);
        dismissCurrentItem();
    }, Qt::QueuedConnection);

    listWidget->insertItem(row, item);
    listWidget->setItemWidget(item, widget);
}

void NotificationListWidget::updateList() {
    listWidget->setUpdatesEnabled(false);
    emit statusMessage(tr("Updating list..."));

    // Prepare Target List
    QList<Notification> targetNotifications;
    for (const Notification &n : m_allNotifications) {
        bool show = false;
        if (m_filterMode == 0) { // Inbox
            if (n.inInbox) show = true;
        } else if (m_filterMode == 1) { // Unread
            if (n.inInbox && n.unread) show = true;
        } else if (m_filterMode == 2) { // Read
            if (!n.inInbox) show = true;
        }
        if (show) {
            targetNotifications.append(n);
        }
    }

    // Sync Loop
    int i = 0;
    while (i < targetNotifications.size()) {
        const Notification &n = targetNotifications[i];
        QListWidgetItem *currentItem = listWidget->item(i);

        // Check if current item matches target
        QString currentId = currentItem ? currentItem->data(Qt::UserRole + 1).toString() : QString();

        // Skip "Load More" item if encountered during sync (should be at end)
        if (currentItem == loadMoreItem && loadMoreItem != nullptr) {
             // If we hit load more item but still have notifications to place,
             // we need to insert before it.
             currentId = ""; // Treat as mismatch
        }

        if (currentItem && currentId == n.id) {
            // Match: Update existing
            NotificationItemWidget *widget = qobject_cast<NotificationItemWidget *>(listWidget->itemWidget(currentItem));
            if (widget) {
                widget->updateNotification(n);
            }
            // Update Item Data
            currentItem->setData(Qt::UserRole + 4, n.toJson());
            // Font update is handled in updateNotification -> setRead
            i++;
        } else {
            // Mismatch
            // Check if n.id exists later in the list
            int foundIndex = -1;
            for (int k = i + 1; k < listWidget->count(); ++k) {
                QListWidgetItem *searchItem = listWidget->item(k);
                if (searchItem == loadMoreItem) continue;
                if (searchItem->data(Qt::UserRole + 1).toString() == n.id) {
                    foundIndex = k;
                    break;
                }
            }

            if (foundIndex != -1) {
                // Found later: Move it to current position 'i'
                // Safely remove old item and widget
                QWidget *widget = listWidget->itemWidget(listWidget->item(foundIndex));
                if (widget) {
                    widget->deleteLater();
                }
                QListWidgetItem *movedItem = listWidget->takeItem(foundIndex);
                delete movedItem;

                insertNotificationItem(i, n);
            } else {
                // Not found: Insert new
                insertNotificationItem(i, n);
            }
            i++;
        }
    }

    // Cleanup: Remove remaining items starting from i (excluding loadMoreItem if we want to keep it logic clean)
    // We iterate backwards to avoid index shifting issues
    for (int k = listWidget->count() - 1; k >= i; --k) {
        QListWidgetItem *item = listWidget->item(k);
        if (item == loadMoreItem) {
            // Handle loadMoreItem logic below, don't delete here unless we reset it
            continue;
        }
        QWidget *w = listWidget->itemWidget(item);
        if (w) w->deleteLater();
        delete listWidget->takeItem(k);
    }

    // Handle Load More Item
    // It should be at the end if m_hasMore is true.
    if (m_hasMore) {
        if (!loadMoreItem) {
            loadMoreItem = new QListWidgetItem();
            QPushButton *loadMoreBtn = new QPushButton(tr("Load More"));
            connect(loadMoreBtn, &QPushButton::clicked, this, &NotificationListWidget::onLoadMoreClicked);

            loadMoreItem->setSizeHint(loadMoreBtn->sizeHint());
            loadMoreItem->setFlags(Qt::NoItemFlags);

            listWidget->addItem(loadMoreItem);
            listWidget->setItemWidget(loadMoreItem, loadMoreBtn);
        } else {
             // Ensure it is at the very end
             int r = listWidget->row(loadMoreItem);
             if (r != listWidget->count() - 1) {
                 QWidget *w = listWidget->itemWidget(loadMoreItem);
                 if(w) w->deleteLater();
                 listWidget->takeItem(r);
                 delete loadMoreItem; // Delete old pointer

                 loadMoreItem = new QListWidgetItem();
                 QPushButton *loadMoreBtn = new QPushButton(tr("Load More"));
                 connect(loadMoreBtn, &QPushButton::clicked, this, &NotificationListWidget::onLoadMoreClicked);

                 loadMoreItem->setSizeHint(loadMoreBtn->sizeHint());
                 loadMoreItem->setFlags(Qt::NoItemFlags);

                 listWidget->addItem(loadMoreItem);
                 listWidget->setItemWidget(loadMoreItem, loadMoreBtn);
             }
        }
    } else {
        if (loadMoreItem) {
            int r = listWidget->row(loadMoreItem);
            if (r >= 0) {
                 QWidget *w = listWidget->itemWidget(loadMoreItem);
                 if(w) w->deleteLater();
                 delete listWidget->takeItem(r);
            }
            loadMoreItem = nullptr;
        }
    }

    applyClientFilters();

    listWidget->setUpdatesEnabled(true);
    emit statusMessage(tr("Items: %1").arg(listWidget->count() - (loadMoreItem ? 1 : 0)));

    QTimer::singleShot(0, this, &NotificationListWidget::handleLoadMoreStrategy);
}

void NotificationListWidget::applyClientFilters() {
    bool filterRepo = (m_repoFilter != tr("All Repositories") && !m_repoFilter.isEmpty());
    bool filterText = !m_searchFilter.isEmpty();

    int visibleCount = 0;

    for (int i = 0; i < listWidget->count(); ++i) {
        QListWidgetItem *item = listWidget->item(i);
        if (item == loadMoreItem) continue;

        bool matchRepo = true;
        if (filterRepo) {
            QString repo = item->data(Qt::UserRole + 3).toString();
            if (repo != m_repoFilter) matchRepo = false;
        }

        bool matchText = true;
        if (filterText) {
            QString title = item->data(Qt::UserRole + 2).toString();
            QString repo = item->data(Qt::UserRole + 3).toString();
            if (!title.contains(m_searchFilter, Qt::CaseInsensitive) &&
                !repo.contains(m_searchFilter, Qt::CaseInsensitive)) {
                matchText = false;
            }
        }

        bool visible = matchRepo && matchText;
        item->setHidden(!visible);
        if (visible) visibleCount++;
    }

    emit statusMessage(tr("Items: %1").arg(visibleCount));
}

NotificationItemWidget *NotificationListWidget::findNotificationWidget(const QString &id) {
    for (int i = 0; i < listWidget->count(); ++i) {
        QListWidgetItem *item = listWidget->item(i);
        if (item->data(Qt::UserRole + 1).toString() == id) {
            return qobject_cast<NotificationItemWidget *>(listWidget->itemWidget(item));
        }
    }
    return nullptr;
}

void NotificationListWidget::dismissCurrentItem() {
    QListWidgetItem *item = listWidget->currentItem();
    if (!item) return;

    NotificationItemWidget *widget = qobject_cast<NotificationItemWidget *>(listWidget->itemWidget(item));
    if (widget && widget->isLoading()) return;
    if (widget) widget->setLoading(true);

    QString id = item->data(Qt::UserRole + 1).toString();
    QJsonObject json = item->data(Qt::UserRole + 4).toJsonObject();
    Notification n = Notification::fromJson(json);

    n.unread = false;
    n.inInbox = false;
    item->setData(Qt::UserRole + 4, n.toJson());
    QFont font = item->font();
    font.setBold(false);
    item->setFont(font);

    emit markAsDone(id);
    knownNotificationIds.remove(id);

    delete listWidget->takeItem(listWidget->row(item));
}

void NotificationListWidget::openCurrentItem() {
    QListWidgetItem *item = listWidget->currentItem();
    if (item) {
        onItemActivated(item);
    }
}

void NotificationListWidget::copyLinkCurrentItem() {
    QListWidgetItem *item = listWidget->currentItem();
    if (item) {
        QString apiUrl = item->data(Qt::UserRole).toString();
        QString htmlUrl = GitHubClient::apiToHtmlUrl(apiUrl);
        QApplication::clipboard()->setText(htmlUrl);
    }
}

QList<Notification> NotificationListWidget::getUnreadNotifications(int limit) const {
    QList<Notification> unread;
    for (const auto &n : m_allNotifications) {
        if (n.unread) {
            unread.append(n);
            if (unread.count() >= limit) break;
        }
    }
    return unread;
}
