# Kgithub-notify

A GitHub notification system tray application written in C++ using Qt5. It notifies you when there are new GitHub notifications, allowing you to view them in your browser or dismiss them directly from the application.

## Features

*   **System Tray Integration**: Runs in the background and sits in your system tray.
*   **Periodic Polling**: Checks for new notifications every 5 minutes.
*   **Desktop Notifications**: Shows system notifications when new items arrive.
*   **Quick Actions**:
    *   **Click**: Opens the notification URL in your default browser.
    *   **Right-Click**: Dismisses the notification (marks it as read on GitHub).
*   **Secure**: Stores your Personal Access Token (PAT) securely using Qt settings.

## Prerequisites

*   **C++ Compiler**: Must support C++17 (e.g., GCC 7+, Clang 5+, MSVC 2017+).
*   **CMake**: Version 3.10 or higher.
*   **Qt5**: Requires `Qt5Widgets`, `Qt5Network`, and `Qt5Test`.

### Installing Dependencies (Ubuntu/Debian)

```bash
sudo apt update
sudo apt install build-essential cmake qtbase5-dev qttools5-dev-tools libqt5svg5-dev
```

## Build Instructions

1.  **Clone the repository**:
    ```bash
    git clone https://github.com/yourusername/Kgithub-notify.git
    cd Kgithub-notify
    ```

2.  **Create a build directory**:
    ```bash
    mkdir build
    cd build
    ```

3.  **Configure and build**:
    ```bash
    cmake ..
    cmake --build .
    ```

4.  **Run the application**:
    ```bash
    ./Kgithub-notify
    ```

## Configuration

On the first run, or by selecting "Settings" from the File menu or tray icon context menu, you need to provide a GitHub Personal Access Token (PAT).

1.  Go to [GitHub Settings > Developer settings > Personal access tokens](https://github.com/settings/tokens).
2.  Generate a new token (classic).
3.  Select the `notifications` scope. If you want to receive notifications for private repositories, also select the `repo` scope.
4.  Copy the token and paste it into the application's settings dialog.

## Testing

The project includes unit tests located in the `tests/` directory.

To run the tests:

```bash
cd build
ctest --output-on-failure
```

Or run the test executable directly:

```bash
./TestMain
```

## License

This project is licensed under the BSD 3-Clause License - see the [LICENSE](LICENSE) file for details.
