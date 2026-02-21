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

add_executable(Kgithub-notify
    src/main.cpp
    src/GitHubClient.cpp
    src/GitHubClient.h
    src/SettingsDialog.cpp
    src/SettingsDialog.h
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
)

enable_testing()

add_executable(TestMain
    tests/TestMain.cpp
    src/GitHubClient.cpp
    src/SettingsDialog.cpp
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
)

target_link_libraries(TestAuth
    Qt5::Widgets
    Qt5::Network
    Qt5::Test
    KF5::Wallet
)

add_test(NAME TestAuth COMMAND TestAuth)

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
