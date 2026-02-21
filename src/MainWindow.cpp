#include "MainWindow.h"
#include "NotificationItemWidget.h"
#include <QApplication>
#include <QScreen>
#include <QDesktopServices>
#include <QUrl>
#include <QAction>
#include <QMenuBar>
#include <QCloseEvent>
#include <QMessageBox>
#include <QDebug>
#include <QStyle>
#include <QVBoxLayout>
#include <QClipboard>
#include "SettingsDialog.h"
#include <limits>

static int calculateSafeInterval(int minutes) {
    if (minutes <= 0) minutes = 1; // Minimum 1 minute
    qint64 msec = static_cast<qint64>(minutes) * 60 * 1000;
    if (msec > std::numeric_limits<int>::max()) {
        return std::numeric_limits<int>::max();
    }
    return static_cast<int>(msec);
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), client(nullptr), pendingAuthError(false), authNotification(nullptr) {
    setWindowTitle("Kgithub-notify");
    setWindowIcon(QIcon(":/assets/icon.png"));
    resize(400, 600);

    // Initialize Stacked Widget
    stackWidget = new QStackedWidget(this);
    setCentralWidget(stackWidget);

    // Notification List
    notificationList = new QListWidget(this);
    notificationList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    connect(notificationList, &QListWidget::itemActivated, this, &MainWindow::onNotificationItemActivated);

    // Context menu for list
    notificationList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(notificationList, &QListWidget::customContextMenuRequested, this, &MainWindow::showContextMenu);

    contextMenu = new QMenu(this);

    openAction = new QAction("Open", this);
    connect(openAction, &QAction::triggered, this, &MainWindow::openCurrentItem);
    contextMenu->addAction(openAction);

    copyLinkAction = new QAction("Copy Link", this);
    connect(copyLinkAction, &QAction::triggered, this, &MainWindow::copyLinkCurrentItem);
    contextMenu->addAction(copyLinkAction);

    dismissAction = new QAction("Dismiss", this);
    connect(dismissAction, &QAction::triggered, this, &MainWindow::dismissCurrentItem);
    contextMenu->addAction(dismissAction);

    stackWidget->addWidget(notificationList);

    // Toolbar
    toolbar = new QToolBar(this);
    toolbar->setMovable(false);
    addToolBar(Qt::TopToolBarArea, toolbar);

    refreshAction = new QAction(QApplication::style()->standardIcon(QStyle::SP_BrowserReload), "Refresh", this);
    refreshAction->setShortcut(QKeySequence::Refresh);
    connect(refreshAction, &QAction::triggered, this, &MainWindow::onRefreshClicked);
    toolbar->addAction(refreshAction);

    toolbar->addSeparator();

    selectAllAction = new QAction("Select All", this);
    selectAllAction->setShortcut(QKeySequence::SelectAll);
    connect(selectAllAction, &QAction::triggered, this, &MainWindow::onSelectAllClicked);
    toolbar->addAction(selectAllAction);

    selectNoneAction = new QAction("Select None", this);
    selectNoneAction->setShortcut(QKeySequence("Ctrl+Shift+A"));
    connect(selectNoneAction, &QAction::triggered, this, &MainWindow::onSelectNoneClicked);
    toolbar->addAction(selectNoneAction);

    selectTop10Action = new QAction("Top 10", this);
    selectTop10Action->setShortcut(QKeySequence("Ctrl+1"));
    connect(selectTop10Action, &QAction::triggered, this, &MainWindow::onSelectTop10Clicked);
    toolbar->addAction(selectTop10Action);

    toolbar->addSeparator();

    dismissSelectedAction = new QAction(QApplication::style()->standardIcon(QStyle::SP_DialogDiscardButton), "Dismiss Selected", this);
    dismissSelectedAction->setShortcut(QKeySequence::Delete);
    connect(dismissSelectedAction, &QAction::triggered, this, &MainWindow::onDismissSelectedClicked);
    toolbar->addAction(dismissSelectedAction);

    openSelectedAction = new QAction(QApplication::style()->standardIcon(QStyle::SP_DirOpenIcon), "Open Selected", this);
    openSelectedAction->setShortcut(Qt::Key_Return);
    connect(openSelectedAction, &QAction::triggered, this, &MainWindow::onOpenSelectedClicked);
    toolbar->addAction(openSelectedAction);

    // Error Page
    createErrorPage();
    stackWidget->addWidget(errorPage);

    // Login Page
    createLoginPage();
    stackWidget->addWidget(loginPage);

    // Empty State Page
    createEmptyStatePage();
    stackWidget->addWidget(emptyStatePage);

    createTrayIcon();

    // Menu
    QMenu *fileMenu = menuBar()->addMenu("&File");
    QAction *settingsAction = new QAction("&Settings", this);
    connect(settingsAction, &QAction::triggered, this, &MainWindow::showSettings);
    fileMenu->addAction(settingsAction);

    QAction *quitAction = new QAction("&Quit", this);
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);
    fileMenu->addAction(quitAction);

    // Status Bar
    statusBar = new QStatusBar(this);
    setStatusBar(statusBar);

    countLabel = new QLabel("Items: 0", this);
    timerLabel = new QLabel("Next refresh: --:--", this);

    statusBar->addWidget(countLabel);
    statusBar->addPermanentWidget(timerLabel);

    refreshTimer = new QTimer(this);
    countdownTimer = new QTimer(this);

    connect(countdownTimer, &QTimer::timeout, this, &MainWindow::updateStatusBar);
    countdownTimer->start(1000);

    // Initial State Check
    QString token = SettingsDialog::getToken();
    if (token.isEmpty()) {
        stackWidget->setCurrentWidget(loginPage);
    } else {
        stackWidget->setCurrentWidget(notificationList);
    }
}

