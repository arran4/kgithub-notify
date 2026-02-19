import unittest
from unittest.mock import MagicMock, patch
import sys
import os

# Add src to path
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), '../src')))

from github_client import GitHubClient

class TestGitHubClient(unittest.TestCase):
    def setUp(self):
        self.client = GitHubClient("fake_token")

    @patch('requests.Session.get')
    def test_get_notifications(self, mock_get):
        # Mock response
        mock_response = MagicMock()
        mock_response.status_code = 200
        mock_response.json.return_value = [{"id": "1", "subject": {"title": "Test"}}]
        mock_get.return_value = mock_response

        notifications = self.client.get_notifications()
        self.assertEqual(len(notifications), 1)
        self.assertEqual(notifications[0]['subject']['title'], "Test")
        mock_get.assert_called_once()

    @patch('requests.Session.patch')
    def test_mark_thread_as_read(self, mock_patch):
        mock_response = MagicMock()
        mock_response.status_code = 205
        mock_patch.return_value = mock_response

        result = self.client.mark_thread_as_read("123")
        self.assertTrue(result)
        mock_patch.assert_called_with('https://api.github.com/notifications/threads/123')

    @patch('requests.Session.get')
    def test_get_user_info(self, mock_get):
        mock_response = MagicMock()
        mock_response.status_code = 200
        mock_response.json.return_value = {"login": "testuser"}
        mock_get.return_value = mock_response

        user_info = self.client.get_user_info()
        self.assertEqual(user_info['login'], "testuser")

    def test_missing_token(self):
        client = GitHubClient()
        with self.assertRaises(ValueError):
            client.get_notifications()

if __name__ == '__main__':
    unittest.main()
