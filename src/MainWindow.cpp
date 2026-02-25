#include "MainWindow.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QCoreApplication>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDate>
#include <QDebug>
#include <QDesktopServices>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QMenuBar>
#include <QMessageBox>
#include <QProcess>
#include <QScreen>
#include <QSettings>
#include <QSpinBox>
#include <QStyle>
#include <QUrl>
#include <QVBoxLayout>
#include <limits>

#include <QSvgRenderer>
#include <QPainter>

#include "NotificationItemWidget.h"
#include "SettingsDialog.h"

static int calculateSafeInterval(int minutes) {
    if (minutes <= 0) minutes = 1;  // Minimum 1 minute
    qint64 msec = static_cast<qint64>(minutes) * 60 * 1000;
    if (msec > std::numeric_limits<int>::max()) {
        return std::numeric_limits<int>::max();
    }
    return static_cast<int>(msec);
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      client(nullptr),
      loadMoreItem(nullptr),
      m_isManualRefresh(false),
      pendingAuthError(false),
      authNotification(nullptr) {
    setupWindow();
    setupCentralWidget();
    setupNotificationList();
    setupToolbar();
    setupPages();
    createTrayIcon();
    setupMenus();
    setupStatusBar();

    // Initial State Check
    stackWidget->setCurrentWidget(loadingPage);

    loadSavedNotifications();
    loadDoneNotifications();
    loadToken();
}

void MainWindow::onTokenLoaded() {
    m_loadedToken = tokenWatcher->result();

    if (m_loadedToken.isEmpty()) {
        stackWidget->setCurrentWidget(loginPage);
    } else {
        stackWidget->setCurrentWidget(notificationList);
        if (client) {
            client->setToken(m_loadedToken);
            client->checkNotifications();
        }
    }
}

MainWindow::~MainWindow() {
    QSettings settings;
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());
}

void MainWindow::createErrorPage() {
    errorPage = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(errorPage);
    layout->setAlignment(Qt::AlignCenter);

    errorLabel = new QLabel(tr("Authentication Error"), errorPage);
    errorLabel->setWordWrap(true);
    errorLabel->setAlignment(Qt::AlignCenter);

    settingsButton = new QPushButton(tr("Open Settings"), errorPage);
    connect(settingsButton, &QPushButton::clicked, this, &MainWindow::showSettings);

    layout->addWidget(errorLabel);
    layout->addWidget(settingsButton);
}

void MainWindow::createLoginPage() {
    loginPage = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(loginPage);
    layout->setAlignment(Qt::AlignCenter);

    loginLabel = new QLabel(
        tr("Welcome to Kgithub-notify!\n\nPlease configure your Personal Access Token (PAT) to get started."),
        loginPage);
    loginLabel->setWordWrap(true);
    loginLabel->setAlignment(Qt::AlignCenter);

    loginButton = new QPushButton(tr("Open Settings"), loginPage);
    connect(loginButton, &QPushButton::clicked, this, &MainWindow::showSettings);

    layout->addWidget(loginLabel);
    layout->addWidget(loginButton);
}

void MainWindow::createEmptyStatePage() {
    emptyStatePage = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(emptyStatePage);
    layout->setAlignment(Qt::AlignCenter);

    emptyStateLabel = new QLabel(tr("No new notifications"), emptyStatePage);
    emptyStateLabel->setAlignment(Qt::AlignCenter);

    layout->addWidget(emptyStateLabel);
}

void MainWindow::createLoadingPage() {
    loadingPage = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(loadingPage);
    layout->setAlignment(Qt::AlignCenter);

    loadingLabel = new QLabel(tr("Loading..."), loadingPage);
    loadingLabel->setAlignment(Qt::AlignCenter);

    layout->addWidget(loadingLabel);
}

void MainWindow::setClient(GitHubClient *c) {
    client = c;
    connect(client, &GitHubClient::loadingStarted, this, &MainWindow::onLoadingStarted);
    connect(client, &GitHubClient::notificationsReceived, this, &MainWindow::updateNotifications);
    connect(client, &GitHubClient::errorOccurred, this, &MainWindow::showError);
    connect(client, &GitHubClient::authError, this, &MainWindow::onAuthError);

    connect(client, &GitHubClient::detailsError, this, [this](const QString &id, const QString &error){
        NotificationItemWidget *widget = findNotificationWidget(id);
        if (widget) {
            widget->setError(error);
        }
    });

    connect(client, &GitHubClient::detailsReceived, this, [this](const QString &id, const QString &author, const QString &avatarUrl, const QString &htmlUrl){
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
                    if (client) client->fetchImage(avatarUrl, id);
                }
            });

    connect(client, &GitHubClient::imageReceived, this, [this](const QString &id, const QPixmap &pixmap) {
        NotificationDetails &details = detailsCache[id];
        details.avatar = pixmap;
        details.hasImage = true;

        NotificationItemWidget *widget = findNotificationWidget(id);
        if (widget) {
            widget->setAuthor(details.author, pixmap);
        }
    });

    if (refreshTimer) {
        connect(refreshTimer, &QTimer::timeout, client, &GitHubClient::checkNotifications);
        int interval = SettingsDialog::getInterval();
        refreshTimer->setInterval(calculateSafeInterval(interval));
        refreshTimer->start();
    }

    if (!m_loadedToken.isEmpty()) {
        client->setToken(m_loadedToken);
        client->checkNotifications();
    }
}

QIcon MainWindow::themedIcon(const QStringList &names, const QString &fallbackResource,
                             QStyle::StandardPixmap fallbackPixmap) const {
    for (const QString &name : names) {
        QIcon icon = QIcon::fromTheme(name);
        if (!icon.isNull()) {
            return icon;
        }
    }

    if (!fallbackResource.isEmpty()) {
        QIcon icon(fallbackResource);
        if (!icon.isNull()) {
            return icon;
        }
    }

    return QApplication::style()->standardIcon(fallbackPixmap);
}

QIcon MainWindow::loadSvgIcon(const QString &path) {
    QSvgRenderer renderer(path);
    if (!renderer.isValid()) {
        return QIcon(path);
    }

    QIcon icon;
    for (int size : {16, 22, 24, 32, 48, 64, 128, 256}) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        renderer.render(&painter);
        icon.addPixmap(pixmap);
    }
    return icon;
}