MainWindow::~MainWindow() {
}

void MainWindow::createErrorPage() {
    errorPage = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(errorPage);
    layout->setAlignment(Qt::AlignCenter);

    errorLabel = new QLabel("Authentication Error", errorPage);
    errorLabel->setWordWrap(true);
    errorLabel->setAlignment(Qt::AlignCenter);

    settingsButton = new QPushButton("Open Settings", errorPage);
    connect(settingsButton, &QPushButton::clicked, this, &MainWindow::showSettings);

    layout->addWidget(errorLabel);
    layout->addWidget(settingsButton);
}

void MainWindow::createLoginPage() {
    loginPage = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(loginPage);
    layout->setAlignment(Qt::AlignCenter);

    loginLabel = new QLabel("Welcome to Kgithub-notify!\n\nPlease configure your Personal Access Token (PAT) to get started.", loginPage);
    loginLabel->setWordWrap(true);
    loginLabel->setAlignment(Qt::AlignCenter);

    loginButton = new QPushButton("Open Settings", loginPage);
    connect(loginButton, &QPushButton::clicked, this, &MainWindow::showSettings);

    layout->addWidget(loginLabel);
    layout->addWidget(loginButton);
}

void MainWindow::createEmptyStatePage() {
    emptyStatePage = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(emptyStatePage);
    layout->setAlignment(Qt::AlignCenter);

    emptyStateLabel = new QLabel("No new notifications", emptyStatePage);
    emptyStateLabel->setAlignment(Qt::AlignCenter);

    layout->addWidget(emptyStateLabel);
}

void MainWindow::setClient(GitHubClient *c) {
    client = c;
    connect(client, &GitHubClient::notificationsReceived, this, &MainWindow::updateNotifications);
    connect(client, &GitHubClient::errorOccurred, this, &MainWindow::showError);
    connect(client, &GitHubClient::authError, this, &MainWindow::onAuthError);

    if (refreshTimer) {
        connect(refreshTimer, &QTimer::timeout, client, &GitHubClient::checkNotifications);
        int interval = SettingsDialog::getInterval();
        refreshTimer->setInterval(calculateSafeInterval(interval));
        refreshTimer->start();
    }

    QString token = SettingsDialog::getToken();
    if (!token.isEmpty()) {
        client->setToken(token);
    }
}

void MainWindow::createTrayIcon() {
    trayIconMenu = new QMenu(this);
    updateTrayMenu();

    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setContextMenu(trayIconMenu);

    QIcon icon(":/assets/icon.png");
    if (icon.isNull()) {
        icon = QApplication::style()->standardIcon(QStyle::SP_ComputerIcon);
    }
    trayIcon->setIcon(icon);

    connect(trayIcon, &QSystemTrayIcon::activated, this, &MainWindow::onTrayIconActivated);
    connect(trayIcon, &QSystemTrayIcon::messageClicked, this, &MainWindow::onTrayMessageClicked);

    trayIcon->show();
}

