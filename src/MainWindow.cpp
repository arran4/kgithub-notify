#include "MainWindow.h"
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
#include "SettingsDialog.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), client(nullptr), pendingAuthError(false), authNotification(nullptr) {
    setWindowTitle("Kgithub-notify");
    resize(400, 600);

    // Initialize Stacked Widget
    stackWidget = new QStackedWidget(this);
    setCentralWidget(stackWidget);

    // Notification List
    notificationList = new QListWidget(this);
    connect(notificationList, &QListWidget::itemActivated, this, &MainWindow::onNotificationItemActivated);

    // Context menu for list
    notificationList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(notificationList, &QListWidget::customContextMenuRequested, this, &MainWindow::showContextMenu);

    contextMenu = new QMenu(this);
    dismissAction = new QAction("Dismiss", this);
    connect(dismissAction, &QAction::triggered, this, &MainWindow::dismissCurrentItem);
    contextMenu->addAction(dismissAction);

    stackWidget->addWidget(notificationList);

    // Error Page
    createErrorPage();
    stackWidget->addWidget(errorPage);

    // Login Page
    createLoginPage();
    stackWidget->addWidget(loginPage);

    createTrayIcon();

    // Menu
    QMenu *fileMenu = menuBar()->addMenu("&File");
    QAction *settingsAction = new QAction("&Settings", this);
    connect(settingsAction, &QAction::triggered, this, &MainWindow::showSettings);
    fileMenu->addAction(settingsAction);

    QAction *quitAction = new QAction("&Quit", this);
    connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);
    fileMenu->addAction(quitAction);

    // Initial State Check
    QString token = SettingsDialog::getToken();
    if (token.isEmpty()) {
        stackWidget->setCurrentWidget(loginPage);
    } else {
        stackWidget->setCurrentWidget(notificationList);
    }
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

void MainWindow::setClient(GitHubClient *c) {
    client = c;
    connect(client, &GitHubClient::notificationsReceived, this, &MainWindow::updateNotifications);
    connect(client, &GitHubClient::errorOccurred, this, &MainWindow::showError);
    connect(client, &GitHubClient::authError, this, &MainWindow::onAuthError);

    QString token = SettingsDialog::getToken();
    if (!token.isEmpty()) {
        client->setToken(token);
    }
}

void MainWindow::createTrayIcon() {
    trayIconMenu = new QMenu(this);

    QAction *showAction = new QAction("Show", this);
    connect(showAction, &QAction::triggered, this, &QWidget::showNormal);
    trayIconMenu->addAction(showAction);

    QAction *settingsAction = new QAction("Settings", this);
    connect(settingsAction, &QAction::triggered, this, &MainWindow::showSettings);
    trayIconMenu->addAction(settingsAction);

    QAction *quitAction = new QAction("Quit", this);
    connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);
    trayIconMenu->addAction(quitAction);

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

    if (authNotification && authNotification->isVisible()) {
        authNotification->close();
    }

    // Switch to list view on successful update
    if (stackWidget->currentWidget() != notificationList) {
        stackWidget->setCurrentWidget(notificationList);
    }

    notificationList->clear();
    int unreadCount = 0;
    int newNotifications = 0;

    for (const Notification &n : notifications) {
        QString label = QString("[%1] %2 (%3)").arg(n.type, n.title, n.repository);
        QListWidgetItem *item = new QListWidgetItem(label);

        item->setData(Qt::UserRole, n.url);
        item->setData(Qt::UserRole + 1, n.id);

        if (n.unread) {
            QFont font = item->font();
            font.setBold(true);
            item->setFont(font);
            unreadCount++;

            if (!knownNotificationIds.contains(n.id)) {
                newNotifications++;
                knownNotificationIds.insert(n.id);
            }
        }

        notificationList->addItem(item);
    }

    if (unreadCount > 0) {
        trayIcon->setIcon(QApplication::style()->standardIcon(QStyle::SP_MessageBoxInformation));
        if (newNotifications > 0) {
            showTrayMessage("GitHub Notifications", QString("You have %1 new notification(s)").arg(newNotifications));
        }
    } else {
        QIcon icon(":/assets/icon.png");
        if (icon.isNull()) {
            icon = QApplication::style()->standardIcon(QStyle::SP_ComputerIcon);
        }
        trayIcon->setIcon(icon);
    }
}

void MainWindow::onAuthNotificationSettingsClicked() {
    if (authNotification) {
        authNotification->close();
    }
    showSettings();
}

void MainWindow::onNotificationItemActivated(QListWidgetItem *item) {
    QString apiUrl = item->data(Qt::UserRole).toString();

    QString htmlUrl = apiUrl;
    htmlUrl.replace("api.github.com/repos", "github.com");
    htmlUrl.replace("/pulls/", "/pull/");

    QDesktopServices::openUrl(QUrl(htmlUrl));
}

void MainWindow::showTrayMessage(const QString &title, const QString &message) {
    trayIcon->showMessage(title, message, QSystemTrayIcon::Information, 3000);
}

void MainWindow::showSettings() {
    SettingsDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        QString newToken = dialog.getToken();
        if (client) {
            client->setToken(newToken);
            client->checkNotifications(); // Force check immediately

            // If we were on login page and now have a token, we might want to switch
            // to notification list immediately or wait for callback.
            // But waiting for callback is safer as token might be invalid.
            // However, visually it's nice to switch away from "Please login".
            // Let's rely on updateNotifications or onAuthError to switch.
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

    // Position near tray icon if possible
    QRect screenGeom = QGuiApplication::primaryScreen()->availableGeometry();
    bool positioned = false;

    if (trayIcon && trayIcon->isVisible()) {
        QRect trayGeom = trayIcon->geometry();
        if (!trayGeom.isEmpty()) {
             int x = trayGeom.center().x() - authNotification->width() / 2;
             int y;

             // If tray is in top half, show below
             if (trayGeom.center().y() < screenGeom.height() / 2) {
                 y = trayGeom.bottom() + 10;
             } else {
                 // Show above
                 y = trayGeom.top() - authNotification->height() - 10;
             }

             // Clamp X
             if (x < screenGeom.left()) x = screenGeom.left() + 10;
             if (x + authNotification->width() > screenGeom.right()) x = screenGeom.right() - authNotification->width() - 10;

             authNotification->move(x, y);
             positioned = true;
        }
    }

    if (!positioned) {
        // Fallback: Bottom right
        authNotification->move(screenGeom.bottomRight() - QPoint(authNotification->width() + 10, authNotification->height() + 10));
    }

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