void MainWindow::createTrayIcon() {
    trayIconMenu = new QMenu(this);
    updateTrayMenu();

    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setContextMenu(trayIconMenu);

    QIcon icon = QIcon::fromTheme("kgithub-notify");
    if (icon.isNull()) {
        icon = loadSvgIcon(":/assets/icon.svg");
    }
    trayIcon->setIcon(icon);

    connect(trayIcon, &QSystemTrayIcon::activated, this, &MainWindow::onTrayIconActivated);
    connect(trayIcon, &QSystemTrayIcon::messageClicked, this, &MainWindow::onTrayMessageClicked);

    trayIcon->show();
}

void MainWindow::updateTrayMenu() {
    if (!trayIconMenu) return;
    trayIconMenu->clear();

    QAction *openAppAction =
        new QAction(themedIcon({QStringLiteral("kgithub-notify")}),
                    tr("Open Kgithub-notify"), trayIconMenu);
    QFont font = openAppAction->font();
    font.setBold(true);
    openAppAction->setFont(font);
    connect(openAppAction, &QAction::triggered, this, &QWidget::showNormal);
    trayIconMenu->addAction(openAppAction);

    trayIconMenu->addSeparator();

    int unreadCount = 0;
    QList<QListWidgetItem *> unreadItems;
    for (int i = 0; i < notificationList->count(); ++i) {
        QListWidgetItem *item = notificationList->item(i);
        if (item->font().bold()) {
            unreadCount++;
            unreadItems.append(item);
        }
    }

    if (unreadCount > 0) {
        QAction *header = new QAction(tr("%1 Unread Notifications").arg(unreadCount), trayIconMenu);
        header->setEnabled(false);
        trayIconMenu->addAction(header);

        int limit = qMin(unreadCount, 5);
        for (int i = 0; i < limit; ++i) {
            QListWidgetItem *item = unreadItems[i];
            QString title = item->data(Qt::UserRole + 2).toString();
            QString repo = item->data(Qt::UserRole + 3).toString();
            QString apiUrl = item->data(Qt::UserRole).toString();
            QString id = item->data(Qt::UserRole + 1).toString();

            QString label = QString("%1: %2").arg(repo, title);

            QAction *itemAction = new QAction(label, trayIconMenu);
            connect(itemAction, &QAction::triggered, [this, apiUrl, id]() {
                if (client) client->markAsRead(id);
                QString htmlUrl = GitHubClient::apiToHtmlUrl(apiUrl, id);
                QDesktopServices::openUrl(QUrl(htmlUrl));

                // Update local state and refresh menu
                for (int i = 0; i < notificationList->count(); ++i) {
                    QListWidgetItem *item = notificationList->item(i);
                    if (item->data(Qt::UserRole + 1).toString() == id) {
                        QFont font = item->font();
                        font.setBold(false);
                        item->setFont(font);
                        QTimer::singleShot(0, this, &MainWindow::updateTrayMenu);
                        break;
                    }
                }
            });
            trayIconMenu->addAction(itemAction);
        }

        trayIconMenu->addSeparator();

        QAction *dismissAllAction = new QAction(tr("Dismiss All"), trayIconMenu);
        connect(dismissAllAction, &QAction::triggered, this, &MainWindow::dismissAllNotifications);
        trayIconMenu->addAction(dismissAllAction);
    } else {
        QAction *empty = new QAction(tr("No new notifications"), trayIconMenu);
        empty->setEnabled(false);
        trayIconMenu->addAction(empty);
    }

    trayIconMenu->addSeparator();

    QAction *trayRefreshAction =
        new QAction(themedIcon({QStringLiteral("view-refresh")}), tr("Force Refresh"), trayIconMenu);
    connect(trayRefreshAction, &QAction::triggered, this, &MainWindow::onRefreshClicked);
    trayIconMenu->addAction(trayRefreshAction);

    QAction *settingsAction =
        new QAction(themedIcon({QStringLiteral("settings-configure")}), tr("Settings"), trayIconMenu);
    connect(settingsAction, &QAction::triggered, this, &MainWindow::showSettings);
    trayIconMenu->addAction(settingsAction);

    QAction *notificationSettingsAction = new QAction(themedIcon({QStringLiteral("preferences-desktop-notification")}),
                                                      tr("Configure Notifications"), trayIconMenu);
    connect(notificationSettingsAction, &QAction::triggered, this, &MainWindow::openKdeNotificationSettings);
    trayIconMenu->addAction(notificationSettingsAction);

    trayIconMenu->addSeparator();

    QAction *quitAction = new QAction(themedIcon({QStringLiteral("application-exit")}), tr("Quit"), trayIconMenu);
    connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);
    trayIconMenu->addAction(quitAction);

    updateTrayToolTip();
}

void MainWindow::updateTrayToolTip() {
    if (!trayIcon) return;

    QStringList parts;
    parts << tr("KGitHub Notify");

    if (!lastError.isEmpty()) {
        parts << tr("Status: Error");
    } else if (stackWidget->currentWidget() == loadingPage) {
        parts << tr("Status: Checking...");
    } else {
        parts << tr("Status: OK");
    }

    int unreadCount = 0;
    QStringList unreadTitles;
    if (notificationList) {
        for (int i = 0; i < notificationList->count(); ++i) {
            QListWidgetItem *item = notificationList->item(i);
            if (item && item->font().bold()) {
                unreadCount++;
                if (unreadTitles.count() < 5) {
                    QString title = item->data(Qt::UserRole + 2).toString();
                    QString repo = item->data(Qt::UserRole + 3).toString();
                    unreadTitles << QStringLiteral("- %1: %2").arg(repo, title);
                }
            }
        }
    }
    parts << tr("Unread: %1").arg(unreadCount);
    if (!unreadTitles.isEmpty()) {
        parts << unreadTitles;
        if (unreadCount > 5) {
            parts << tr("... and %1 more").arg(unreadCount - 5);
        }
    }

    if (m_lastCheckTime.isValid()) {
        parts << tr("Last Check: %1").arg(QLocale::system().toString(m_lastCheckTime, QLocale::ShortFormat));

        if (refreshTimer && refreshTimer->isActive()) {
            QDateTime nextCheck = m_lastCheckTime.addMSecs(refreshTimer->interval());
            parts << tr("Next Check: %1").arg(QLocale::system().toString(nextCheck, QLocale::ShortFormat));
        }
    } else {
        parts << tr("Last Check: Never");
    }

    trayIcon->setToolTip(parts.join(QStringLiteral("\n")));
}

