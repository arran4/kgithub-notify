cmake_minimum_required(VERSION 3.10)
project(Kgithub-notify)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 COMPONENTS Widgets Network Test REQUIRED)

add_executable(Kgithub-notify
    src/main.cpp
    src/GitHubClient.cpp
    src/GitHubClient.h
    src/SettingsDialog.cpp
    src/SettingsDialog.h
    src/MainWindow.cpp
    src/MainWindow.h
    src/resources.qrc
)

target_link_libraries(Kgithub-notify
    Qt5::Widgets
    Qt5::Network
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
)

add_test(NAME TestAuth COMMAND TestAuth)

add_executable(TestMainWindow
    tests/TestMainWindow.cpp
    src/MainWindow.cpp
    src/GitHubClient.cpp
    src/SettingsDialog.cpp
    src/resources.qrc
)

target_link_libraries(TestMainWindow
    Qt5::Widgets
    Qt5::Network
    Qt5::Test
)

add_test(NAME TestMainWindow COMMAND TestMainWindow)
