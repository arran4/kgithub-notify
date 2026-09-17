cmake_minimum_required(VERSION 3.10)
project(kgithub-notify VERSION 0.1.27)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 COMPONENTS Widgets Network Svg DBus Test REQUIRED)
find_package(KF6Wallet REQUIRED)
find_package(KF6Notifications REQUIRED)
find_package(KF6Config REQUIRED)

enable_testing()

add_executable(TestGitHubClient
    tests/TestGitHubClient.cpp
    tests/MockNetworkReply.cpp
    tests/FakeNetworkAccessManager.cpp
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
set_tests_properties(TestGitHubClient PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(TestUrlHelper
    tests/TestUrlHelper.cpp
    src/utils/UrlHelper.h
)
target_link_libraries(TestUrlHelper
    Qt6::Test
    Qt6::Gui
)
add_test(NAME TestUrlHelper COMMAND TestUrlHelper)
set_tests_properties(TestUrlHelper PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(TestDesktopEntryHelper
    tests/TestDesktopEntryHelper.cpp
    src/utils/DesktopEntryHelper.cpp
    src/utils/DesktopEntryHelper.h
)
target_link_libraries(TestDesktopEntryHelper
    Qt6::Test
    Qt6::Core
    Qt6::Gui
    KF6::ConfigCore
)
add_test(NAME TestDesktopEntryHelper COMMAND TestDesktopEntryHelper)
set_tests_properties(TestDesktopEntryHelper PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(TestCapabilityVerification
    tests/TestCapabilityVerification.cpp
    tests/MockNetworkReply.cpp
    tests/FakeNetworkAccessManager.cpp
    tests/MockNetworkReply.h
    src/GitHubClient.cpp
    src/GitHubClient.h
    src/Notification.cpp
    src/Notification.h
    src/SecureString.h
)

target_link_libraries(TestCapabilityVerification
        Qt6::Widgets
        Qt6::Network
        Qt6::Svg
        Qt6::Test
)

add_test(NAME TestCapabilityVerification COMMAND TestCapabilityVerification)
set_tests_properties(TestCapabilityVerification PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(TestFilterParser
    tests/TestFilterParser.cpp
    src/utils/FilterParser.cpp
    src/utils/FilterParser.h
)

target_link_libraries(TestFilterParser
        Qt6::Test
        Qt6::Core
)

add_test(NAME TestFilterParser COMMAND TestFilterParser)

add_executable(TestRulesDialog
    tests/TestRulesDialog.cpp
    src/RulesDialog.cpp
    src/RulesDialog.h
    src/NotificationRuleEngine.cpp
    src/NotificationRuleEngine.h
    src/Notification.cpp
    src/Notification.h
    src/utils/FilterParser.cpp
    src/utils/FilterParser.h
)

target_link_libraries(TestRulesDialog
        Qt6::Widgets
        Qt6::Network
        Qt6::Test
        Qt6::Core
        Qt6::Gui
)

add_test(NAME TestRulesDialog COMMAND TestRulesDialog)
set_tests_properties(TestRulesDialog PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")


add_executable(kgithub-notify
    src/main.cpp
    src/GitHubClient.cpp
    src/GitHubClient.h
    src/Notification.cpp
    src/Notification.h
    src/SecureString.h
    src/SettingsDialog.cpp
    src/SettingsDialog.h
    src/NotificationRuleEngine.cpp
    src/NotificationRuleEngine.h
    src/utils/FilterParser.cpp
    src/utils/FilterParser.h
    src/utils/DesktopEntryHelper.cpp
    src/utils/DesktopEntryHelper.h
    src/RulesDialog.cpp
    src/RulesDialog.h
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
    src/NewIssueDialog.cpp
    src/NewIssueDialog.h
    src/resources.qrc
)

find_package(KF6CoreAddons REQUIRED)
find_package(KF6XmlGui REQUIRED)
find_package(KF6ConfigWidgets REQUIRED)
find_package(KF6I18n REQUIRED)

target_link_libraries(kgithub-notify
        Qt6::Widgets
        Qt6::Network
        Qt6::Svg
        Qt6::DBus
        KF6::CoreAddons
        KF6::XmlGui
        KF6::ConfigWidgets
        KF6::I18n
        KF6::Wallet
        KF6::Notifications
)

target_compile_definitions(kgithub-notify PRIVATE KGHN_APP_VERSION="${PROJECT_VERSION}")


# Installation
install(TARGETS kgithub-notify
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    BUNDLE DESTINATION .
)

install(FILES src/kgithub-notify.desktop
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

install(FILES src/com.arran4.kgithub_notify.metainfo.xml
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/metainfo
)

# Exercise the real consumers with manually completed network replies.
get_target_property(consumer_sources kgithub-notify SOURCES)
list(REMOVE_ITEM consumer_sources src/main.cpp)
add_executable(TestRequestConsumers tests/TestRequestConsumers.cpp
    tests/FakeNetworkAccessManager.cpp ${consumer_sources})
get_target_property(consumer_libraries kgithub-notify LINK_LIBRARIES)
target_link_libraries(TestRequestConsumers PRIVATE ${consumer_libraries} Qt6::Test)
add_test(NAME TestRequestConsumers COMMAND TestRequestConsumers)
set_tests_properties(TestRequestConsumers PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(TestCredentialPersistence tests/TestCredentialPersistence.cpp ${consumer_sources})
target_link_libraries(TestCredentialPersistence PRIVATE ${consumer_libraries} Qt6::Test)
add_test(NAME TestCredentialPersistence COMMAND TestCredentialPersistence)
set_tests_properties(TestCredentialPersistence PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

if(BUILD_TESTING)
    add_executable(TestAuthIncidentState tests/TestAuthIncidentState.cpp)
    target_link_libraries(TestAuthIncidentState PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME AuthIncidentState COMMAND TestAuthIncidentState)
endif()