void MainWindow::dismissAllNotifications() {
    onSelectAllClicked();
    onDismissSelectedClicked();
}

void MainWindow::onTrayIconActivated(QSystemTrayIcon::ActivationReason reason) {
    if (reason == QSystemTrayIcon::Trigger) {
        if (isVisible()) {
            hide();
        } else {
            ensureWindowActive();
        }
    } else if (reason == QSystemTrayIcon::Context) {
        if (trayIcon->contextMenu()) {
            trayIcon->contextMenu()->exec(QCursor::pos());
        }
    }
}

void MainWindow::updateNotifications(const QList<Notification> &notifications, bool append, bool hasMore) {
    m_lastCheckTime = QDateTime::currentDateTime();
    pendingAuthError = false;
    lastError.clear();

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

    if (unreadCount > 0) {
        trayIcon->setIcon(QIcon(":/assets/icon-dotted.svg"));
        if (newNotifications > 0) {
            if (newNotifications == 1) {
                sendNotification(newlyAddedNotifications.first());
            } else {
                sendSummaryNotification(newNotifications, newlyAddedNotifications);
            }
        }
    } else {
        trayIcon->setIcon(themedIcon({QStringLiteral("kgithub-notify")},
                                   QStringLiteral(":/assets/icon.svg"), QStyle::SP_ComputerIcon));
    }
    updateTrayMenu();

    // If in Saved view, do not update list
    if (filterComboBox && filterComboBox->currentIndex() == 1) {
        return;
    }

    // If in Done view, do not update list
    if (filterComboBox && filterComboBox->currentIndex() == 2) {
        return;
    }


    if (authNotification && authNotification->isVisible()) {
        authNotification->close();
    }

    // Switch to list view on successful update
    if (!append && notifications.isEmpty()) {
        if (stackWidget->currentWidget() != emptyStatePage) {
            stackWidget->setCurrentWidget(emptyStatePage);
        }
    } else {
        if (stackWidget->currentWidget() != notificationList) {
            stackWidget->setCurrentWidget(notificationList);
        }
    }

    notificationList->setUpdatesEnabled(false);

    if (loadMoreItem) {
        int row = notificationList->row(loadMoreItem);
        if (row >= 0) {
            delete notificationList->takeItem(row);
        }
        loadMoreItem = nullptr;
    }

    if (!append) {
        notificationList->clear();
        loadMoreItem = nullptr;
    }

    if (loadingLabel && !append) loadingLabel->setText(tr("Loading...")); // Reset loading text if it was error

    for (const Notification &n : notifications) {
        if (isNotificationDone(n.id)) continue;
        addNotificationItem(n);
    }

    if (hasMore) {
        loadMoreItem = new QListWidgetItem();
        QPushButton *loadMoreBtn = new QPushButton(tr("Load More"));
        connect(loadMoreBtn, &QPushButton::clicked, this, [this, loadMoreBtn]() {
            loadMoreBtn->setEnabled(false);
            loadMoreBtn->setText(tr("Loading..."));
            if (client) client->loadMore();
        });

        loadMoreItem->setSizeHint(loadMoreBtn->sizeHint());
        loadMoreItem->setFlags(Qt::NoItemFlags);

        notificationList->addItem(loadMoreItem);
        notificationList->setItemWidget(loadMoreItem, loadMoreBtn);
    }

    notificationList->setUpdatesEnabled(true);

    updateSelectionComboBox();

    if (countLabel) {
        countLabel->setText(tr("Items: %1").arg(notificationList->count()));
    }

    if (unreadCount > 0) {
        trayIcon->setIcon(loadSvgIcon(":/assets/icon-dotted.svg"));
        if (newNotifications > 0) {
            if (newNotifications == 1) {
                sendNotification(newlyAddedNotifications.first());
            } else {
                sendSummaryNotification(newNotifications, newlyAddedNotifications);
            }
        }
    } else {
        QIcon icon = QIcon::fromTheme("kgithub-notify");
        if (icon.isNull()) {
            icon = loadSvgIcon(":/assets/icon.svg");
        }
        trayIcon->setIcon(icon);
    }
    updateTrayMenu();
}

void MainWindow::onAuthNotificationSettingsClicked() {
    if (authNotification) {
        authNotification->close();
    }
    showSettings();
}

void MainWindow::onNotificationItemActivated(QListWidgetItem *item) {
    QString apiUrl = item->data(Qt::UserRole).toString();
    QString id = item->data(Qt::UserRole + 1).toString();

    if (client) {
        client->markAsRead(id);
    }

    // Visual update
    NotificationItemWidget *widget = qobject_cast<NotificationItemWidget *>(notificationList->itemWidget(item));
    if (widget) {
        widget->setRead(true);
    }

    // Update internal data
    QJsonObject json = item->data(Qt::UserRole + 4).toJsonObject();
    Notification n = Notification::fromJson(json);
    n.unread = false;
    item->setData(Qt::UserRole + 4, n.toJson());

    QFont font = item->font();
    font.setBold(false);
    item->setFont(font);

    updateTrayMenu();

    QString htmlUrl = GitHubClient::apiToHtmlUrl(apiUrl, id);
    QDesktopServices::openUrl(QUrl(htmlUrl));
}

void MainWindow::openCurrentItem() {
    QListWidgetItem *item = notificationList->currentItem();
    if (item) {
        onNotificationItemActivated(item);
    }
}

