cmake_minimum_required(VERSION 3.10)
project(kgithub-notify VERSION 0.1.0)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 COMPONENTS Widgets Network Svg DBus Test REQUIRED)
find_package(KF6Wallet REQUIRED)
find_package(KF6Notifications REQUIRED)

enable_testing()

add_executable(TestGitHubClient
    tests/TestGitHubClient.cpp
    tests/MockNetworkReply.cpp
    tests/MockNetworkReply.h
    src/GitHubClient.cpp
    src/GitHubClient.h
    src/Notification.cpp
    src/Notification.h
    src/SecureString.h
)

target_link_libraries(TestGitHubClient
    Qt6::Widgets
    Qt6::Network
    Qt6::Svg
    Qt6::Test
)

add_test(NAME TestGitHubClient COMMAND TestGitHubClient)

add_executable(kgithub-notify
    src/main.cpp
    src/GitHubClient.cpp
    src/GitHubClient.h
    src/Notification.cpp
    src/Notification.h
    src/SecureString.h
    src/SettingsDialog.cpp
    src/SettingsDialog.h
    src/DebugWindow.cpp
    src/DebugWindow.h
    src/RepoListWindow.cpp
    src/RepoListWindow.h
    src/trending/TrendingWindow.cpp
    src/trending/TrendingWindow.h
    src/WalletManager.cpp
    src/WalletManager.h
    src/MainWindow.cpp
    src/MainWindow.h
    src/PopupNotification.cpp
    src/PopupNotification.h
    src/NotificationItemWidget.cpp
    src/NotificationItemWidget.h
    src/NotificationListWidget.cpp
    src/NotificationListWidget.h
    src/WorkItemWindow.cpp
    src/WorkItemWindow.h
    src/NotificationWindow.cpp
    src/NotificationWindow.h
    src/PullRequestWindow.cpp
    src/PullRequestWindow.h
    src/ActionWindow.cpp
    src/ActionWindow.h
    src/resources.qrc
)

find_package(KF6CoreAddons REQUIRED)
find_package(KF6XmlGui REQUIRED)
find_package(KF6ConfigWidgets REQUIRED)

target_link_libraries(kgithub-notify
    Qt6::Widgets
    Qt6::Network
    Qt6::Svg
    Qt6::DBus
    KF6::CoreAddons
    KF6::XmlGui
    KF6::ConfigWidgets
    KF6::Wallet
    KF6::Notifications
)

target_compile_definitions(kgithub-notify PRIVATE KGHN_APP_VERSION="${PROJECT_VERSION}")


# Installation
install(TARGETS kgithub-notify
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    BUNDLE DESTINATION .
)

install(FILES kgithub-notify.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
    RENAME com.arran4.kgithub_notify.desktop
)

install(FILES src/assets/icon.svg
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
    RENAME kgithub-notify.svg
)

install(FILES src/kgithub-notify.notifyrc
    DESTINATION ${CMAKE_INSTALL_DATADIR}/knotifications6
)

install(FILES src/kgithub-notifyui.rc
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/kxmlgui6/kgithub-notify
)
