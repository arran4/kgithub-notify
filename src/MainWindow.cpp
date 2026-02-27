#include "MainWindow.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
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
#include <QPointer>
#include <QProcess>
#include <QScreen>
#include <QSettings>
#include <QSpinBox>
#include <QLineEdit>
#include <QStyle>
#include <QUrl>
#include <QVBoxLayout>
#include <limits>

#include "NotificationItemWidget.h"
#include "SettingsDialog.h"
#include "NotificationListWidget.h"
#include "DebugWindow.h"

// -----------------------------------------------------------------------------
// Constants / Static Helpers
// -----------------------------------------------------------------------------

static int calculateSafeInterval(int minutes) {
    if (minutes <= 0) minutes = 1;  // Minimum 1 minute
    qint64 msec = static_cast<qint64>(minutes) * 60 * 1000;
    if (msec > std::numeric_limits<int>::max()) {
        return std::numeric_limits<int>::max();
    }
    return static_cast<int>(msec);
}

// -----------------------------------------------------------------------------
// Constructor / Destructor
// -----------------------------------------------------------------------------

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      debugWindow(nullptr),
      trayIcon(nullptr),
      trayIconMenu(nullptr),
      notificationListWidget(nullptr),
      client(nullptr),
      pendingAuthError(false),
      authNotification(nullptr),
      m_lastUnreadCount(0) {
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

    loadToken();
}

MainWindow::~MainWindow() {
    QSettings settings;
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());
}

// -----------------------------------------------------------------------------
// Public Methods
// -----------------------------------------------------------------------------