void MainWindow::copyLinkCurrentItem() {
    QListWidgetItem *item = notificationList->currentItem();
    if (item) {
        QString apiUrl = item->data(Qt::UserRole).toString();
        // Don't include notification_referrer_id for copied links
        QString htmlUrl = GitHubClient::apiToHtmlUrl(apiUrl);
        QApplication::clipboard()->setText(htmlUrl);
    }
}

void MainWindow::showTrayMessage(const QString &title, const QString &message) {
    trayIcon->showMessage(title, message, QSystemTrayIcon::Information, 3000);
}

void MainWindow::showSettings() {
    SettingsDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        QString newToken = dialog.getToken();
        int interval = SettingsDialog::getInterval();
        if (client) {
            client->setToken(newToken);
            m_isManualRefresh = true;
            client->checkNotifications();  // Force check immediately
            m_isManualRefresh = false;
        }
        if (refreshTimer) {
            refreshTimer->setInterval(calculateSafeInterval(interval));
            refreshTimer->start();
            updateStatusBar();
        }
    }
}

void MainWindow::onLoadingStarted() {
    if (!notificationList) return;
    if (notificationList->count() == 0 || m_isManualRefresh) {
        if (stackWidget->currentWidget() != loadingPage) {
            stackWidget->setCurrentWidget(loadingPage);
        }
        if (loadingLabel) {
            loadingLabel->setText(tr("Loading..."));
        }
    }
    updateTrayToolTip();
}

void MainWindow::onAuthError(const QString &message) {
    pendingAuthError = true;

    // Update error page
    errorLabel->setText(tr("Authentication Error: %1\n\nPlease update your token in Settings.").arg(message));
    stackWidget->setCurrentWidget(errorPage);

    if (!authNotification) {
        authNotification = new AuthErrorNotification(this);
        connect(authNotification, &AuthErrorNotification::settingsClicked, this,
                &MainWindow::onAuthNotificationSettingsClicked);
        // dismissed signal is handled by AuthErrorNotification internally closing itself, but we can hook if needed.
    }

    authNotification->setMessage(message);

    positionPopup(authNotification);
    authNotification->show();

    if (!trayIcon || !trayIcon->isVisible()) {
        // If tray not visible, show settings or ensure window is visible
        if (isVisible()) {
            showSettings();
        }
    }
}

void MainWindow::onTrayMessageClicked() {
    if (pendingAuthError) {
        showSettings();
        // Don't clear pendingAuthError here, wait for successful update
    }
}

void MainWindow::showContextMenu(const QPoint &pos) {
    QListWidgetItem *item = notificationList->itemAt(pos);
    if (item) {
        notificationList->setCurrentItem(item);  // Ensure item is selected

        QString id = item->data(Qt::UserRole + 1).toString();
        bool saved = isNotificationSaved(id);

        QJsonObject json = item->data(Qt::UserRole + 4).toJsonObject();
        Notification n = Notification::fromJson(json);

        if (markAsReadAction) markAsReadAction->setVisible(n.unread);
        if (saveAction) saveAction->setVisible(!saved);
        if (unsaveAction) unsaveAction->setVisible(saved);

        contextMenu->exec(notificationList->mapToGlobal(pos));
    }
}

void MainWindow::dismissCurrentItem() {
    QListWidgetItem *item = notificationList->currentItem();
    if (!item) return;

    QString id = item->data(Qt::UserRole + 1).toString();
    QJsonObject json = item->data(Qt::UserRole + 4).toJsonObject();
    Notification n = Notification::fromJson(json);

    saveDoneNotification(n);

    if (client) {
        client->markAsRead(id);
        client->markAsDone(id);
    }

    knownNotificationIds.remove(id);

    // Only remove from list if in Inbox
    if (filterComboBox && filterComboBox->currentIndex() == 0) {
        delete notificationList->takeItem(notificationList->row(item));

        // Update icon if list is empty
        if (notificationList->count() == 0) {
            trayIcon->setIcon(themedIcon({QStringLiteral("kgithub-notify")},
                                       QStringLiteral(":/assets/icon.svg"), QStyle::SP_ComputerIcon));
        }
    }
    updateSelectionComboBox();

    updateTrayMenu();
}

void MainWindow::showError(const QString &error) {
    if (error == lastError) return;
    lastError = error;

    // Only show error via tray if visible, otherwise standard message box (but avoid spamming boxes)
    if (trayIcon && trayIcon->isVisible()) {
        trayIcon->showMessage(tr("GitHub Error"), error, QSystemTrayIcon::Warning, 5000);
    } else {
        // Maybe don't show message box on every polling error if window is hidden?
        // But if window is visible, we should show it.
        if (isVisible()) {
            QMessageBox::warning(this, tr("GitHub Error"), error);
        }
    }

    if (stackWidget->currentWidget() == loadingPage) {
        if (notificationList->count() > 0) {
            stackWidget->setCurrentWidget(notificationList);
        } else {
            if (loadingLabel) {
                loadingLabel->setText(tr("Error: %1").arg(error));
            }
        }
    }

    // Reset load more button if error occurred during loading more
    loadMoreButton->setEnabled(true);
    loadMoreButton->setText(tr("Load More"));
    updateTrayToolTip();
    if (loadMoreItem) {
        QPushButton *btn = qobject_cast<QPushButton*>(notificationList->itemWidget(loadMoreItem));
        if (btn) {
            btn->setEnabled(true);
            btn->setText(tr("Load More"));
        }
    }
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (trayIcon->isVisible()) {
        hide();
        event->ignore();
    }
}

void MainWindow::onRefreshClicked() {
    if (client) {
        m_isManualRefresh = true;
        client->checkNotifications();
        m_isManualRefresh = false;
        if (refreshTimer) {
            refreshTimer->start();  // Restart timer
            updateStatusBar();      // Force update
        }
    }
}

