from PyQt5.QtWidgets import (QDialog, QVBoxLayout, QLabel, QLineEdit,
                             QPushButton, QMessageBox, QHBoxLayout)
import keyring
import json
import os

APP_NAME = "Kgithub-notify"
KEYRING_SERVICE = "github_token"
CONFIG_FILE = os.path.expanduser("~/.config/kgithub-notify/config.json")

class SettingsDialog(QDialog):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Settings")
        self.resize(400, 150)

        layout = QVBoxLayout()

        layout.addWidget(QLabel("GitHub Personal Access Token:"))
        self.token_input = QLineEdit()
        self.token_input.setEchoMode(QLineEdit.Password)
        layout.addWidget(self.token_input)

        # Load existing token
        existing_token = self.load_token()
        if existing_token:
            self.token_input.setText(existing_token)

        btn_layout = QHBoxLayout()
        self.save_btn = QPushButton("Save")
        self.save_btn.clicked.connect(self.save_token)
        self.cancel_btn = QPushButton("Cancel")
        self.cancel_btn.clicked.connect(self.reject)

        btn_layout.addWidget(self.save_btn)
        btn_layout.addWidget(self.cancel_btn)
        layout.addLayout(btn_layout)

        self.setLayout(layout)

    def load_token(self):
        try:
            token = keyring.get_password(APP_NAME, KEYRING_SERVICE)
            if token:
                return token
        except Exception:
            pass

        # Fallback to config file
        if os.path.exists(CONFIG_FILE):
            try:
                with open(CONFIG_FILE, 'r') as f:
                    config = json.load(f)
                    return config.get('token')
            except Exception:
                pass
        return None

    def save_token(self):
        token = self.token_input.text().strip()
        if not token:
            QMessageBox.warning(self, "Error", "Token cannot be empty")
            return

        try:
            keyring.set_password(APP_NAME, KEYRING_SERVICE, token)
        except Exception:
            # Fallback to config file
            try:
                os.makedirs(os.path.dirname(CONFIG_FILE), exist_ok=True)
                with open(CONFIG_FILE, 'w') as f:
                    json.dump({'token': token}, f)
            except Exception as e:
                QMessageBox.critical(self, "Error", f"Failed to save token: {e}")
                return

        self.accept()
