#include "MainWindow.h"
#include <QApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QAction>
#include <QMenuBar>
#include <QCloseEvent>
#include <QMessageBox>
#include <QDebug>
#include <QStyle>
#include "SettingsDialog.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), client(nullptr), pendingAuthError(false) {
    setWindowTitle("Kgithub-notify");
    resize(400, 600);

    notificationList = new QListWidget(this);
    setCentralWidget(notificationList);
    connect(notificationList, &QListWidget::itemActivated, this, &MainWindow::onNotificationItemActivated);

    // Context menu
    notificationList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(notificationList, &QListWidget::customContextMenuRequested, this, &MainWindow::showContextMenu);

    contextMenu = new QMenu(this);
    dismissAction = new QAction("Dismiss", this);
    connect(dismissAction, &QAction::triggered, this, &MainWindow::dismissCurrentItem);
    contextMenu->addAction(dismissAction);

    createTrayIcon();

    // Menu
    QMenu *fileMenu = menuBar()->addMenu("&File");
    QAction *settingsAction = new QAction("&Settings", this);
    connect(settingsAction, &QAction::triggered, this, &MainWindow::showSettings);
    fileMenu->addAction(settingsAction);

    QAction *quitAction = new QAction("&Quit", this);
    connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);
    fileMenu->addAction(quitAction);

    // Check if token exists, if not show settings
    QString token = SettingsDialog::getToken();
    if (token.isEmpty()) {
        // We can't show modal dialog in constructor usually, but let's try or defer it.
        // Better to defer it or let the main loop handle it.
    }
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
        if (client) {
            client->setToken(dialog.getToken());
            client->checkNotifications();
        }
    }
}

void MainWindow::onAuthError(const QString &message) {
    pendingAuthError = true;
    if (trayIcon && trayIcon->isVisible()) {
        trayIcon->showMessage("GitHub Error", message + "\nClick to open settings.", QSystemTrayIcon::Warning, 10000);
    } else {
        // If tray not visible, should we force settings?
        // Maybe better to just show settings if window is visible, or open it.
        // For now rely on tray message or if window is open, show dialog.
        if (isVisible()) {
            showSettings();
        }
    }
}

void MainWindow::onTrayMessageClicked() {
    if (pendingAuthError) {
        showSettings();
        pendingAuthError = false;
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