void MainWindow::onFilterChanged(int index) {
    if (client) {
        m_isManualRefresh = true;
        if (index == 0) {
            // Inbox: Show All (Read + Unread)
            client->setShowAll(true);
            client->checkNotifications();
        } else if (index == 1) {
            // Saved view
            if (m_savedNotifications.isEmpty()) {
                stackWidget->setCurrentWidget(emptyStatePage);
            } else {
                stackWidget->setCurrentWidget(notificationList);
            }

            notificationList->setUpdatesEnabled(false);
            notificationList->clear();
            loadMoreItem = nullptr;

            for (const Notification &n : m_savedNotifications) {
                addNotificationItem(n);
            }
            notificationList->setUpdatesEnabled(true);

            if (countLabel) {
                countLabel->setText(tr("Items: %1").arg(m_savedNotifications.count()));
            }
        } else if (index == 2) {
            // Done view
            if (m_doneNotifications.isEmpty()) {
                stackWidget->setCurrentWidget(emptyStatePage);
            } else {
                stackWidget->setCurrentWidget(notificationList);
            }

            notificationList->setUpdatesEnabled(false);
            notificationList->clear();
            loadMoreItem = nullptr;

            for (const Notification &n : m_doneNotifications) {
                addNotificationItem(n);
            }
            notificationList->setUpdatesEnabled(true);

            updateSelectionComboBox();

            if (countLabel) {
                countLabel->setText(tr("Items: %1").arg(m_doneNotifications.count()));
            }
        }
        m_isManualRefresh = false;
        if (refreshTimer) {
            refreshTimer->start();
            updateStatusBar();
        }
    }
}

void MainWindow::onSelectAllClicked() {
    if (notificationList) {
        notificationList->selectAll();
    }
}

void MainWindow::onSelectNoneClicked() {
    if (notificationList) {
        notificationList->clearSelection();
    }
}

void MainWindow::updateSelectionComboBox() {
    if (!selectionComboBox || !notificationList) return;

    int count = notificationList->count();
    bool wasBlocked = selectionComboBox->blockSignals(true);
    selectionComboBox->clear();
    selectionComboBox->addItem(tr("Select..."));

    if (count > 0) {
        if (count >= 5) selectionComboBox->addItem(tr("Top 5"), 5);
        if (count >= 10) selectionComboBox->addItem(tr("Top 10"), 10);
        if (count >= 20) selectionComboBox->addItem(tr("Top 20"), 20);
        if (count >= 50) selectionComboBox->addItem(tr("Top 50"), 50);
        selectionComboBox->addItem(tr("All (%1)").arg(count), count);
    }

    selectionComboBox->setCurrentIndex(0);
    selectionComboBox->blockSignals(wasBlocked);
}

void MainWindow::onSelectionChanged(int index) {
    if (index <= 0) return;

    int n = selectionComboBox->currentData().toInt();
    if (n <= 0) return;

    notificationList->clearSelection();
    int count = notificationList->count();
    int limit = qMin(n, count);

    for (int i = 0; i < limit; ++i) {
        QListWidgetItem *item = notificationList->item(i);
        if (item) item->setSelected(true);
    }

    bool wasBlocked = selectionComboBox->blockSignals(true);
    selectionComboBox->setCurrentIndex(0);
    selectionComboBox->blockSignals(wasBlocked);
}

void MainWindow::onDismissSelectedClicked() {
    if (!notificationList || !client) return;

    QList<QListWidgetItem *> items = notificationList->selectedItems();
    for (auto item : items) {
        QString id = item->data(Qt::UserRole + 1).toString();
        QJsonObject json = item->data(Qt::UserRole + 4).toJsonObject();
        Notification n = Notification::fromJson(json);

        saveDoneNotification(n);
        client->markAsRead(id);
        client->markAsDone(id);
        knownNotificationIds.remove(id);

        if (filterComboBox && filterComboBox->currentIndex() == 0) {
            delete notificationList->takeItem(notificationList->row(item));
        }
    }

    // Update icon if list is empty
    if (notificationList->count() == 0 && filterComboBox && filterComboBox->currentIndex() == 0) {
        trayIcon->setIcon(themedIcon({QStringLiteral("kgithub-notify")},
                                   QStringLiteral(":/assets/icon.svg"), QStyle::SP_ComputerIcon));
    }
    updateSelectionComboBox();
    updateTrayMenu();
}

void MainWindow::onOpenSelectedClicked() {
    if (!notificationList) return;

    QList<QListWidgetItem *> items = notificationList->selectedItems();
    for (auto item : items) {
        QString apiUrl = item->data(Qt::UserRole).toString();
        QString id = item->data(Qt::UserRole + 1).toString();
        QString htmlUrl = GitHubClient::apiToHtmlUrl(apiUrl, id);
        QDesktopServices::openUrl(QUrl(htmlUrl));
    }
}


void MainWindow::updateStatusBar() {
    if (refreshTimer && refreshTimer->isActive()) {
        qint64 remaining = refreshTimer->remainingTime();
        if (remaining >= 0) {
            int seconds = (remaining / 1000) % 60;
            int minutes = (remaining / 60000);
            timerLabel->setText(
                tr("Next refresh: %1:%2").arg(minutes, 2, 10, QChar('0')).arg(seconds, 2, 10, QChar('0')));
            return;
        }
    }
    if (timerLabel) {
        timerLabel->setText(tr("Next refresh: --:--"));
    }
}

void MainWindow::positionPopup(QWidget *popup) {
    if (!popup) return;

    QRect screenGeom = QGuiApplication::primaryScreen()->availableGeometry();
    bool positioned = false;

    if (trayIcon && trayIcon->isVisible()) {
        QRect trayGeom = trayIcon->geometry();
        if (!trayGeom.isEmpty()) {
            int x = trayGeom.center().x() - popup->width() / 2;
            int y;

            // If tray is in top half, show below
            if (trayGeom.center().y() < screenGeom.height() / 2) {
                y = trayGeom.bottom() + 10;
            } else {
                // Show above
                y = trayGeom.top() - popup->height() - 10;
            }

            // Clamp X
            if (x < screenGeom.left()) x = screenGeom.left() + 10;
            if (x + popup->width() > screenGeom.right()) x = screenGeom.right() - popup->width() - 10;

            popup->move(x, y);
            positioned = true;
        }
    }

    if (!positioned) {
        // Fallback: Bottom right
        popup->move(screenGeom.bottomRight() - QPoint(popup->width() + 10, popup->height() + 10));
    }
}