void MainWindow::updateTrayMenu() {
    if (!trayIconMenu) return;
    trayIconMenu->clear();

    QAction *openAppAction = new QAction("Open Kgithub-notify", trayIconMenu);
    QFont font = openAppAction->font();
    font.setBold(true);
    openAppAction->setFont(font);
    connect(openAppAction, &QAction::triggered, this, &QWidget::showNormal);
    trayIconMenu->addAction(openAppAction);

    trayIconMenu->addSeparator();

    int unreadCount = 0;
    QList<QListWidgetItem*> unreadItems;
    for(int i=0; i<notificationList->count(); ++i) {
        QListWidgetItem* item = notificationList->item(i);
        if(item->font().bold()) {
            unreadCount++;
            unreadItems.append(item);
        }
    }

    if (unreadCount > 0) {
        QAction *header = new QAction(QString("%1 Unread Notifications").arg(unreadCount), trayIconMenu);
        header->setEnabled(false);
        trayIconMenu->addAction(header);

        int limit = qMin(unreadCount, 5);
        for(int i=0; i<limit; ++i) {
            QListWidgetItem* item = unreadItems[i];
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
                for(int i=0; i<notificationList->count(); ++i) {
                     QListWidgetItem* item = notificationList->item(i);
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

        QAction *dismissAllAction = new QAction("Dismiss All", trayIconMenu);
        connect(dismissAllAction, &QAction::triggered, this, &MainWindow::dismissAllNotifications);
        trayIconMenu->addAction(dismissAllAction);
    } else {
        QAction *empty = new QAction("No new notifications", trayIconMenu);
        empty->setEnabled(false);
        trayIconMenu->addAction(empty);
    }

    trayIconMenu->addSeparator();

    QAction *trayRefreshAction = new QAction("Force Refresh", trayIconMenu);
    connect(trayRefreshAction, &QAction::triggered, [this]() {
        if (client) {
            client->checkNotifications();
        }
    });
    trayIconMenu->addAction(trayRefreshAction);

    QAction *settingsAction = new QAction("Settings", trayIconMenu);
    connect(settingsAction, &QAction::triggered, this, &MainWindow::showSettings);
    trayIconMenu->addAction(settingsAction);

    QAction *quitAction = new QAction("Quit", trayIconMenu);
    connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);
    trayIconMenu->addAction(quitAction);
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
            showNormal();
            activateWindow();
        }
    }
}

void MainWindow::updateNotifications(const QList<Notification> &notifications) {
    pendingAuthError = false;
    lastError.clear();

    if (countLabel) {
        countLabel->setText(QString("Items: %1").arg(notifications.count()));
    }

    if (authNotification && authNotification->isVisible()) {
        authNotification->close();
    }

    // Switch to list view on successful update
    if (notifications.isEmpty()) {
        if (stackWidget->currentWidget() != emptyStatePage) {
            stackWidget->setCurrentWidget(emptyStatePage);
        }
    } else {
        if (stackWidget->currentWidget() != notificationList) {
            stackWidget->setCurrentWidget(notificationList);
        }
    }

    notificationList->clear();
    int unreadCount = 0;
    int newNotifications = 0;
    QList<Notification> newlyAddedNotifications;

    for (const Notification &n : notifications) {
        QListWidgetItem *item = new QListWidgetItem();
        NotificationItemWidget *widget = new NotificationItemWidget(n);

        item->setData(Qt::UserRole, n.url);
        item->setData(Qt::UserRole + 1, n.id);
        item->setData(Qt::UserRole + 2, n.title);
        item->setData(Qt::UserRole + 3, n.repository);
        item->setSizeHint(widget->sizeHint());

        if (n.unread) {
            unreadCount++;

            if (!knownNotificationIds.contains(n.id)) {
                newNotifications++;
                knownNotificationIds.insert(n.id);
                newlyAddedNotifications.append(n);
            }
        }

        notificationList->addItem(item);
        notificationList->setItemWidget(item, widget);
    }

    if (unreadCount > 0) {
        trayIcon->setIcon(QIcon(":/assets/icon-dotted.png"));
        if (newNotifications > 0) {
            if (newNotifications == 1) {
                sendNotification(newlyAddedNotifications.first());
            } else {
                sendSummaryNotification(newNotifications, newlyAddedNotifications);
            }
        }
    } else {
        QIcon icon(":/assets/icon.png");
        if (icon.isNull()) {
            icon = QApplication::style()->standardIcon(QStyle::SP_ComputerIcon);
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
            client->checkNotifications(); // Force check immediately
        }
        if (refreshTimer) {
            refreshTimer->setInterval(calculateSafeInterval(interval));
            refreshTimer->start();
            updateStatusBar();
        }
    }
}

void MainWindow::onAuthError(const QString &message) {
    pendingAuthError = true;

    // Update error page
    errorLabel->setText("Authentication Error: " + message + "\n\nPlease update your token in Settings.");
    stackWidget->setCurrentWidget(errorPage);

    if (!authNotification) {
        authNotification = new AuthErrorNotification(this);
        connect(authNotification, &AuthErrorNotification::settingsClicked, this, &MainWindow::onAuthNotificationSettingsClicked);
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
        notificationList->setCurrentItem(item); // Ensure item is selected
        contextMenu->exec(notificationList->mapToGlobal(pos));
    }
}

