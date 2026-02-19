from PyQt5.QtWidgets import (QMainWindow, QWidget, QVBoxLayout, QListWidget,
                             QListWidgetItem, QPushButton, QHBoxLayout, QLabel,
                             QAction, QToolBar, QMessageBox)
from PyQt5.QtGui import QIcon, QColor
from PyQt5.QtCore import Qt, QUrl
from PyQt5.QtGui import QDesktopServices
import webbrowser

class NotificationWindow(QMainWindow):
    def __init__(self, github_client, parent=None):
        super().__init__(parent)
        self.client = github_client
        self.setWindowTitle("GitHub Notifications")
        self.resize(800, 600)

        self.central_widget = QWidget()
        self.setCentralWidget(self.central_widget)
        self.layout = QVBoxLayout(self.central_widget)

        self.list_widget = QListWidget()
        self.list_widget.itemDoubleClicked.connect(self.open_notification)
        self.layout.addWidget(self.list_widget)

        btn_layout = QHBoxLayout()
        self.refresh_btn = QPushButton("Refresh")
        self.refresh_btn.clicked.connect(self.refresh_notifications)

        self.dismiss_btn = QPushButton("Dismiss Selected")
        self.dismiss_btn.clicked.connect(self.dismiss_selected)

        btn_layout.addWidget(self.refresh_btn)
        btn_layout.addWidget(self.dismiss_btn)
        self.layout.addLayout(btn_layout)

        self.notifications = []

    def refresh_notifications(self):
        self.list_widget.clear()
        try:
            self.notifications = self.client.get_notifications()
        except ValueError:
            # Token might not be set
            self.notifications = []

        for note in self.notifications:
            item = QListWidgetItem()
            title = note['subject']['title']
            repo = note['repository']['full_name']
            type_ = note['subject']['type']

            text = f"[{repo}] {title} ({type_})"
            item.setText(text)
            item.setData(Qt.UserRole, note)

            if note['unread']:
                item.setForeground(QColor("blue"))

            self.list_widget.addItem(item)

    def open_notification(self, item):
        note = item.data(Qt.UserRole)
        # Construct URL
        subject_url = note['subject'].get('url')
        if subject_url:
            # Simple heuristic to convert API URL to HTML URL
            html_url = subject_url.replace("api.github.com/repos", "github.com")
            # Handle PRs and Issues
            if "/pulls/" in html_url:
                html_url = html_url.replace("/pulls/", "/pull/")

            webbrowser.open(html_url)

        # Mark as read
        self.client.mark_thread_as_read(note['id'])
        item.setForeground(QColor("black")) # Mark as read visually

    def dismiss_selected(self):
        items = self.list_widget.selectedItems()
        for item in items:
            note = item.data(Qt.UserRole)
            if self.client.mark_thread_as_read(note['id']):
                self.list_widget.takeItem(self.list_widget.row(item))
