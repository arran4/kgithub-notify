#include <QAction>
#include <QDesktopServices>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include "../src/ActionWindow.h"
#include "../src/DebugWindow.h"
#include "../src/MainWindow.h"
#include "../src/NewIssueDialog.h"
#include "../src/PullRequestWindow.h"
#include "../src/RepoListWindow.h"
#include "../src/SettingsDialog.h"
#include "../src/trending/TrendingWindow.h"
#include "FakeNetworkAccessManager.h"

class TestRequestConsumers : public QObject {
    Q_OBJECT

    QTemporaryDir storage;
    QList<QUrl> openedUrls;

   private slots:
    void recordUrl(const QUrl& url) { openedUrls.append(url); }

   private:
    FakeNetworkAccessManager* installNetwork(GitHubClient& client) {
        delete client.manager;
        auto* manager = new FakeNetworkAccessManager(&client);
        manager->autoEmitFinished = false;
        client.manager = manager;
        connect(manager, &QNetworkAccessManager::finished, &client, &GitHubClient::onReplyFinished);
        client.setToken("test-token");
        return manager;
    }

    static QByteArray repos(const char* name) {
        QJsonObject repo{
            {"name", name}, {"full_name", QString("owner/") + name}, {"owner", QJsonObject{{"login", "owner"}}}};
        return QJsonDocument(QJsonArray{repo}).toJson();
    }

    void prepareMain(MainWindow& window, GitHubClient& client) {
        disconnect(window.tokenWatcher, nullptr, &window, nullptr);
        window.m_loadedToken.clear();
        window.trayIcon->hide();
        window.authNotificationSent = true;
        window.setClient(&client);
        window.refreshTimer->stop();
        QSettings().setValue("dataOption", SettingsDialog::Manual);
    }

    void verify(NewIssueDialog& dialog, const QString& repo) {
        dialog.setInitialRepo(repo);
        dialog.m_verifyTimer->stop();
        dialog.verifyRepo();
    }