void MainWindow::dismissCurrentItem() {
    QListWidgetItem *item = notificationList->currentItem();
    if (!item) return;
    QString id = item->data(Qt::UserRole + 1).toString();
    if (client) client->markAsRead(id);

    knownNotificationIds.remove(id);

    delete notificationList->takeItem(notificationList->row(item));

    // Update icon if list is empty
    if (notificationList->count() == 0) {
        QIcon icon(":/assets/icon.png");
        if (icon.isNull()) {
            icon = QApplication::style()->standardIcon(QStyle::SP_ComputerIcon);
        }
        trayIcon->setIcon(icon);
    }
    updateTrayMenu();
}

void MainWindow::showError(const QString &error) {
    if (error == lastError) return;
    lastError = error;

    // Only show error via tray if visible, otherwise standard message box (but avoid spamming boxes)
    if (trayIcon && trayIcon->isVisible()) {
        trayIcon->showMessage("GitHub Error", error, QSystemTrayIcon::Warning, 5000);
    } else {
        // Maybe don't show message box on every polling error if window is hidden?
        // But if window is visible, we should show it.
        if (isVisible()) {
             QMessageBox::warning(this, "GitHub Error", error);
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
        client->checkNotifications();
        if (refreshTimer) {
            refreshTimer->start(); // Restart timer
            updateStatusBar(); // Force update
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

void MainWindow::onSelectTop10Clicked() {
    if (!notificationList) return;
    notificationList->clearSelection();
    int count = notificationList->count();
    int limit = qMin(10, count);
    for (int i = 0; i < limit; ++i) {
        QListWidgetItem *item = notificationList->item(i);
        if (item) item->setSelected(true);
    }
}

void MainWindow::onDismissSelectedClicked() {
    if (!notificationList || !client) return;

    QList<QListWidgetItem*> items = notificationList->selectedItems();
    for (auto item : items) {
        QString id = item->data(Qt::UserRole + 1).toString();
        client->markAsRead(id);
        knownNotificationIds.remove(id);
        delete notificationList->takeItem(notificationList->row(item));
    }

    // Update icon if list is empty
    if (notificationList->count() == 0) {
        QIcon icon(":/assets/icon.png");
        if (icon.isNull()) {
            icon = QApplication::style()->standardIcon(QStyle::SP_ComputerIcon);
        }
        trayIcon->setIcon(icon);
    }
    updateTrayMenu();
}

void MainWindow::onOpenSelectedClicked() {
    if (!notificationList) return;

    QList<QListWidgetItem*> items = notificationList->selectedItems();
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
            timerLabel->setText(QString("Next refresh: %1:%2")
                                .arg(minutes, 2, 10, QChar('0'))
                                .arg(seconds, 2, 10, QChar('0')));
            return;
        }
    }
    if (timerLabel) {
        timerLabel->setText("Next refresh: --:--");
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
    notification->setTitle(n.repository);
    notification->setText(n.title);

    // Actions
    QStringList actions;
    actions << "Open";
    notification->setActions(actions);

    connect(notification, &KNotification::action1Activated, this, [this, n](){
        QString htmlUrl = GitHubClient::apiToHtmlUrl(n.url, n.id);
        QDesktopServices::openUrl(QUrl(htmlUrl));
    });

    connect(notification, &KNotification::defaultActivated, this, [this](){
        this->showNormal();
        this->activateWindow();
    });
    connect(notification, &KNotification::closed, notification, &QObject::deleteLater);

    notification->sendEvent();
}

void MainWindow::sendSummaryNotification(int count, const QList<Notification> &notifications) {
    KNotification *notification = new KNotification("NewNotification");
    notification->setTitle(QString("%1 New Notifications").arg(count));

    QString summary;
    int limit = qMin(count, 5);
    for(int i=0; i<limit; ++i) {
        summary += "- " + notifications[i].title + "\n";
    }
    if (count > limit) {
        summary += QString("... and %1 more").arg(count - limit);
    }
    notification->setText(summary.trimmed());

    // Actions
    QStringList actions;
    actions << "Open Client";
    notification->setActions(actions);

    connect(notification, &KNotification::action1Activated, this, [this](){
        this->showNormal();
        this->activateWindow();
    });

    connect(notification, &KNotification::defaultActivated, this, [this](){
        this->showNormal();
        this->activateWindow();
    });
    connect(notification, &KNotification::closed, notification, &QObject::deleteLater);

    notification->sendEvent();
}