void MainWindow::setClient(GitHubClient *c) {
    client = c;
    connect(client, &GitHubClient::loadingStarted, this, &MainWindow::onLoadingStarted);
    connect(client, &GitHubClient::notificationsReceived, this, &MainWindow::updateNotifications);
    connect(client, &GitHubClient::errorOccurred, this, &MainWindow::showError);
    connect(client, &GitHubClient::authError, this, &MainWindow::onAuthError);

    connect(client, &GitHubClient::detailsError, notificationListWidget, &NotificationListWidget::updateError);
    connect(client, &GitHubClient::detailsReceived, notificationListWidget, &NotificationListWidget::updateDetails);
    connect(client, &GitHubClient::imageReceived, notificationListWidget, &NotificationListWidget::updateImage);

    // Wire up ListWidget requests
    connect(notificationListWidget, &NotificationListWidget::requestDetails, client, &GitHubClient::fetchNotificationDetails);
    connect(notificationListWidget, &NotificationListWidget::requestImage, client, &GitHubClient::fetchImage);
    connect(notificationListWidget, &NotificationListWidget::markAsRead, client, &GitHubClient::markAsRead);
    connect(notificationListWidget, &NotificationListWidget::markAsDone, client, &GitHubClient::markAsDone);
    connect(notificationListWidget, &NotificationListWidget::loadMoreRequested, client, &GitHubClient::loadMore);

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

void MainWindow::showTrayMessage(const QString &title, const QString &message) {
    trayIcon->showMessage(title, message, QSystemTrayIcon::Information, 3000);
}

// -----------------------------------------------------------------------------
// Slots
// -----------------------------------------------------------------------------

void MainWindow::updateNotifications(const QList<Notification> &notifications, bool append, bool hasMore) {
    m_lastCheckTime = QDateTime::currentDateTime();
    pendingAuthError = false;
    lastError.clear();

    if (authNotification && authNotification->isVisible()) {
        authNotification->close();
    }

    notificationListWidget->setNotifications(notifications, append, hasMore);

    updateSelectionComboBox();

    if (statusLabel) {
        statusLabel->setText(tr("Updated"));
    }
}

void MainWindow::onListCountsChanged(int total, int unread, int newCount, const QList<Notification>& newItems) {
    m_lastUnreadCount = unread;
    updateTrayIconState(unread, newCount, newItems);

    if (total == 0) {
        if(stackWidget->currentWidget() != emptyStatePage) {
           stackWidget->setCurrentWidget(emptyStatePage);
        }
    } else {
        if(stackWidget->currentWidget() != notificationListWidget) {
            stackWidget->setCurrentWidget(notificationListWidget);
        }
    }

    if (countLabel) {
        countLabel->setText(tr("Items: %1").arg(total));
    }

    // Update repo filter
    QString currentRepo = repoFilterComboBox->currentText();
    bool wasBlocked = repoFilterComboBox->blockSignals(true);
    repoFilterComboBox->clear();
    repoFilterComboBox->addItem(tr("All Repositories"));
    repoFilterComboBox->addItems(notificationListWidget->getAvailableRepos());

    int index = repoFilterComboBox->findText(currentRepo);
    if (index >= 0) {
        repoFilterComboBox->setCurrentIndex(index);
    } else {
        repoFilterComboBox->setCurrentIndex(0);
    }
    repoFilterComboBox->blockSignals(wasBlocked);
}

void MainWindow::onListStatusMessage(const QString &message) {
    if (countLabel) {
        countLabel->setText(message);
    }
}

void MainWindow::showError(const QString &error) {
    if (error == lastError) return;
    lastError = error;

    if (trayIcon && trayIcon->isVisible()) {
        trayIcon->showMessage(tr("GitHub Error"), error, QSystemTrayIcon::Warning, 5000);
    } else {
        if (isVisible()) {
            QMessageBox::warning(this, tr("GitHub Error"), error);
        }
    }

    if (stackWidget->currentWidget() == loadingPage) {
        if (notificationListWidget->count() > 0) {
            stackWidget->setCurrentWidget(notificationListWidget);
        } else {
            if (loadingLabel) {
                loadingLabel->setText(tr("Error: %1").arg(error));
            }
        }
    }

    if (statusLabel) {
        statusLabel->setText(tr("Error"));
    }

    updateTrayToolTip();
}

void MainWindow::onAuthError(const QString &message) {
    pendingAuthError = true;

    errorLabel->setText(tr("Authentication Error: %1\n\nPlease update your token in Settings.").arg(message));
    stackWidget->setCurrentWidget(errorPage);

    if (!authNotification) {
        authNotification = new AuthErrorNotification(this);
        connect(authNotification, &AuthErrorNotification::settingsClicked, this,
                &MainWindow::onAuthNotificationSettingsClicked);
    }

    authNotification->setMessage(message);
    positionPopup(authNotification);
    authNotification->show();

    if (!trayIcon || !trayIcon->isVisible()) {
        if (isVisible()) {
            showSettings();
        }
    }
}

void MainWindow::onTrayIconActivated(QSystemTrayIcon::ActivationReason reason) {
    if (reason == QSystemTrayIcon::Trigger) {
        if (isVisible()) {
            hide();
            return;
        }
        ensureWindowActive();
        return;
    }

    if (reason == QSystemTrayIcon::Context && trayIcon->contextMenu()) {
        trayIcon->contextMenu()->exec(QCursor::pos());
    }
}

void MainWindow::onTrayMessageClicked() {
    if (pendingAuthError) {
        showSettings();
    }
}

void MainWindow::showSettings() {
    SettingsDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        QString newToken = dialog.getToken();
        int interval = SettingsDialog::getInterval();
        if (client) {
            client->setToken(newToken);
            client->checkNotifications();
        }
        if (refreshTimer) {
            refreshTimer->setInterval(calculateSafeInterval(interval));
            refreshTimer->start();
            updateStatusBar();
        }
    }
}

void MainWindow::onLoadingStarted() {
    if (!notificationListWidget) return;

    if (statusLabel) {
        statusLabel->setText(tr("Checking..."));
    }

    if (notificationListWidget->count() == 0) {
        if (stackWidget->currentWidget() != loadingPage) {
            stackWidget->setCurrentWidget(loadingPage);
        }
        if (loadingLabel) {
            loadingLabel->setText(tr("Loading..."));
        }
    }
    updateTrayToolTip();
}

void MainWindow::onAuthNotificationSettingsClicked() {
    if (authNotification) {
        authNotification->close();
    }
    showSettings();
}

void MainWindow::dismissAllNotifications() {
    notificationListWidget->selectAll();
    notificationListWidget->dismissSelected();
}

void MainWindow::onTokenLoaded() {
    m_loadedToken = tokenWatcher->result();

    if (m_loadedToken.isEmpty()) {
        stackWidget->setCurrentWidget(loginPage);
    } else {
        stackWidget->setCurrentWidget(notificationListWidget);
        if (client) {
            client->setToken(m_loadedToken);
            client->checkNotifications();
        }
    }
}

