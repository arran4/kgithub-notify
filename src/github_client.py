import requests
import logging

class GitHubClient:
    BASE_URL = "https://api.github.com"

    def __init__(self, token=None):
        self.token = token
        self.session = requests.Session()
        if token:
            self.session.headers.update({"Authorization": f"Bearer {token}"})
        self.session.headers.update({"Accept": "application/vnd.github.v3+json"})

    def set_token(self, token):
        self.token = token
        self.session.headers.update({"Authorization": f"Bearer {token}"})

    def get_notifications(self, all=False, participating=False):
        if not self.token:
            raise ValueError("Token not set")

        params = {
            "all": str(all).lower(),
            "participating": str(participating).lower()
        }

        try:
            response = self.session.get(f"{self.BASE_URL}/notifications", params=params)
            response.raise_for_status()
            return response.json()
        except requests.RequestException as e:
            logging.error(f"Error fetching notifications: {e}")
            return []

    def mark_thread_as_read(self, thread_id):
        if not self.token:
            raise ValueError("Token not set")

        try:
            response = self.session.patch(f"{self.BASE_URL}/notifications/threads/{thread_id}")
            response.raise_for_status()
            return True
        except requests.RequestException as e:
            logging.error(f"Error marking thread {thread_id} as read: {e}")
            return False

    def unsubscribe_thread(self, thread_id):
        if not self.token:
            raise ValueError("Token not set")

        try:
            # Delete subscription
            response = self.session.delete(f"{self.BASE_URL}/notifications/threads/{thread_id}/subscription")
            response.raise_for_status()
            return True
        except requests.RequestException as e:
            logging.error(f"Error unsubscribing thread {thread_id}: {e}")
            return False

    def get_user_info(self):
        if not self.token:
            raise ValueError("Token not set")
        try:
            response = self.session.get(f"{self.BASE_URL}/user")
            response.raise_for_status()
            return response.json()
        except requests.RequestException as e:
            logging.error(f"Error fetching user info: {e}")
            return None
