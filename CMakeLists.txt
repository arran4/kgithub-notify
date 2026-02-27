cmake_minimum_required(VERSION 3.10)
project(kgithub-notify VERSION 0.1.0)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 COMPONENTS Widgets Network Svg DBus Test REQUIRED)
find_package(KF5Wallet REQUIRED)
find_package(KF5Notifications REQUIRED)

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
    Qt5::Widgets
    Qt5::Network
    Qt5::Svg
    Qt5::Test
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
    src/trending/TrendingWindow.cpp
    src/trending/TrendingWindow.h
    src/WalletManager.cpp
    src/WalletManager.h
    src/MainWindow.cpp
    src/MainWindow.h
    src/AuthErrorNotification.cpp
    src/AuthErrorNotification.h
    src/NotificationItemWidget.cpp
    src/NotificationItemWidget.h
    src/NotificationListWidget.cpp
    src/NotificationListWidget.h
    src/resources.qrc
)

target_link_libraries(kgithub-notify
    Qt5::Widgets
    Qt5::Network
    Qt5::Svg
    Qt5::DBus
    KF5::Wallet
    KF5::Notifications
)

target_compile_definitions(kgithub-notify PRIVATE KGHN_APP_VERSION="${PROJECT_VERSION}")


# Installation
install(TARGETS kgithub-notify
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    BUNDLE DESTINATION .
)

install(FILES kgithub-notify.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
    RENAME org.kgithub_notify.desktop
)

install(FILES src/assets/icon.svg
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
    RENAME kgithub-notify.svg
)

install(FILES src/kgithub-notify.notifyrc
    DESTINATION ${CMAKE_INSTALL_DATADIR}/knotifications5
)
