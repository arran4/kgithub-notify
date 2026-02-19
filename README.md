# Kgithub-notify

A GitHub Notification Tool written in Python that notifies you when there is a new GitHub notification.
It sits in the system tray and provides a window to view and manage notifications.

## Features

*   **System Tray Integration**: Quietly runs in the background.
*   **Desktop Notifications**: Alerts you of new notifications.
*   **Notification Window**: View, dismiss, or open notifications in the browser.
*   **Authentication**: Supports Personal Access Tokens (PAT).

## Prerequisites

*   Python 3.6 or higher
*   pip

## Installation & Running

1.  Clone the repository:
    ```bash
    git clone https://github.com/yourusername/Kgithub-notify.git
    cd Kgithub-notify
    ```

2.  Run the application using the provided script:
    ```bash
    ./run.sh
    ```

    Alternatively, you can install dependencies manually and run:
    ```bash
    pip install -r requirements.txt
    python3 src/main.py
    ```

## Configuration

On the first run, the application will prompt you for your GitHub Personal Access Token.
You can generate one [here](https://github.com/settings/tokens).
Make sure to grant the `notifications` scope.

To change the token later, right-click the tray icon and select "Settings".

## License

This project is licensed under the BSD 3-Clause License - see the [LICENSE](LICENSE) file for details.
