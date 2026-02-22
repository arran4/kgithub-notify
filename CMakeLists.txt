cmake_minimum_required(VERSION 3.10)
project(Kgithub-notify)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 COMPONENTS Widgets Network Svg REQUIRED)
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
    Qt5::Svg
    KF5::Wallet
    KF5::Notifications
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

install(FILES src/kgithub-notify.notifyrc
    DESTINATION ${CMAKE_INSTALL_DATADIR}/knotifications5
)
