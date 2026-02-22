cmake_minimum_required(VERSION 3.10)
project(Kgithub-notify)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 COMPONENTS Widgets Network Test REQUIRED)
find_package(KF5Wallet REQUIRED)
find_package(KF5Notifications REQUIRED)

add_executable(Kgithub-notify
    src/main.cpp
    src/GitHubClient.cpp
    src/GitHubClient.h
    src/SettingsDialog.cpp
    src/SettingsDialog.h
    src/WalletManager.cpp
    src/WalletManager.h
    src/MainWindow.cpp
    src/MainWindow.h
    src/AuthErrorNotification.cpp
    src/AuthErrorNotification.h
    src/NotificationItemWidget.cpp
    src/NotificationItemWidget.h
    src/NotificationTextFormatter.cpp
    src/NotificationTextFormatter.h
    src/GitHubResponseParser.cpp
    src/GitHubResponseParser.h
    src/resources.qrc
)

target_link_libraries(Kgithub-notify
    Qt5::Widgets
    Qt5::Network
    KF5::Wallet
    KF5::Notifications
)

enable_testing()

add_executable(TestMain
    tests/TestMain.cpp
    src/GitHubClient.cpp
    src/GitHubResponseParser.cpp
    src/SettingsDialog.cpp
    src/WalletManager.cpp
)

target_link_libraries(TestMain
    Qt5::Widgets
    Qt5::Network
    Qt5::Test
    KF5::Wallet
)

add_test(NAME TestMain COMMAND TestMain)

add_executable(TestNotificationTextFormatter
    tests/TestNotificationTextFormatter.cpp
    src/NotificationTextFormatter.cpp
    src/GitHubClient.cpp
    src/GitHubResponseParser.cpp
)

target_link_libraries(TestNotificationTextFormatter
    Qt5::Widgets
    Qt5::Network
    Qt5::Test
    KF5::Wallet
)

add_test(NAME TestNotificationTextFormatter COMMAND TestNotificationTextFormatter)

add_executable(TestGitHubResponseParser
    tests/TestGitHubResponseParser.cpp
    src/GitHubResponseParser.cpp
    src/GitHubClient.cpp
)

target_link_libraries(TestGitHubResponseParser
    Qt5::Widgets
    Qt5::Network
    Qt5::Test
    KF5::Wallet
)

add_test(NAME TestGitHubResponseParser COMMAND TestGitHubResponseParser)


add_executable(BenchmarkListUpdate
    tests/BenchmarkListUpdate.cpp
    src/NotificationItemWidget.cpp
    src/NotificationTextFormatter.cpp
    src/GitHubClient.cpp
    src/GitHubResponseParser.cpp
)

target_link_libraries(BenchmarkListUpdate
    Qt5::Widgets
    Qt5::Network
)

# Installation
install(TARGETS Kgithub-notify
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    BUNDLE DESTINATION .
)

install(FILES kgithub-notify.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)

install(FILES src/assets/icon.png
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/1024x1024/apps
    RENAME kgithub-notify.png
)