void MainWindow::onRefreshClicked() {
    if (!client) return;

    client->checkNotifications();

    if (refreshTimer) {
        refreshTimer->start();
        updateStatusBar();
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

void MainWindow::onSelectAllClicked() {
    if (notificationListWidget) {
        notificationListWidget->selectAll();
    }
}

void MainWindow::onSelectNoneClicked() {
    if (notificationListWidget) {
        notificationListWidget->selectNone();
    }
}

void MainWindow::onSelectionChanged(int index) {
    if (index <= 0) return;

    int n = selectionComboBox->currentData().toInt();
    if (n <= 0) return;

    if (notificationListWidget) {
        notificationListWidget->selectTop(n);
    }

    bool wasBlocked = selectionComboBox->blockSignals(true);
    selectionComboBox->setCurrentIndex(0);
    selectionComboBox->blockSignals(wasBlocked);
}

void MainWindow::onDismissSelectedClicked() {
    if (notificationListWidget) {
        notificationListWidget->dismissSelected();
    }
}

void MainWindow::onOpenSelectedClicked() {
    if (notificationListWidget) {
        notificationListWidget->openSelected();
    }
}

void MainWindow::onFilterChanged(int index) {
    if(notificationListWidget) {
        notificationListWidget->setFilterMode(index);
    }

    if (refreshTimer) {
        refreshTimer->start();
        updateStatusBar();
    }
}

void MainWindow::showAboutDialog() {
    const QString copyright = tr("© %1 Kgithub-notify contributors").arg(QDate::currentDate().year());
    const QString description = tr("A KDE-friendly system tray client for GitHub notifications.");

    QMessageBox aboutBox(this);
    aboutBox.setWindowTitle(tr("About KGitHub Notify"));
    aboutBox.setIconPixmap(themedIcon({QStringLiteral("kgithub-notify")}, QStringLiteral(":/assets/icon.png"),
                                      QStyle::SP_ComputerIcon)
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

void MainWindow::showDebugWindow() {
    if (!debugWindow) {
        debugWindow = new DebugWindow(client, this);
    }
    debugWindow->show();
    debugWindow->raise();
    debugWindow->activateWindow();
}

void MainWindow::openKdeNotificationSettings() {
    bool launched = QProcess::startDetached(QStringLiteral("systemsettings"), {QStringLiteral("kcm_notifications")});
    if (!launched) launched = QProcess::startDetached(QStringLiteral("kcmshell6"), {QStringLiteral("kcm_notifications")});
    if (!launched) launched = QProcess::startDetached(QStringLiteral("systemsettings5"), {QStringLiteral("kcm_notifications")});
    if (!launched) launched = QProcess::startDetached(QStringLiteral("kcmshell5"), {QStringLiteral("kcm_notifications")});
    if (!launched) launched = QProcess::startDetached(QStringLiteral("kcmshell"), {QStringLiteral("kcm_notifications")});

    if (!launched) {
        showTrayMessage(tr("Notification settings unavailable"),
                        tr("Could not launch KDE notification settings on this system."));
    }
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (trayIcon->isVisible()) {
        hide();
        event->ignore();
    }
}

void MainWindow::createTrayIcon() {
    trayIconMenu = new QMenu(this);

    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setContextMenu(trayIconMenu);

    updateTrayMenu();

    QIcon icon = QIcon::fromTheme("kgithub-notify", QIcon(":/assets/icon.png"));
    trayIcon->setIcon(icon);

    connect(trayIcon, &QSystemTrayIcon::activated, this, &MainWindow::onTrayIconActivated);
    connect(trayIcon, &QSystemTrayIcon::messageClicked, this, &MainWindow::onTrayMessageClicked);

    trayIcon->show();
}

void MainWindow::updateTrayMenu() {
    if (!trayIconMenu) return;
    trayIconMenu->clear();

    QAction *openAppAction =
        new QAction(themedIcon({QStringLiteral("kgithub-notify")}), tr("Open Kgithub-notify"), trayIconMenu);
    QFont font = openAppAction->font();
    font.setBold(true);
    openAppAction->setFont(font);
    connect(openAppAction, &QAction::triggered, this, &QWidget::showNormal);
    trayIconMenu->addAction(openAppAction);

    trayIconMenu->addSeparator();

    int unreadCount = m_lastUnreadCount;
    QList<Notification> unreadNotifications = notificationListWidget ? notificationListWidget->getUnreadNotifications() : QList<Notification>();

    if (unreadCount > 0) {
        QAction *header = new QAction(tr("%1 Unread Notifications").arg(unreadCount), trayIconMenu);
        header->setEnabled(false);
        trayIconMenu->addAction(header);

        int limit = qMin(unreadNotifications.size(), 5);
        for (int i = 0; i < limit; ++i) {
            const Notification& n = unreadNotifications[i];
            QString label = QString("%1: %2").arg(n.repository, n.title);

            QAction *itemAction = new QAction(label, trayIconMenu);
            QString id = n.id;
            QString url = n.url;

            connect(itemAction, &QAction::triggered, [this, url, id]() {
                if (client) client->markAsRead(id);
                QString htmlUrl = GitHubClient::apiToHtmlUrl(url, id);
                QDesktopServices::openUrl(QUrl(htmlUrl));

                if(notificationListWidget) notificationListWidget->focusNotification(id);
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

    parts << tr("Unread: %1").arg(m_lastUnreadCount);

    QList<Notification> unreadNotifications = notificationListWidget ? notificationListWidget->getUnreadNotifications() : QList<Notification>();

    if(!unreadNotifications.isEmpty()) {
        int limit = qMin(unreadNotifications.size(), 5);
        for(int i=0; i<limit; ++i) {
             const Notification& n = unreadNotifications[i];
             parts << QStringLiteral("- %1: %2").arg(n.repository, n.title);
        }
        if(m_lastUnreadCount > 5) {
            parts << tr("... and %1 more").arg(m_lastUnreadCount - 5);
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

void MainWindow::positionPopup(QWidget *popup) {
    if (!popup) return;

    QRect screenGeom = QGuiApplication::primaryScreen()->availableGeometry();

    if (!trayIcon || !trayIcon->isVisible() || trayIcon->geometry().isEmpty()) {
        popup->move(screenGeom.bottomRight() - QPoint(popup->width() + 10, popup->height() + 10));
        return;
    }

    QRect trayGeom = trayIcon->geometry();
    int x = trayGeom.center().x() - popup->width() / 2;
    int y;

    if (trayGeom.center().y() < screenGeom.height() / 2) {
        y = trayGeom.bottom() + 10;
    } else {
        y = trayGeom.top() - popup->height() - 10;
    }

    if (x < screenGeom.left()) x = screenGeom.left() + 10;
    if (x + popup->width() > screenGeom.right()) x = screenGeom.right() - popup->width() - 10;

    popup->move(x, y);
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
        tr("Welcome to Kgithub-notify!\n\nPlease configure your Personal Access Token (PAT) to get started."), loginPage);
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

void MainWindow::ensureWindowActive() {
    showNormal();
    raise();

    if (QGuiApplication::platformName().startsWith(QLatin1String("wayland"), Qt::CaseInsensitive)) {
        QApplication::alert(this, 0);
    } else {
        activateWindow();
    }
}

void MainWindow::setupWindow() {
    setWindowTitle(tr("Kgithub-notify"));
    setWindowIcon(QIcon(":/assets/icon.png"));
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
    notificationListWidget = new NotificationListWidget(this);
    connect(notificationListWidget, &NotificationListWidget::countsChanged, this, &MainWindow::onListCountsChanged);
    connect(notificationListWidget, &NotificationListWidget::statusMessage, this, &MainWindow::onListStatusMessage);
    connect(notificationListWidget, &NotificationListWidget::linkActivated, this, [this](const QUrl &url){
        // Do nothing specific, link already opened. Maybe update status?
    });

    stackWidget->addWidget(notificationListWidget);
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
    filterComboBox->addItem(tr("Unread"));
    filterComboBox->addItem(tr("Read"));
    connect(filterComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onFilterChanged);
    toolbar->addWidget(filterComboBox);

    toolbar->addSeparator();

    repoFilterComboBox = new QComboBox(this);
    repoFilterComboBox->addItem(tr("All Repositories"));
    repoFilterComboBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    connect(repoFilterComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index){
        if(notificationListWidget) {
            notificationListWidget->setRepoFilter(repoFilterComboBox->itemText(index));
        }
    });
    toolbar->addWidget(repoFilterComboBox);

    searchLineEdit = new QLineEdit(this);
    searchLineEdit->setPlaceholderText(tr("Search..."));
    searchLineEdit->setFixedWidth(200);
    connect(searchLineEdit, &QLineEdit::textChanged, this, [this](const QString &text){
        if(notificationListWidget) {
            notificationListWidget->setSearchFilter(text);
        }
    });
    toolbar->addWidget(searchLineEdit);

    QComboBox *sortComboBox = new QComboBox(this);
    sortComboBox->addItem(tr("Updated (Newest)"));
    sortComboBox->addItem(tr("Updated (Oldest)"));
    sortComboBox->addItem(tr("Repository (A-Z)"));
    sortComboBox->addItem(tr("Repository (Z-A)"));
    sortComboBox->addItem(tr("Title (A-Z)"));
    sortComboBox->addItem(tr("Title (Z-A)"));
    sortComboBox->addItem(tr("Type (A-Z)"));
    sortComboBox->addItem(tr("Type (Z-A)"));
    sortComboBox->addItem(tr("Last Read (Newest)"));
    sortComboBox->addItem(tr("Last Read (Oldest)"));
    sortComboBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    connect(sortComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index){
        if(notificationListWidget) {
            notificationListWidget->setSortMode(index);
        }
    });
    toolbar->addWidget(sortComboBox);

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

    QMenu *toolsMenu = menuBar()->addMenu(tr("&Tools"));
    QAction *debugAction = new QAction(tr("Debug GitHub API"), this);
    connect(debugAction, &QAction::triggered, this, &MainWindow::showDebugWindow);
    toolsMenu->addAction(debugAction);
}

void MainWindow::setupStatusBar() {
    statusBar = new QStatusBar(this);
    setStatusBar(statusBar);

    countLabel = new QLabel(tr("Items: 0"), this);
    timerLabel = new QLabel(tr("Next refresh: --:--"), this);

    statusBar->addWidget(countLabel);

    statusLabel = new QLabel(this);
    statusBar->addWidget(statusLabel);

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


void MainWindow::sendNotification(const Notification &n) {
    KNotification *notification = new KNotification("NewNotification");
    notification->setComponentName(QStringLiteral("kgithub-notify"));
    notification->setTitle(n.repository);
    notification->setText(n.title);

    // Actions
    QStringList actions;
    actions << tr("Open in GitHub") << tr("Open kgithub-notify");
    notification->setActions(actions);

    connect(notification, &KNotification::action1Activated, this, [this, n]() {
        QString htmlUrl = GitHubClient::apiToHtmlUrl(n.url, n.id);
        QDesktopServices::openUrl(QUrl(htmlUrl));
    });

    connect(notification, &KNotification::action2Activated, this, [this, n]() {
        if(notificationListWidget) notificationListWidget->focusNotification(n.id);
        ensureWindowActive();
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
    actions << tr("Open kgithub-notify");
    notification->setActions(actions);

    connect(notification, &KNotification::action1Activated, this, [this]() { this->ensureWindowActive(); });

    connect(notification, &KNotification::defaultActivated, this, [this]() {
        this->showNormal();
        this->activateWindow();
    });
    connect(notification, &KNotification::closed, notification, &QObject::deleteLater);

    notification->sendEvent();
}


void MainWindow::updateTrayIconState(int unreadCount, int newNotifications,
                                     const QList<Notification> &newlyAddedNotifications) {

    if (unreadCount <= 0) {
        trayIcon->setIcon(themedIcon({QStringLiteral("kgithub-notify")}, QStringLiteral(":/assets/icon.png"),
                                     QStyle::SP_ComputerIcon));
        updateTrayMenu();
        return;
    }

    trayIcon->setIcon(QIcon(":/assets/icon-dotted.png"));
    if (newNotifications > 0) {
        if (newNotifications > 10) {
            sendSummaryNotification(newNotifications, newlyAddedNotifications);
        } else {
            for (const Notification &n : newlyAddedNotifications) {
                sendNotification(n);
            }
        }
    }
    updateTrayMenu();
}


void MainWindow::updateSelectionComboBox() {
    if (!selectionComboBox || !notificationListWidget) return;

    int count = notificationListWidget->count();
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
