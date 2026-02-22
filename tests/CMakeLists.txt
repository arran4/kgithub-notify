add_executable(TestGitHubClient
    TestGitHubClient.cpp
    ../src/GitHubClient.cpp
)

target_link_libraries(TestGitHubClient
    Qt5::Test
    Qt5::Network
    Qt5::Widgets
    KF5::Wallet
    KF5::Notifications
)

add_test(NAME TestGitHubClient COMMAND TestGitHubClient)
