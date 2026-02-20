cmake_minimum_required(VERSION 3.10)
project(Kgithub-notify)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 COMPONENTS Widgets Network REQUIRED)

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
)
