from PyQt5.QtCore import QThread, pyqtSignal
import logging

class NotificationWorker(QThread):
    notifications_ready = pyqtSignal(list)
    error_occurred = pyqtSignal(str)

    def __init__(self, client):
        super().__init__()
        self.client = client

    def run(self):
        try:
            notifications = self.client.get_notifications(participating=False)
            self.notifications_ready.emit(notifications)
        except Exception as e:
            logging.error(f"Error checking notifications: {e}")
            self.error_occurred.emit(str(e))
