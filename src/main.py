import sys
import os
from PyQt5.QtWidgets import QApplication, QSystemTrayIcon, QMenu, QAction
from PyQt5.QtGui import QIcon, QPixmap, QColor, QPainter
from PyQt5.QtCore import QTimer
import keyring
import json

from github_client import GitHubClient
from settings_dialog import SettingsDialog, APP_NAME, KEYRING_SERVICE, CONFIG_FILE
from notification_window import NotificationWindow
from notification_worker import NotificationWorker
import logging

def load_token():
    try:
        token = keyring.get_password(APP_NAME, KEYRING_SERVICE)
        if token:
            return token
    except Exception:
        pass

    if os.path.exists(CONFIG_FILE):
        try:
            with open(CONFIG_FILE, 'r') as f:
                config = json.load(f)
                return config.get('token')
        except Exception:
            pass
    return None

def create_tray_icon():
    # Create a simple colored circle icon programmatically
    pixmap = QPixmap(64, 64)
    pixmap.fill(QColor("transparent"))
    painter = QPainter(pixmap)
    painter.setBrush(QColor("black"))
    painter.drawEllipse(0, 0, 64, 64)
    painter.end()
    return QIcon(pixmap)

class SystemTrayApp:
    def __init__(self):
        self.app = QApplication(sys.argv)
        self.app.setQuitOnLastWindowClosed(False)

        self.client = GitHubClient()
        self.token = load_token()
        if self.token:
            self.client.set_token(self.token)

        self.tray_icon = QSystemTrayIcon(create_tray_icon(), self.app)
        self.tray_icon.setToolTip("GitHub Notifications")

        self.menu = QMenu()

        self.show_window_action = QAction("Show Notifications")
        self.show_window_action.triggered.connect(self.show_window)
        self.menu.addAction(self.show_window_action)

        self.settings_action = QAction("Settings")
        self.settings_action.triggered.connect(self.show_settings)
        self.menu.addAction(self.settings_action)

        self.quit_action = QAction("Quit")
        self.quit_action.triggered.connect(self.app.quit)
        self.menu.addAction(self.quit_action)

        self.tray_icon.setContextMenu(self.menu)
        self.tray_icon.show()

        self.notification_window = NotificationWindow(self.client)
        self.settings_dialog = SettingsDialog()
        self.settings_dialog.accepted.connect(self.on_settings_saved)

        # Poll timer (every 60 seconds)
        self.timer = QTimer()
        self.timer.timeout.connect(self.check_notifications)
        self.timer.start(60000)

        # Initial check
        if self.token:
            self.check_notifications()
        else:
            self.tray_icon.showMessage("GitHub Notify", "Please configure your token in Settings.")
            self.show_settings()

    def show_window(self):
        self.notification_window.refresh_notifications()
        self.notification_window.show()
        self.notification_window.raise_()
        self.notification_window.activateWindow()

    def show_settings(self):
        self.settings_dialog.show()

    def on_settings_saved(self):
        self.token = load_token() # Reload token
        if self.token:
            self.client.set_token(self.token)
            self.check_notifications()

    def check_notifications(self):
        if not self.token:
            return

        self.worker = NotificationWorker(self.client)
        self.worker.notifications_ready.connect(self.on_notifications_ready)
        self.worker.start()

    def on_notifications_ready(self, notifications):
        unread_count = len([n for n in notifications if n['unread']])

        if unread_count > 0:
            self.tray_icon.showMessage(
                "GitHub Notifications",
                f"You have {unread_count} unread notifications.",
                QSystemTrayIcon.Information,
                5000
            )

        # Update window if open or simply push data
        self.notification_window.update_notifications(notifications)

    def run(self):
        sys.exit(self.app.exec_())

if __name__ == "__main__":
    app = SystemTrayApp()
    app.run()