void MainWindow::sendNotification(const Notification &n) {
    KNotification *notification = new KNotification("NewNotification");
    notification->setComponentName(QStringLiteral("kgithub-notify"));
    notification->setTitle(n.repository);
    notification->setText(n.title);

    // Actions
    QStringList actions;
    actions << tr("Open") << tr("Dismiss");
    notification->setActions(actions);

    connect(notification, &KNotification::action1Activated, this, [this, n]() {
        QString htmlUrl = GitHubClient::apiToHtmlUrl(n.url, n.id);
        QDesktopServices::openUrl(QUrl(htmlUrl));
    });

    connect(notification, &KNotification::action2Activated, this, [this, n]() {
        if (client) {
            client->markAsRead(n.id);
            // Ideally remove from list immediately too
            for (int i = 0; i < notificationList->count(); ++i) {
                QListWidgetItem *item = notificationList->item(i);
                if (item->data(Qt::UserRole + 1).toString() == n.id) {
                    delete notificationList->takeItem(i);
                    break;
                }
            }
            updateSelectionComboBox();
            updateTrayMenu();
        }
    });

    connect(notification, &KNotification::defaultActivated, this, [this]() { this->ensureWindowActive(); });
    connect(notification, &KNotification::closed, notification, &QObject::deleteLater);

    notification->sendEvent();
}

void MainWindow::sendSummaryNotification(int count, const QList<Notification> &notifications) {
    KNotification *notification = new KNotification("NewNotification");
    notification->setComponentName(QStringLiteral("kgithub-notify"));
    notification->setTitle(tr("%1 New Notifications").arg(count));

    QString summary;
    int limit = qMin(count, 5);
    for (int i = 0; i < limit; ++i) {
        summary += "- " + notifications[i].title + "\n";
    }
    if (count > limit) {
        summary += tr("... and %1 more").arg(count - limit);
    }
    notification->setText(summary.trimmed());

    // Actions
    QStringList actions;
    actions << tr("Open Client");
    notification->setActions(actions);

    connect(notification, &KNotification::action1Activated, this, [this]() { this->ensureWindowActive(); });

    connect(notification, &KNotification::defaultActivated, this, [this]() {
        this->showNormal();
        this->activateWindow();
    });
    connect(notification, &KNotification::closed, notification, &QObject::deleteLater);

    notification->sendEvent();
}

void MainWindow::showAboutDialog() {
    const QString copyright = tr("© %1 Kgithub-notify contributors").arg(QDate::currentDate().year());
    const QString description = tr("A KDE-friendly system tray client for GitHub notifications.");

    QMessageBox aboutBox(this);
    aboutBox.setWindowTitle(tr("About KGitHub Notify"));
    aboutBox.setIconPixmap(themedIcon({QStringLiteral("kgithub-notify")},
                                      QStringLiteral(":/assets/icon.svg"), QStyle::SP_ComputerIcon)
                               .pixmap(64, 64));
    aboutBox.setText(tr("<b>KGitHub Notify</b>"));
    aboutBox.setInformativeText(tr("%1\n\nVersion: %2\n%3\n\nUses Qt, KDE Wallet, and KDE Notifications.")
                                    .arg(description,
                                         QCoreApplication::applicationVersion().isEmpty()
                                             ? QStringLiteral("dev")
                                             : QCoreApplication::applicationVersion(),
                                         copyright));
    aboutBox.setStandardButtons(QMessageBox::Ok);
    aboutBox.exec();
}

void MainWindow::openKdeNotificationSettings() {
    // Try generic systemsettings first (works on Plasma 6 and often 5)
    bool launched = QProcess::startDetached(QStringLiteral("systemsettings"),
                                            {QStringLiteral("kcm_notifications")});

    // Try Plasma 6 kcmshell
    if (!launched) {
        launched = QProcess::startDetached(QStringLiteral("kcmshell6"), {QStringLiteral("kcm_notifications")});
    }

    // Try Plasma 5 systemsettings
    if (!launched) {
        launched = QProcess::startDetached(QStringLiteral("systemsettings5"),
                                            {QStringLiteral("kcm_notifications")});
    }

    // Try Plasma 5 kcmshell
    if (!launched) {
        launched = QProcess::startDetached(QStringLiteral("kcmshell5"), {QStringLiteral("kcm_notifications")});
    }

    // Fallback: Check if generic kcmshell exists
    if (!launched) {
        launched = QProcess::startDetached(QStringLiteral("kcmshell"), {QStringLiteral("kcm_notifications")});
    }

    if (!launched) {
        showTrayMessage(tr("Notification settings unavailable"),
                        tr("Could not launch KDE notification settings on this system."));
    }
}

NotificationItemWidget *MainWindow::findNotificationWidget(const QString &id) {
    if (!notificationList) return nullptr;
    for (int i = 0; i < notificationList->count(); ++i) {
        QListWidgetItem *item = notificationList->item(i);
        if (item->data(Qt::UserRole + 1).toString() == id) {
            return qobject_cast<NotificationItemWidget *>(notificationList->itemWidget(item));
        }
    }
    return nullptr;
}

void MainWindow::ensureWindowActive() {
    showNormal();
    raise();

    // Check if running on Wayland
    if (QGuiApplication::platformName().startsWith(QLatin1String("wayland"), Qt::CaseInsensitive)) {
        // Wayland doesn't support requestActivate(), so we alert the user
        QApplication::alert(this, 0);
    } else {
        activateWindow();
    }
}

void MainWindow::setupWindow() {
    setWindowTitle(tr("Kgithub-notify"));
    setWindowIcon(QIcon(":/assets/icon.svg"));
    resize(800, 600);

    QSettings settings;
    if (settings.contains("geometry")) {
        restoreGeometry(settings.value("geometry").toByteArray());
    }
    if (settings.contains("windowState")) {
        restoreState(settings.value("windowState").toByteArray());
    }
}

void MainWindow::setupCentralWidget() {
    stackWidget = new QStackedWidget(this);
    setCentralWidget(stackWidget);
}