   private slots:
    void initTestCase() {
        QVERIFY(storage.isValid());
        QDesktopServices::setUrlHandler("https", this, "recordUrl");
        QStandardPaths::setTestModeEnabled(true);
        qputenv("DBUS_SESSION_BUS_ADDRESS", "unix:path=/nonexistent-request-consumer-test-bus");
        qputenv("XDG_DATA_HOME", storage.path().toUtf8());
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, storage.path());
        QCoreApplication::setOrganizationName("request-consumer-tests");
        QCoreApplication::setApplicationName("request-consumer-tests");
    }

    void init() {
        QFile::remove(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/repos_cache.json");
    }

    void testDebugAndTrendingSimultaneous() {
        GitHubClient client;
        auto* network = installNetwork(client);
        DebugWindow debug(&client);
        debug.setEndpoint("/user");
        debug.sendRequest();
        TrendingWindow trending(&client);
        QCOMPARE(network->requests.size(), 2);
        network->requests[1].reply->complete("{\"items\":[]}");
        QVERIFY(trending.refreshButton->isEnabled());
        QVERIFY(!debug.m_sendButton->isEnabled());
        QCOMPARE(debug.m_responseOutput->toPlainText(), QString("Loading..."));
        QCOMPARE(trending.tableWidget->item(0, 0)->text(), QString("No results found."));
        network->requests[0].reply->complete("{\"items\":[{\"name\":\"debug-only\"}]}");
        QVERIFY(debug.m_sendButton->isEnabled());
        QVERIFY(debug.m_responseOutput->toPlainText().contains("debug-only"));
        QCOMPARE(trending.tableWidget->item(0, 0)->text(), QString("No results found."));
    }

    void testRepoListAndNewIssuePagination() {
        GitHubClient client;
        auto* network = installNetwork(client);
        RepoListWindow list(&client);
        NewIssueDialog issue(&client);
        list.onRefreshClicked();
        issue.onRefreshClicked();
        const QUuid issueId = issue.m_repoLoadRequestId;
        network->requests[1].reply->setRawHeader("Link", "<https://api.github.com/user/repos?page=2>; rel=\"next\"");
        network->requests[1].reply->complete(repos("issue-first"));
        QCOMPARE(network->requests[2].reply->property("reqId").toUuid(), issueId);
        QVERIFY(list.m_allRepos.isEmpty());
        QVERIFY(!issue.m_refreshButton->isEnabled());
        network->requests[0].reply->complete(repos("list-only"));
        QVERIFY(list.m_refreshAction->isEnabled());
        QCOMPARE(list.m_allRepos.size(), 1);
        network->requests[2].reply->complete(repos("issue-second"));
        QVERIFY(issue.m_refreshButton->isEnabled());
        QCOMPARE(issue.m_allRepos.size(), 2);
        QCOMPARE(list.m_allRepos[0].toObject()["name"].toString(), QString("list-only"));
    }

    void testTwoNewIssueDialogs() {
        GitHubClient client;
        auto* network = installNetwork(client);
        NewIssueDialog first(&client), second(&client);
        first.onRefreshClicked();
        second.onRefreshClicked();
        network->requests[1].reply->complete(repos("second"));
        QVERIFY(!first.m_refreshButton->isEnabled());
        network->requests[0].reply->complete(repos("first"));
        QCOMPARE(first.m_allRepos[0].toObject()["name"].toString(), QString("first"));
        QCOMPARE(second.m_allRepos[0].toObject()["name"].toString(), QString("second"));
        verify(first, "uncached/first");
        verify(second, "uncached/second");
        network->requests[3].reply->complete("{}");
        QVERIFY(second.m_createButton->isEnabled());
        QVERIFY(!first.m_createButton->isEnabled());
        network->requests[2].reply->complete("{}");
        first.m_titleEdit->setText("First title");
        second.m_titleEdit->setText("Second title");
        first.onCreateClicked();
        second.onCreateClicked();
        QSignalSpy firstAccepted(&first, &QDialog::accepted);
        QSignalSpy secondAccepted(&second, &QDialog::accepted);
        network->requests[5].reply->complete("{\"html_url\":\"https://github.com/uncached/second/issues/1\"}");
        QCOMPARE(secondAccepted.count(), 1);
        QCOMPARE(firstAccepted.count(), 0);
        QCOMPARE(openedUrls.last(), QUrl("https://github.com/uncached/second/issues/1"));
        QVERIFY(second.m_createButton->isEnabled());
        QVERIFY(!first.m_createButton->isEnabled());
        network->requests[4].reply->completeWithError(QNetworkReply::TimeoutError, "Request timed out");
        QVERIFY(first.m_createButton->isEnabled());
        QCOMPARE(first.m_titleEdit->text(), QString("First title"));
        QVERIFY(first.m_statusLabel->text().contains("timed out"));
        QVERIFY(!second.m_statusLabel->text().contains("timed out"));
    }

    void testIndependentTimeoutsAndCancellation_data() {
        QTest::addColumn<int>("error");
        QTest::newRow("timeout") << int(QNetworkReply::TimeoutError);
        QTest::newRow("cancel") << int(QNetworkReply::OperationCanceledError);
        QTest::newRow("network") << int(QNetworkReply::ConnectionRefusedError);
    }

    void testIndependentTimeoutsAndCancellation() {
        QFETCH(int, error);
        GitHubClient client;
        auto* network = installNetwork(client);
        DebugWindow debug(&client);
        debug.setEndpoint("/user");
        debug.sendRequest();
        TrendingWindow trending(&client);
        network->requests[1].reply->completeWithError(QNetworkReply::NetworkError(error), "Second failed");
        QVERIFY(trending.refreshButton->isEnabled());
        QVERIFY(!debug.m_sendButton->isEnabled());
        network->requests[0].reply->completeWithError(QNetworkReply::NetworkError(error), "First failed");
        QVERIFY(debug.m_sendButton->isEnabled());
        QVERIFY(debug.m_responseOutput->toPlainText().contains("First failed"));
    }

    void testStaleSuccessAndErrorAfterRefresh() {
        GitHubClient client;
        auto* network = installNetwork(client);
        DebugWindow debug(&client);
        debug.setEndpoint("/user");
        debug.sendRequest();
        debug.sendRequest();
        debug.sendRequest();
        network->requests[2].reply->complete("newest");
        network->requests[0].reply->complete("old");
        network->requests[1].reply->completeWithError(QNetworkReply::TimeoutError, "old timeout");
        QCOMPARE(debug.m_responseOutput->toPlainText(), QString("newest"));
        QVERIFY(debug.m_sendButton->isEnabled());
        debug.sendRequest();
        // A completion for an already retired request cannot unlock the newer request.
        client.errorOccurred(network->requests[2].reply->property("reqId").toUuid(), "duplicate");
        QVERIFY(!debug.m_sendButton->isEnabled());
        network->requests[3].reply->abort();
        QVERIFY(debug.m_sendButton->isEnabled());
    }

    void testDestructionBeforeReply() {
        GitHubClient client;
        auto* network = installNetwork(client);
        auto* debug = new DebugWindow(&client);
        debug->setEndpoint("/user");
        debug->sendRequest();
        QPointer<DebugWindow> guard(debug);
        QPointer<QNetworkReply> reply(network->requests[0].reply);
        delete debug;
        QVERIFY(guard.isNull());
        network->requests[0].reply->complete("late response");
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(reply.isNull());
    }

    void testMissingTokenRestoresButtons() {
        GitHubClient client;
        auto* network = installNetwork(client);
        client.setToken("");
        DebugWindow debug(&client);
        debug.setEndpoint("/user");
        RepoListWindow repos(&client);
        NewIssueDialog issue(&client);
        TrendingWindow trending(&client);
        debug.sendRequest();
        repos.onRefreshClicked();
        issue.onRefreshClicked();
        trending.onRefreshClicked();
        QVERIFY(debug.m_sendButton->isEnabled());
        QVERIFY(repos.m_refreshAction->isEnabled());
        QVERIFY(issue.m_refreshButton->isEnabled());
        QVERIFY(!issue.m_isFetchingRepos);
        QVERIFY(trending.refreshButton->isEnabled());
        QVERIFY(network->requests.isEmpty());
    }

    void testSettingsVerificationIsolation() {
        SettingsDialog settings;
        settings.tokenEdit->setText("first-token");
        settings.testClient = new GitHubClient(&settings);
        connect(settings.testClient, &GitHubClient::tokenVerified, &settings, &SettingsDialog::onVerificationResult);
        auto* network = installNetwork(*settings.testClient);
        settings.onTestClicked();
        settings.tokenEdit->setText("second-token");
        settings.onTestClicked();
        network->requests[1].reply->complete("{\"login\":\"new-user\"}");
        QVERIFY(settings.testButton->isEnabled());
        QVERIFY(settings.statusLabel->text().contains("new-user"));
        network->requests[0].reply->completeWithError(QNetworkReply::TimeoutError, "stale timeout");
        QVERIFY(settings.statusLabel->text().contains("new-user"));
        settings.onTestClicked();
        QVERIFY(!settings.testButton->isEnabled());
        network->requests[2].reply->abort();
        QVERIFY(settings.testButton->isEnabled());
        QVERIFY(settings.statusLabel->text().contains("cancelled"));
        settings.onTestClicked();
        network->requests[3].reply->completeWithError(QNetworkReply::TimeoutError, "Request timed out");
        QVERIFY(settings.testButton->isEnabled());
        QVERIFY(settings.statusLabel->text().contains("timed out"));
        settings.onTestClicked();
        settings.tokenEdit->clear();
        settings.onTestClicked();
        network->requests[4].reply->complete("{}");
        QVERIFY(settings.testButton->isEnabled());
        QVERIFY(settings.statusLabel->text().contains("enter a token"));
    }

    void testIssueCreationFailures_data() { testIndependentTimeoutsAndCancellation_data(); }

    void testIssueCreationFailures() {
        QFETCH(int, error);
        GitHubClient client;
        auto* network = installNetwork(client);
        NewIssueDialog issue(&client);
        verify(issue, "uncached/repo");
        network->requests[0].reply->complete("{}");
        issue.m_titleEdit->setText("Keep this title");
        issue.m_bodyEdit->setPlainText("Keep this body");
        issue.onCreateClicked();
        QVERIFY(!issue.m_createButton->isEnabled());
        network->requests[1].reply->completeWithError(QNetworkReply::NetworkError(error), "creation failed");
        QVERIFY(issue.m_createButton->isEnabled());
        QCOMPARE(issue.m_titleEdit->text(), QString("Keep this title"));
        QCOMPARE(issue.m_bodyEdit->toPlainText(), QString("Keep this body"));
        client.setToken("");
        issue.onCreateClicked();
        QVERIFY(issue.m_createButton->isEnabled());
        QVERIFY(issue.m_statusLabel->text().contains("No token"));
    }

    void testRepositoryFailures_data() { testIndependentTimeoutsAndCancellation_data(); }

    void testRepositoryFailures() {
        QFETCH(int, error);
        GitHubClient client;
        auto* network = installNetwork(client);
        RepoListWindow list(&client);
        NewIssueDialog issue(&client);
        list.onRefreshClicked();
        issue.onRefreshClicked();
        network->requests[0].reply->completeWithError(QNetworkReply::NetworkError(error), "list failed");
        QVERIFY(list.m_refreshAction->isEnabled());
        QVERIFY(!issue.m_refreshButton->isEnabled());
        network->requests[1].reply->completeWithError(QNetworkReply::NetworkError(error), "issue failed");
        QVERIFY(issue.m_refreshButton->isEnabled());
        QVERIFY(!issue.m_isFetchingRepos);
    }

    void testNotificationFailures_data() { testIndependentTimeoutsAndCancellation_data(); }

    void testNotificationFailures() {
        QFETCH(int, error);
        GitHubClient client;
        auto* network = installNetwork(client);
        MainWindow window;
        prepareMain(window, client);
        window.onRefreshClicked();
        auto* page = network->requests[0].reply;
        page->setRawHeader("Link", "<https://api.github.com/notifications?page=2>; rel=\"next\"");
        page->complete("[]");
        auto* list = window.notificationListWidget;
        auto* button = qobject_cast<QPushButton*>(list->listWidget->itemWidget(list->loadMoreItem));
        QVERIFY(button);
        list->onLoadMoreClicked();
        QVERIFY(!button->isEnabled());
        network->requests[1].reply->completeWithError(QNetworkReply::NetworkError(error), "page failed");
        QVERIFY(button->isEnabled());
        QVERIFY(!window.m_notificationLoading);
        client.setToken("");
        list->onLoadMoreClicked();
        QVERIFY(button->isEnabled());
        QVERIFY(!window.m_notificationLoading);
        client.setToken("test-token");
        list->onLoadMoreClicked();
        QVERIFY(!button->isEnabled());
        network->requests[2].reply->complete("[]");
        QVERIFY(!window.m_notificationLoading);
        QCOMPARE(window.statusLabel->text(), QString("Updated"));
        window.onRefreshClicked();
        window.onRefreshClicked();
        window.notificationListWidget->loadMoreRequested();
        QCOMPARE(network->requests.size(), 5);
        QVERIFY(window.m_notificationLoading);
        network->requests[4].reply->complete("[]");
        network->requests[3].reply->completeWithError(QNetworkReply::NetworkError(error), "stale failed");
        QCOMPARE(window.statusLabel->text(), QString("Updated"));
        client.setToken("");
        window.onRefreshClicked();
        QVERIFY(!window.m_notificationLoading);
        QCOMPARE(window.stackWidget->currentWidget(), window.errorPage);
    }

    void testPrAndActionTimeoutRetryIsolation() {
        GitHubClient client;
        client.setToken("test-token");
        FakeNetworkAccessManager prNetwork, actionNetwork;
        prNetwork.autoEmitFinished = false;
        actionNetwork.autoEmitFinished = false;
        Notification notification;
        notification.url = "https://api.github.com/repos/o/r/pulls/1";
        PullRequestWindow pr(notification, &client, nullptr, &prNetwork);
        notification.url = "https://api.github.com/repos/o/r/actions/runs/1";
        ActionWindow action(notification, &client, nullptr, &actionNetwork);
        QCOMPARE(prNetwork.requests[0].request.transferTimeout(), 30000);
        QCOMPARE(actionNetwork.requests[0].request.transferTimeout(), 30000);
        actionNetwork.requests[0].reply->completeWithError(QNetworkReply::TimeoutError, "Action timed out");
        QVERIFY(action.m_retryButton->isEnabled());
        QVERIFY(action.m_requestStatus->text().contains("Action timed out"));
        QVERIFY(!pr.m_retryButton->isEnabled());
        prNetwork.requests[0].reply->completeWithError(QNetworkReply::TimeoutError, "PR timed out");
        QVERIFY(pr.m_retryButton->isEnabled());
        QVERIFY(pr.m_requestStatus->text().contains("PR timed out"));
        action.fetchRunDetails();
        action.fetchRunDetails();
        actionNetwork.requests[2].reply->complete("{\"name\":\"new action\"}");
        actionNetwork.requests[1].reply->completeWithError(QNetworkReply::TimeoutError, "stale failure");
        QVERIFY(action.m_statusLabel->text().contains("new action"));
        QVERIFY(!action.m_requestStatus->text().contains("stale"));
        pr.fetchPrDetails();
        pr.fetchPrDetails();
        prNetwork.requests[1].reply->completeWithError(QNetworkReply::TimeoutError, "stale PR failure");
        QVERIFY(!pr.m_retryButton->isEnabled());
        prNetwork.requests[2].reply->abort();
        QVERIFY(pr.m_retryButton->isEnabled());
        QVERIFY(pr.m_requestStatus->text().contains("cancelled"));
    }

    void testNotificationDetailsIgnoreOlderRequests() {
        GitHubClient client;
        auto* network = installNetwork(client);
        MainWindow window;
        prepareMain(window, client);
        auto* list = window.notificationListWidget;
        list->requestDetails("https://api.github.com/repos/o/r/issues/1", "1");
        list->requestDetails("https://api.github.com/repos/o/r/issues/1", "1");
        network->requests[1].reply->complete("{\"user\":{\"login\":\"current author\"}}");
        network->requests[0].reply->complete("{\"user\":{\"login\":\"old author\"}}");
        QCOMPARE(list->detailsCache["1"].author, QString("current author"));
        list->requestDetails("https://api.github.com/repos/o/r/issues/1", "1");
        window.onRefreshClicked();
        network->requests[3].reply->complete("[]");
        network->requests[2].reply->complete("{\"user\":{\"login\":\"stale generation\"}}");
        QCOMPARE(list->detailsCache["1"].author, QString("current author"));
    }

    void testNotificationPaginationGeneration() {
        GitHubClient client;
        auto* network = installNetwork(client);
        MainWindow window;
        prepareMain(window, client);
        window.onRefreshClicked();
        window.onRefreshClicked();
        const QUuid session = window.m_currentRefreshId;
        auto* current = network->requests[1].reply;
        current->setRawHeader("Link", "<https://api.github.com/notifications?page=2>; rel=\"next\"");
        current->complete("[{\"id\":\"new\",\"subject\":{\"title\":\"current title\"}}]");
        network->requests[0].reply->setRawHeader("Link",
                                                 "<https://api.github.com/notifications?page=99>; rel=\"next\"");
        network->requests[0].reply->complete("[{\"id\":\"old\",\"subject\":{\"title\":\"stale title\"}}]");
        QCOMPARE(window.notificationListWidget->m_allNotifications.size(), 1);
        QCOMPARE(window.notificationListWidget->m_allNotifications[0].title, QString("current title"));
        QCOMPARE(client.m_nextPageUrl, QString("https://api.github.com/notifications?page=2"));
        window.notificationListWidget->onLoadMoreClicked();
        QCOMPARE(network->requests.size(), 3);
        QCOMPARE(network->requests[2].reply->property("reqId").toUuid(), session);
        network->requests[2].reply->setRawHeader("Link", "<https://api.github.com/notifications?page=3>; rel=\"next\"");
        network->requests[2].reply->complete("[]");
        window.notificationListWidget->onLoadMoreClicked();
        QCOMPARE(network->requests[3].reply->property("reqId").toUuid(), session);
        window.onRefreshClicked();
        window.notificationListWidget->loadMoreRequested();
        QCOMPARE(network->requests.size(), 5);
        QVERIFY(window.m_notificationLoading);
        network->requests[4].reply->complete("[]");
        network->requests[3].reply->setRawHeader("Link", "<https://api.github.com/notifications?page=4>; rel=\"next\"");
        network->requests[3].reply->complete("[{\"id\":\"stale-page\"}]");
        QVERIFY(window.notificationListWidget->m_allNotifications.isEmpty());
        QVERIFY(client.m_nextPageUrl.isEmpty());
        QCOMPARE(window.statusLabel->text(), QString("Updated"));
    }
};

QTEST_MAIN(TestRequestConsumers)
#include "TestRequestConsumers.moc"
