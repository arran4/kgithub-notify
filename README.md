# 🚀 Kgithub-notify

A sleek GitHub notification system tray application written natively in C++ using Qt6 and KDE Frameworks 6 (KF6). It quietly monitors your GitHub account, notifying you when there are new notifications, and provides a rich desktop interface to manage them.

<div align="center">
  <img src="logo.png" alt="kgithub-notify logo" width="128" />
</div>

<p align="center">
    <img src="docs/Screenshot_20260221_210604.png" alt="Main Interface" width="45%" />
    <img src="docs/Screenshot_20260221_210702.png" alt="Notification Item" width="45%" />
</p>

## ✨ Features

Kgithub-notify acts as a mini-client for managing your daily GitHub workflows directly from your desktop.

### 🔔 Smart Notification Management
* **Rich Notification List**: View, filter (Inbox, Unread, Read, All), sort, and search your notifications natively.
* **Periodic Polling**: Quietly checks for new notifications every 5 minutes.
* **Desktop Alerts**: Natively integrates with your desktop environment for system notifications using KDE's KNotification.
* **Quick Actions**: Double-click to view properties, or right-click to Open in Browser, Mark as Read, Mark as Done, Copy Link, and View Raw JSON.
* **Differential Updates**: Smooth UI refresh strategy to prevent annoying screen flicker.

### 🛠️ Developer Tools & Work Items
* **Trending Explorer**: Browse trending repositories and developers right from the `Tools` menu, complete with language filters and "star" indicators for repos you already follow.
* **Work Item Hub**: Automatically aggregate your "Open Issues" and "Open Pull Requests" based on involvement (assigned, mentioned, authored, review-requested).
* **Detailed Views**: Dedicated windows to inspect Pull Requests, GitHub Actions (Check Suites/Workflows), and raw JSON payloads.
* **Repository List**: Export and view a complete list of your repositories directly to CSV.

### 🐧 Deep KDE Integration (KF6)
* **Native Feel**: Built upon KXmlGuiWindow to adhere perfectly to KDE Human Interface Guidelines (HIG).
* **Secure Token Storage**: Safely stores your Personal Access Token (PAT) utilizing KWallet. No plain-text secrets on your drive!
* **System Tray**: Unobtrusive background execution with a handy context menu.

## 📦 Prerequisites

* **C++ Compiler**: Must support C++17 (e.g., GCC 7+, Clang 5+, MSVC 2017+).
* **CMake**: Version 3.10 or higher.
* **Qt6 & KDE Frameworks 6**:

### Installing Dependencies (Debian / Ubuntu 24.10+)

```bash
sudo apt update
sudo apt install build-essential cmake qt6-base-dev qt6-tools-dev-tools qt6-svg-dev \
  libkf6notifications-dev libkf6wallet-dev libkf6coreaddons-dev libkf6xmlgui-dev \
  libkf6configwidgets-dev libkf6i18n-dev
```
> **Note:** If you are on an older distribution (like Ubuntu 24.04) that lacks standard KF6 packages, you can use the provided `.jules/Dockerfile` to compile the application inside an isolated Debian Testing container.

## 🚀 Build Instructions

1. **Clone the repository**:
    ```bash
    git clone https://github.com/arran4/Kgithub-notify.git
    cd Kgithub-notify
    ```

2. **Create a build directory**:
    ```bash
    mkdir build
    cd build
    ```

3. **Configure and compile**:
    ```bash
    cmake ..
    make -j$(nproc)
    ```

4. **Run the application**:
    ```bash
    ./kgithub-notify
    ```

## ⚙️ Configuration

On the first run, or by selecting "Settings" from the File menu (or tray icon context menu), you need to provide a GitHub Personal Access Token (PAT).

1. Go to [GitHub Settings > Developer settings > Personal access tokens](https://github.com/settings/tokens).
2. Generate a new token. You can use either a **Classic Token** or a **Fine-grained Token**.

### Token Scopes

To ensure the application can fetch your notifications and their details (especially for private repositories), your token must have the correct permissions.

**For a Classic Token:**
* Select the `notifications` scope to access your inbox.
* To receive notifications and fetch details (like pull request status or issue authors) for **private repositories**, you **must** also select the full `repo` scope.

**For a Fine-grained Token:**
* **Repository Access:** Select the specific repositories you want to monitor, or "All repositories".
* **Permissions:** Under "Repository permissions", grant **Read-only** access to:
    * `Pull requests` (Required to fetch pull request details)
    * `Issues` (Required to fetch issue details)
    * `Metadata` (Usually required automatically)
* **User Permissions:** Under "User permissions", grant **Read-only** access to:
    * `Notifications` (Required to access your inbox)

3. Copy the token and paste it into the application's settings dialog.

## 📄 License

This project is licensed under the BSD 3-Clause License - see the [LICENSE](LICENSE) file for details.