void MainWindow::setupNotificationList() {
    notificationList = new QListWidget(this);
    notificationList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    connect(notificationList, &QListWidget::itemActivated, this, &MainWindow::onNotificationItemActivated);

    // Context menu for list
    notificationList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(notificationList, &QListWidget::customContextMenuRequested, this, &MainWindow::showContextMenu);

    contextMenu = new QMenu(this);

    openAction = new QAction(tr("Open"), this);
    connect(openAction, &QAction::triggered, this, &MainWindow::openCurrentItem);
    contextMenu->addAction(openAction);

    copyLinkAction = new QAction(tr("Copy Link"), this);
    connect(copyLinkAction, &QAction::triggered, this, &MainWindow::copyLinkCurrentItem);
    contextMenu->addAction(copyLinkAction);

    markAsReadAction = new QAction(tr("Mark as Read"), this);
    connect(markAsReadAction, &QAction::triggered, this, [this]() {
        QListWidgetItem *item = notificationList->currentItem();
        if (!item) return;
        QString id = item->data(Qt::UserRole + 1).toString();
        if (client) client->markAsRead(id);

        NotificationItemWidget *widget = qobject_cast<NotificationItemWidget *>(notificationList->itemWidget(item));
        if (widget) {
            widget->setRead(true);
        }

        QJsonObject json = item->data(Qt::UserRole + 4).toJsonObject();
        Notification n = Notification::fromJson(json);
        n.unread = false;
        item->setData(Qt::UserRole + 4, n.toJson());

        QFont font = item->font();
        font.setBold(false);
        item->setFont(font);
        updateTrayMenu();
    });
    contextMenu->addAction(markAsReadAction);

    saveAction = new QAction(tr("Save"), this);
    connect(saveAction, &QAction::triggered, this, [this]() {
        QListWidgetItem *item = notificationList->currentItem();
        if (!item) return;
        QJsonObject json = item->data(Qt::UserRole + 4).toJsonObject();
        Notification n = Notification::fromJson(json);
        saveNotification(n);

        NotificationItemWidget *widget = qobject_cast<NotificationItemWidget *>(notificationList->itemWidget(item));
        if (widget) widget->setSaved(true);
    });
    contextMenu->addAction(saveAction);

    unsaveAction = new QAction(tr("Unsave"), this);
    connect(unsaveAction, &QAction::triggered, this, [this]() {
        QListWidgetItem *item = notificationList->currentItem();
        if (!item) return;
        QString id = item->data(Qt::UserRole + 1).toString();
        unsaveNotification(id);

        NotificationItemWidget *widget = qobject_cast<NotificationItemWidget *>(notificationList->itemWidget(item));
        if (widget) widget->setSaved(false);
    });
    contextMenu->addAction(unsaveAction);

    markAsDoneAction = new QAction(tr("Mark as Done"), this);
    connect(markAsDoneAction, &QAction::triggered, this, &MainWindow::dismissCurrentItem);
    contextMenu->addAction(markAsDoneAction);

    contextMenu->addSeparator();

    dismissAction = new QAction(tr("Dismiss"), this);
    connect(dismissAction, &QAction::triggered, this, &MainWindow::dismissCurrentItem);
    contextMenu->addAction(dismissAction);

    stackWidget->addWidget(notificationList);
}

void MainWindow::setupToolbar() {
    toolbar = new QToolBar(this);
    toolbar->setMovable(false);
    addToolBar(Qt::TopToolBarArea, toolbar);

    refreshAction = new QAction(themedIcon({QStringLiteral("view-refresh")}, QString(), QStyle::SP_BrowserReload),
                                tr("Refresh"), this);
    refreshAction->setShortcut(QKeySequence::Refresh);
    connect(refreshAction, &QAction::triggered, this, &MainWindow::onRefreshClicked);
    toolbar->addAction(refreshAction);

    filterComboBox = new QComboBox(this);
    filterComboBox->addItem(tr("Inbox"));
    filterComboBox->addItem(tr("Saved"));
    filterComboBox->addItem(tr("Done"));
    connect(filterComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onFilterChanged);
    toolbar->addWidget(filterComboBox);

    toolbar->addSeparator();

    selectAllAction = new QAction(tr("Select All"), this);
    selectAllAction->setShortcut(QKeySequence::SelectAll);
    connect(selectAllAction, &QAction::triggered, this, &MainWindow::onSelectAllClicked);
    toolbar->addAction(selectAllAction);

    selectNoneAction = new QAction(tr("Select None"), this);
    selectNoneAction->setShortcut(QKeySequence("Ctrl+Shift+A"));
    connect(selectNoneAction, &QAction::triggered, this, &MainWindow::onSelectNoneClicked);
    toolbar->addAction(selectNoneAction);

    selectionComboBox = new QComboBox(this);
    selectionComboBox->addItem(tr("Select..."));
    connect(selectionComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &MainWindow::onSelectionChanged);
    toolbar->addWidget(selectionComboBox);

    toolbar->addSeparator();

    dismissSelectedAction = new QAction(themedIcon({QStringLiteral("mail-mark-read"), QStringLiteral("edit-delete")},
                                                   QString(), QStyle::SP_DialogDiscardButton),
                                        tr("Dismiss Selected"), this);
    dismissSelectedAction->setShortcut(QKeySequence::Delete);
    connect(dismissSelectedAction, &QAction::triggered, this, &MainWindow::onDismissSelectedClicked);
    toolbar->addAction(dismissSelectedAction);

    openSelectedAction =
        new QAction(themedIcon({QStringLiteral("document-open"), QStringLiteral("internet-web-browser")}, QString(),
                               QStyle::SP_DirOpenIcon),
                    tr("Open Selected"), this);
    openSelectedAction->setShortcut(Qt::Key_Return);
    connect(openSelectedAction, &QAction::triggered, this, &MainWindow::onOpenSelectedClicked);
    toolbar->addAction(openSelectedAction);

    updateSelectionComboBox();
}

void MainWindow::setupPages() {
    createErrorPage();
    stackWidget->addWidget(errorPage);

    createLoginPage();
    stackWidget->addWidget(loginPage);

    createEmptyStatePage();
    stackWidget->addWidget(emptyStatePage);

    createLoadingPage();
    stackWidget->addWidget(loadingPage);
}

