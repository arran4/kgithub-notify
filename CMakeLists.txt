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
    src/SettingsDialog.cpp
    src/WalletManager.cpp
    # MainWindow depends on widgets and might be hard to test headless if we included it, but we only test Client/Settings.
    # However SettingsDialog depends on QDialog which is widgets.
    # So we need widgets linked.
)

target_link_libraries(TestMain
    Qt5::Widgets
    Qt5::Network
    Qt5::Test
    KF5::Wallet
)

add_test(NAME TestMain COMMAND TestMain)

add_executable(TestAuth
    tests/TestAuth.cpp
    src/GitHubClient.cpp
    src/SettingsDialog.cpp
    src/WalletManager.cpp
)

target_link_libraries(TestAuth
    Qt5::Widgets
    Qt5::Network
    Qt5::Test
    KF5::Wallet
)

add_test(NAME TestAuth COMMAND TestAuth)

add_executable(TestVerifyToken
    tests/TestVerifyToken.cpp
    src/GitHubClient.cpp
)

target_link_libraries(TestVerifyToken
    Qt5::Widgets
    Qt5::Network
    Qt5::Test
    KF5::Wallet
)

add_test(NAME TestVerifyToken COMMAND TestVerifyToken)

add_executable(TestJsonError
    tests/TestJsonError.cpp
    src/GitHubClient.cpp
)

target_link_libraries(TestJsonError
    Qt5::Widgets
    Qt5::Network
    Qt5::Test
    KF5::Wallet
)

add_executable(TestNotificationWidget
    tests/TestNotificationWidget.cpp
    src/NotificationItemWidget.cpp
    src/GitHubClient.cpp
    src/SettingsDialog.cpp
    src/WalletManager.cpp
)

target_link_libraries(TestNotificationWidget
    Qt5::Widgets
    Qt5::Network
    Qt5::Test
    KF5::Wallet
)

add_test(NAME TestJsonError COMMAND TestJsonError)
add_test(NAME TestNotificationWidget COMMAND TestNotificationWidget)

add_executable(BenchmarkListUpdate
    tests/BenchmarkListUpdate.cpp
    src/NotificationItemWidget.cpp
    src/GitHubClient.cpp
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