void MainWindow::setupMenus() {
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));

    QAction *settingsAction = new QAction(themedIcon({QStringLiteral("settings-configure")}), tr("&Settings"), this);
    settingsAction->setShortcut(QKeySequence::Preferences);
    connect(settingsAction, &QAction::triggered, this, &MainWindow::showSettings);
    fileMenu->addAction(settingsAction);

    QAction *notificationsSettingsAction = new QAction(themedIcon({QStringLiteral("preferences-desktop-notification")}),
                                                       tr("Configure &Notifications..."), this);
    connect(notificationsSettingsAction, &QAction::triggered, this, &MainWindow::openKdeNotificationSettings);
    fileMenu->addAction(notificationsSettingsAction);

    fileMenu->addSeparator();

    QAction *quitAction = new QAction(themedIcon({QStringLiteral("application-exit")}), tr("&Quit"), this);
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);
    fileMenu->addAction(quitAction);

    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    QAction *aboutAction = new QAction(themedIcon({QStringLiteral("help-about")}), tr("&About KGitHub Notify"), this);
    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAboutDialog);
    helpMenu->addAction(aboutAction);

    QAction *aboutQtAction = new QAction(tr("About &Qt"), this);
    connect(aboutQtAction, &QAction::triggered, qApp, &QApplication::aboutQt);
    helpMenu->addAction(aboutQtAction);
}

void MainWindow::setupStatusBar() {
    statusBar = new QStatusBar(this);
    setStatusBar(statusBar);

    countLabel = new QLabel(tr("Items: 0"), this);
    timerLabel = new QLabel(tr("Next refresh: --:--"), this);

    statusBar->addWidget(countLabel);
    statusBar->addPermanentWidget(timerLabel);

    refreshTimer = new QTimer(this);
    countdownTimer = new QTimer(this);

    connect(countdownTimer, &QTimer::timeout, this, &MainWindow::updateStatusBar);
    countdownTimer->start(1000);
}

void MainWindow::loadToken() {
    tokenWatcher = new QFutureWatcher<QString>(this);
    connect(tokenWatcher, &QFutureWatcher<QString>::finished, this, &MainWindow::onTokenLoaded);
    tokenWatcher->setFuture(SettingsDialog::getTokenAsync());
}

void MainWindow::loadSavedNotifications() {
    QSettings settings;
    QByteArray data = settings.value("savedNotifications").toByteArray();
    if (!data.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isArray()) {
            QJsonArray array = doc.array();
            for (const QJsonValue &val : array) {
                if (val.isObject()) {
                    m_savedNotifications.append(Notification::fromJson(val.toObject()));
                }
            }
        }
    }
}

void MainWindow::saveSavedNotifications() {
    QJsonArray array;
    for (const Notification &n : m_savedNotifications) {
        array.append(n.toJson());
    }
    QJsonDocument doc(array);
    QSettings settings;
    settings.setValue("savedNotifications", doc.toJson(QJsonDocument::Compact));
}

void MainWindow::saveNotification(const Notification &n) {
    if (isNotificationSaved(n.id)) return;
    m_savedNotifications.append(n);
    saveSavedNotifications();
    // If we are currently viewing "Saved", refresh the list
    if (filterComboBox && filterComboBox->currentIndex() == 1) { // 1 will be Saved
        onFilterChanged(1);
    }
}

void MainWindow::unsaveNotification(const QString &id) {
    for (int i = 0; i < m_savedNotifications.size(); ++i) {
        if (m_savedNotifications[i].id == id) {
            m_savedNotifications.removeAt(i);
            saveSavedNotifications();
            if (filterComboBox && filterComboBox->currentIndex() == 1) {
                onFilterChanged(1);
            }
            break;
        }
    }
}

bool MainWindow::isNotificationSaved(const QString &id) const {
    for (const Notification &n : m_savedNotifications) {
        if (n.id == id) return true;
    }
    return false;
}

void MainWindow::loadDoneNotifications() {
    QSettings settings;
    QByteArray data = settings.value("doneNotifications").toByteArray();
    if (!data.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isArray()) {
            QJsonArray array = doc.array();
            for (const QJsonValue &val : array) {
                if (val.isObject()) {
                    m_doneNotifications.append(Notification::fromJson(val.toObject()));
                }
            }
        }
    }
}

void MainWindow::saveDoneNotifications() {
    QJsonArray array;
    for (const Notification &n : m_doneNotifications) {
        array.append(n.toJson());
    }
    QJsonDocument doc(array);
    QSettings settings;
    settings.setValue("doneNotifications", doc.toJson(QJsonDocument::Compact));
}

void MainWindow::saveDoneNotification(const Notification &n) {
    if (isNotificationDone(n.id)) return;
    m_doneNotifications.append(n);
    while (m_doneNotifications.size() > MAX_DONE_NOTIFICATIONS) {
        m_doneNotifications.removeFirst();
    }
    saveDoneNotifications();
}

bool MainWindow::isNotificationDone(const QString &id) const {
    for (const Notification &n : m_doneNotifications) {
        if (n.id == id) return true;
    }
    return false;
}

void MainWindow::addNotificationItem(const Notification &n) {
    QListWidgetItem *item = new QListWidgetItem();
    NotificationItemWidget *widget = new NotificationItemWidget(n);

    if (detailsCache.contains(n.id)) {
        const NotificationDetails &details = detailsCache[n.id];
        if (details.hasDetails) {
            widget->setAuthor(details.author, details.avatar);
            widget->setHtmlUrl(details.htmlUrl);
        }
    } else {
        if (client) client->fetchNotificationDetails(n.url, n.id);
    }

    item->setData(Qt::UserRole, n.url);
    item->setData(Qt::UserRole + 1, n.id);
    item->setData(Qt::UserRole + 2, n.title);
    item->setData(Qt::UserRole + 3, n.repository);
    item->setData(Qt::UserRole + 4, n.toJson());
    item->setSizeHint(widget->sizeHint());

    widget->setSaved(isNotificationSaved(n.id));
    widget->setDone(isNotificationDone(n.id));
    widget->setRead(!n.unread);

    notificationList->addItem(item);
    notificationList->setItemWidget(item, widget);
}
