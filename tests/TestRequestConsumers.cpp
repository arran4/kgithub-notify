#include <QAction>
#include <QDesktopServices>
#include <QFutureInterface>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include "../src/ActionWindow.h"
#include "../src/DebugWindow.h"
#include "../src/MainWindow.h"
#include "../src/NewIssueDialog.h"
#include "../src/NotificationItemWidget.h"
#include "../src/PullRequestWindow.h"
#include "../src/RepoListWindow.h"
#include "../src/SettingsDialog.h"
#include "../src/WalletManager.h"
#include "../src/WorkItemWindow.h"
#include "../src/trending/TrendingWindow.h"
#include "FakeNetworkAccessManager.h"
class MockWalletBackend : public WalletBackend {
   public:
    QFuture<WalletResult> loadTokenAsync() override {
        QFutureInterface<WalletResult> iface;
        iface.reportResult(loadResult);
        iface.reportFinished();
        return iface.future();
    }
    QFuture<WalletResult> saveTokenAsync(const QString& token) override {
        savedToken = token;
        QFutureInterface<WalletResult> iface;
        iface.reportResult(saveResult);
        iface.reportFinished();
        return iface.future();
    }
    QFuture<WalletResult> clearTokenAsync() override {
        savedToken.clear();
        QFutureInterface<WalletResult> iface;
        iface.reportResult({true, "", ""});
        iface.reportFinished();
        return iface.future();
    }

    WalletResult loadResult{true, "test_token", ""};
    WalletResult saveResult{true, "", ""};
    QString savedToken;
};

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
        // window.authNotificationSent = true; replaced by internal incident state handling
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
        QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QFile::remove(appData + "/repos_cache.json");
        QDir dir(appData);
        const QStringList workItemFiles = dir.entryList({"workitems_*.json"}, QDir::Files);
        for (const QString& file : workItemFiles) {
            dir.remove(file);
        }
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
    void testSettingsClearToken() {
        MockWalletBackend* mockBackend = new MockWalletBackend();
        mockBackend->savedToken = "old_token";
        mockBackend->saveResult = {true, "", ""};  // clearTokenAsync uses saveResult
        WalletManager::setBackend(mockBackend);

        SettingsDialog dialog;
        QLineEdit* tokenInput = dialog.findChild<QLineEdit*>();
        tokenInput->setText("");
        dialog.onAccepted();
        QCoreApplication::processEvents();
        QCOMPARE(mockBackend->savedToken, QString(""));

        delete mockBackend;
        WalletManager::setBackend(nullptr);
    }
    void testSettingsSaveFailureKeepsDialog() {
        MockWalletBackend* mockBackend = new MockWalletBackend();
        mockBackend->saveResult = {false, "", "Simulated failure"};
        WalletManager::setBackend(mockBackend);

        SettingsDialog dialog;
        QLineEdit* tokenInput = dialog.findChild<QLineEdit*>();
        tokenInput->setText("new_token");

        dialog.onAccepted();
        QCoreApplication::processEvents();

        QCOMPARE(mockBackend->savedToken, QString("new_token"));

        delete mockBackend;
        WalletManager::setBackend(nullptr);
    }
    void testSettingsSaveSuccessClosesDialog() {
        MockWalletBackend* mockBackend = new MockWalletBackend();
        mockBackend->saveResult = {true, "", ""};
        WalletManager::setBackend(mockBackend);

        SettingsDialog dialog;
        QLineEdit* tokenInput = dialog.findChild<QLineEdit*>();
        tokenInput->setText("new_token");

        dialog.onAccepted();
        QCoreApplication::processEvents();

        QCOMPARE(mockBackend->savedToken, QString("new_token"));

        delete mockBackend;
        WalletManager::setBackend(nullptr);
    }

    void testSettingsVerificationIsolation() {
        SettingsDialog settings;
        settings.tokenEdit->setText("first-token");
        settings.testClient = new GitHubClient(&settings);
        connect(settings.testClient, &GitHubClient::tokenVerified, &settings, &SettingsDialog::onVerificationResult);

        auto* network = new FakeNetworkAccessManager(settings.testClient);
        network->autoEmitFinished = false;
        settings.testClient->manager = network;

        // Request 1: starts verification 1 (/user)
        settings.onTestClicked();

        // Change token and click again
        settings.tokenEdit->setText("second-token");

        // Request 2: starts verification 2 (/user)
        settings.onTestClicked();

        // Let event loop run to ensure requests are registered
        QCoreApplication::processEvents();
        QCOMPARE(network->requests.size(), 2);

        ControlledFakeReply* req1User = network->requests[0].reply;
        ControlledFakeReply* req2User = network->requests[1].reply;

        // Complete Verification 2 successfully (/user)
        req2User->setRawHeader("X-OAuth-Scopes", "repo, notifications");
        req2User->complete("{\"login\":\"new-user\"}");

        // This should trigger Request 2's /user/repos
        QTRY_COMPARE(network->requests.size(), 3);
        ControlledFakeReply* req2Repos = network->requests[2].reply;
        req2Repos->complete("[]");

        // This should trigger Request 2's /notifications
        QTRY_COMPARE(network->requests.size(), 4);
        ControlledFakeReply* req2Notifs = network->requests[3].reply;
        req2Notifs->complete("[]");

        // Now SettingsDialog should process Verification 2 completion
        QTRY_VERIFY(settings.testButton->isEnabled());
        QVERIFY(settings.statusLabel->text().contains("Authentication Successful"));

        // Complete Verification 1 with error, should be ignored
        req1User->completeWithError(QNetworkReply::TimeoutError, "stale timeout");
        QCoreApplication::processEvents();

        // Verification 1 was ignored, so label remains "Authentication Successful"
        QVERIFY(settings.statusLabel->text().contains("Authentication Successful"));
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

    void testPrPaginationMissingLink() {
        GitHubClient client;
        FakeNetworkAccessManager prNetwork;
        prNetwork.autoEmitFinished = false;
        Notification notification;
        notification.url = "https://api.github.com/repos/o/r/pulls/1";
        PullRequestWindow pr(notification, &client, nullptr, &prNetwork);

        QCOMPARE(prNetwork.requests.size(), 1);

        // PR details reply
        QJsonObject details;
        details["issue_url"] = "https://api.github.com/repos/o/r/issues/1";

        prNetwork.requests[0].reply->complete(QJsonDocument(details).toJson());

        // Details reply should spawn 4 collection fetches
        QCOMPARE(prNetwork.requests.size(), 5);

        // Timeline reply (index 1) - Missing Link Header
        QJsonArray timeline;
        QJsonObject event1;
        event1["event"] = "commented";
        event1["user"] = QJsonObject{{"login", "user1"}};
        event1["body"] = "test comment";
        event1["id"] = 100;
        event1["created_at"] = "2024-01-01T12:00:00Z";
        timeline.append(event1);
        prNetwork.requests[1].reply->complete(QJsonDocument(timeline).toJson());
        prNetwork.requests[2].reply->complete("[]");

        QVERIFY(pr.m_timelineState.isComplete);
        QVERIFY(pr.m_timelineState.nextUrl.isEmpty());

        // It shouldn't have launched a new timeline request
        QCOMPARE(prNetwork.requests.size(), 5);
    }

    void testPrPaginationTimeline() {
        GitHubClient client;
        FakeNetworkAccessManager prNetwork;
        prNetwork.autoEmitFinished = false;
        Notification notification;
        notification.url = "https://api.github.com/repos/o/r/pulls/1";
        PullRequestWindow pr(notification, &client, nullptr, &prNetwork);

        QCOMPARE(prNetwork.requests.size(), 1);

        QJsonObject details;
        details["issue_url"] = "https://api.github.com/repos/o/r/issues/1";

        prNetwork.requests[0].reply->complete(QJsonDocument(details).toJson());

        QCOMPARE(prNetwork.requests.size(), 5);  // details + 4 collections

        // Timeline Page 1 (index 1) with Link header
        QJsonArray timelinePg1;
        QJsonObject event1;
        event1["event"] = "commented";
        event1["user"] = QJsonObject{{"login", "user1"}};
        event1["body"] = "page 1 comment";
        event1["id"] = 101;
        event1["created_at"] = "2024-01-01T12:00:00Z";
        timelinePg1.append(event1);

        prNetwork.requests[1].reply->setRawHeader(
            "Link", "<https://api.github.com/repos/o/r/issues/1/timeline?page=2>; rel=\"next\"");
        prNetwork.requests[1].reply->complete(QJsonDocument(timelinePg1).toJson());

        // State should indicate not complete, and spawned a new request
        QVERIFY(!pr.m_timelineState.isComplete);
        QCOMPARE(pr.m_timelineState.nextUrl, QString("https://api.github.com/repos/o/r/issues/1/timeline?page=2"));
        QCOMPARE(prNetwork.requests.size(), 6);  // +1 timeline request
        QCOMPARE(prNetwork.requests[5].request.url().toString(),
                 QString("https://api.github.com/repos/o/r/issues/1/timeline?page=2"));

        // Timeline Page 2 (index 5) - Last page (No Link header)
        QJsonArray timelinePg2;
        QJsonObject event2;
        event2["event"] = "commented";
        event2["user"] = QJsonObject{{"login", "user2"}};
        event2["body"] = "page 2 comment";
        event2["id"] = 102;
        event2["created_at"] = "2024-01-01T12:05:00Z";
        timelinePg2.append(event2);

        prNetwork.requests[5].reply->complete(QJsonDocument(timelinePg2).toJson());

        QVERIFY(pr.m_timelineState.isComplete);
        qDebug() << "Link Header: " << prNetwork.requests[3].reply->hasRawHeader("Link")
                 << prNetwork.requests[3].reply->rawHeader("Link");
        qDebug() << "NextURL:" << pr.m_commitsState.nextUrl;
        QCOMPARE(prNetwork.requests.size(), 6);  // No more requests
    }

    void testPrHostileTimelineInput() {
        GitHubClient client;
        FakeNetworkAccessManager prNetwork;
        prNetwork.autoEmitFinished = false;
        Notification notification;
        notification.url = "https://api.github.com/repos/o/r/pulls/1";
        PullRequestWindow pr(notification, &client, nullptr, &prNetwork);

        QJsonObject details;
        details["issue_url"] = "https://api.github.com/repos/o/r/issues/1";
        prNetwork.requests[0].reply->complete(QJsonDocument(details).toJson());

        QJsonArray timeline;
        QJsonObject event1;
        event1["event"] = "labeled";
        event1["actor"] = QJsonObject{{"login", "<script>alert(1)</script>"}};
        event1["label"] = QJsonObject{{"name", "<b>evil</b>"}};
        event1["id"] = 101;
        event1["created_at"] = "2024-01-01T12:00:00Z";
        timeline.append(event1);

        prNetwork.requests[1].reply->complete(QJsonDocument(timeline).toJson());

        bool foundEscaped = false;
        for (const PREvent& ev : pr.m_events) {
            if (ev.type == PREvent::TimelineEvent) {
                QVERIFY(ev.actionText.contains("&lt;script&gt;"));
                QVERIFY(ev.actionText.contains("&lt;b&gt;evil&lt;/b&gt;"));
                QVERIFY(!ev.actionText.contains("<script>"));
                foundEscaped = true;
            }
        }
        QVERIFY(foundEscaped);
    }

    void testPrStaleReplyRejected() {
        GitHubClient client;
        FakeNetworkAccessManager prNetwork;
        prNetwork.autoEmitFinished = false;
        Notification notification;
        notification.url = "https://api.github.com/repos/o/r/pulls/1";
        PullRequestWindow pr(notification, &client, nullptr, &prNetwork);

        QJsonObject details;
        details["issue_url"] = "https://api.github.com/repos/o/r/issues/1";

        prNetwork.requests[0].reply->complete(QJsonDocument(details).toJson());

        ControlledFakeReply* oldTimelineReply = prNetwork.requests[1].reply;

        pr.fetchPrDetails();

        QCOMPARE(prNetwork.requests.size(), 6);
        prNetwork.requests[5].reply->complete(QJsonDocument(details).toJson());

        for (int i = 0; i < prNetwork.requests.size(); i++) qDebug() << i << prNetwork.requests[i].request.url();
        QCOMPARE(prNetwork.requests.size(), 10);

        QJsonArray timeline;
        QJsonObject event1;
        event1["event"] = "commented";
        event1["user"] = QJsonObject{{"login", "user1"}};
        event1["body"] = "stale comment";
        timeline.append(event1);
        oldTimelineReply->complete(QJsonDocument(timeline).toJson());

        QVERIFY(!pr.m_timelineState.isComplete);
        QVERIFY(pr.m_events.size() <= 1);
    }

    void testPrDeterministicOrdering() {
        GitHubClient client;
        FakeNetworkAccessManager prNetwork;
        prNetwork.autoEmitFinished = false;
        Notification notification;
        notification.url = "https://api.github.com/repos/o/r/pulls/1";
        PullRequestWindow pr(notification, &client, nullptr, &prNetwork);

        QJsonObject details;
        details["issue_url"] = "https://api.github.com/repos/o/r/issues/1";

        details["review_comments_url"] = "https://api.github.com/repos/o/r/pulls/1/comments";
        prNetwork.requests[0].reply->complete(QJsonDocument(details).toJson());

        // Timeline reply (index 1)
        QJsonArray timeline;
        QJsonObject event1;
        event1["event"] = "commented";
        event1["user"] = QJsonObject{{"login", "user1"}};
        event1["body"] = "First timeline comment";
        event1["id"] = 101;
        event1["created_at"] = "2024-01-01T12:00:00Z";
        timeline.append(event1);

        // Review Comments reply (index 2)
        QJsonArray reviews;
        QJsonObject review1;
        review1["user"] = QJsonObject{{"login", "user2"}};
        review1["body"] = "Review comment";
        review1["path"] = "src/main.cpp";
        review1["diff_hunk"] = "@@ -1,1 +1,1 @@";
        review1["id"] = 201;
        review1["created_at"] = "2024-01-01T11:00:00Z";  // Earlier than timeline comment
        reviews.append(review1);

        // We complete them out of order: Review (index 2) then Timeline (index 1)
        prNetwork.requests[2].reply->complete(QJsonDocument(reviews).toJson());
        prNetwork.requests[1].reply->complete(QJsonDocument(timeline).toJson());

        // Check ordering: review1 should be before event1 despite network reply order
        // m_events has size 3 because index 0 is body
        QCOMPARE(pr.m_events.size(), 3);
        QCOMPARE(pr.m_events[1].id, QString("201"));
        QCOMPARE(pr.m_events[2].id, QString("101"));
    }

    void testPrBodyRenderWhilePending() {
        GitHubClient client;
        FakeNetworkAccessManager prNetwork;
        prNetwork.autoEmitFinished = false;
        Notification notification;
        notification.url = "https://api.github.com/repos/o/r/pulls/1";
        PullRequestWindow pr(notification, &client, nullptr, &prNetwork);

        QJsonObject details;
        details["issue_url"] = "https://api.github.com/repos/o/r/issues/1";
        details["body"] = "This is the PR body text.";
        prNetwork.requests[0].reply->complete(QJsonDocument(details).toJson());

        // Wait to process UI updates
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);

        // Timeline and reviews are STILL pending. (requests size is 5)
        QCOMPARE(prNetwork.requests.size(), 5);

        // Assert body widget is visible!
        bool foundBody = false;
        QList<QLabel*> labels = pr.findChildren<QLabel*>();
        for (QLabel* l : labels) {
            if (l && l->text().contains("This is the PR body text.")) {
                foundBody = true;
                break;
            }
        }

        QVERIFY(foundBody);
    }

    void testPrDeduplication() {
        GitHubClient client;
        FakeNetworkAccessManager prNetwork;
        prNetwork.autoEmitFinished = false;
        Notification notification;
        notification.url = "https://api.github.com/repos/o/r/pulls/1";
        PullRequestWindow pr(notification, &client, nullptr, &prNetwork);

        QJsonObject details;
        details["issue_url"] = "https://api.github.com/repos/o/r/issues/1";
        prNetwork.requests[0].reply->complete(QJsonDocument(details).toJson());

        QJsonArray timeline;

        QJsonObject event1;
        event1["event"] = "commented";
        event1["user"] = QJsonObject{{"login", "user1"}};
        event1["body"] = "Same comment";
        event1["id"] = 101;
        event1["created_at"] = "2024-01-01T12:00:00Z";
        timeline.append(event1);

        // Exact same duplicate
        timeline.append(event1);

        // Same ID, different timestamp (should dedupe!)
        QJsonObject event2 = event1;
        event2["created_at"] = "2024-01-01T12:00:01Z";
        timeline.append(event2);

        // Different ID, same text (should NOT dedupe!)
        QJsonObject event3 = event1;
        event3["id"] = 102;
        timeline.append(event3);

        prNetwork.requests[1].reply->complete(QJsonDocument(timeline).toJson());
        prNetwork.requests[2].reply->complete("[]");

        // The exact duplicate and the same-ID duplicate should be filtered out
        // We should have: Body event + event1 + event3
        QCOMPARE(pr.m_events.size(), 3);
    }

    void testPrConversationRetryIndependent() {
        GitHubClient client;
        FakeNetworkAccessManager prNetwork;
        prNetwork.autoEmitFinished = false;
        Notification notification;
        notification.url = "https://api.github.com/repos/o/r/pulls/1";
        PullRequestWindow pr(notification, &client, nullptr, &prNetwork);

        QJsonObject details;
        details["issue_url"] = "https://api.github.com/repos/o/r/issues/1";
        details["comments_url"] = "https://api.github.com/repos/o/r/issues/1/comments";
        details["commits_url"] = "https://api.github.com/repos/o/r/pulls/1/commits";
        details["review_comments_url"] = "https://api.github.com/repos/o/r/pulls/1/comments";
        prNetwork.requests[0].reply->complete(QJsonDocument(details).toJson());

        QCOMPARE(prNetwork.requests.size(), 5);

        // Timeline is loading. Review comments fails.
        prNetwork.requests[2].reply->completeWithError(QNetworkReply::InternalServerError, "Error");

        // Wait to process UI updates
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);

        QVERIFY(pr.m_timelineState.isLoading);
        QVERIFY(pr.m_reviewState.isFailed);

        pr.m_conversationRetryBtn->clicked();

        // The review comments should retry, the timeline should NOT.
        QCOMPARE(prNetwork.requests.size(), 6);
        QVERIFY(prNetwork.requests[5].request.url().toString().contains("/comments"));
    }

    void testPrCollectionStates() {
        GitHubClient client;
        FakeNetworkAccessManager prNetwork;
        prNetwork.autoEmitFinished = false;
        Notification notification;
        notification.url = "https://api.github.com/repos/o/r/pulls/1";
        PullRequestWindow pr(notification, &client, nullptr, &prNetwork);

        QJsonObject details;
        details["issue_url"] = "https://api.github.com/repos/o/r/issues/1";

        prNetwork.requests[0].reply->complete(QJsonDocument(details).toJson());

        // Check initial state (Loading)
        QVERIFY(pr.m_commitsState.isLoading);
        QVERIFY(pr.m_commitsStatusLabel->text().contains("Loading commits"));

        // Commits completes empty
        QJsonArray emptyCommits;
        prNetwork.requests[3].reply->complete(QJsonDocument(emptyCommits).toJson());
        QVERIFY(!pr.m_commitsState.isLoading);
        QVERIFY(pr.m_commitsState.isComplete);
        QVERIFY(pr.m_commitsStatusLabel->text().contains("No commits"));

        // Files completes with data
        QJsonArray files;
        QJsonObject file1;
        file1["filename"] = "test.txt";
        files.append(file1);
        prNetwork.requests[4].reply->complete(QJsonDocument(files).toJson());
        QVERIFY(!pr.m_filesState.isLoading);
        QVERIFY(pr.m_filesState.isComplete);
        QVERIFY(!pr.m_filesStatusLabel->isVisible());
    }

    void testPrLaterPageFailureAndRetry() {
        GitHubClient client;
        FakeNetworkAccessManager prNetwork;
        prNetwork.autoEmitFinished = false;
        Notification notification;
        notification.url = "https://api.github.com/repos/o/r/pulls/1";
        PullRequestWindow pr(notification, &client, nullptr, &prNetwork);

        QJsonObject details;
        details["issue_url"] = "https://api.github.com/repos/o/r/issues/1";

        prNetwork.requests[0].reply->complete(QJsonDocument(details).toJson());

        QCOMPARE(prNetwork.requests.size(), 5);

        QJsonArray commits1;
        QJsonObject commit1;
        commit1["sha"] = "1111111";
        commit1["commit"] = QJsonObject{{"message", "msg"},
                                        {"author", QJsonObject{{"name", "author"}, {"date", "2024-01-01T12:00:00Z"}}}};
        commits1.append(commit1);
        prNetwork.requests[3].reply->setRawHeader(
            "Link", "<https://api.github.com/repos/o/r/pulls/1/commits?page=2>; rel=\"next\"");
        prNetwork.requests[3].reply->complete(QJsonDocument(commits1).toJson());

        QCOMPARE(prNetwork.requests.size(), 6);
        QVERIFY(!pr.m_commitsState.isComplete);

        QCOMPARE(pr.m_commitsTable->rowCount(), 1);

        prNetwork.requests[5].reply->completeWithError(QNetworkReply::InternalServerError, "Server Error", 500);

        QVERIFY(pr.m_commitsState.isFailed);
        QVERIFY(pr.m_commitsStatusLabel->text().contains("Server Error"));
        QVERIFY(!pr.m_commitsRetryBtn->isHidden());

        QCOMPARE(pr.m_commitsTable->rowCount(), 1);

        pr.m_commitsRetryBtn->click();

        QCOMPARE(prNetwork.requests.size(), 7);
        QCOMPARE(prNetwork.requests[6].request.url().toString(),
                 QString("https://api.github.com/repos/o/r/pulls/1/commits?page=2"));

        QJsonArray commits2;
        QJsonObject commit2;
        commit2["sha"] = "2222222";
        commit2["commit"] = QJsonObject{{"message", "msg2"},
                                        {"author", QJsonObject{{"name", "author"}, {"date", "2024-01-01T12:00:00Z"}}}};
        commits2.append(commit2);
        prNetwork.requests[6].reply->complete(QJsonDocument(commits2).toJson());

        QVERIFY(pr.m_commitsState.isComplete);
        QVERIFY(!pr.m_commitsState.isFailed);
        QCOMPARE(pr.m_commitsTable->rowCount(), 2);
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

    void testMarkAsReadSuccess() {
        GitHubClient client;
        auto* network = installNetwork(client);
        MainWindow window;
        prepareMain(window, client);

        window.onRefreshClicked();
        network->requests[0].reply->complete(
            "[{\"id\":\"1\",\"unread\":true,\"subject\":{\"title\":\"Test\"},\"repository\":{\"full_name\":\"test/"
            "repo\"}}]");

        auto* list = window.notificationListWidget;
        QCOMPARE(list->count(), 1);
        QCOMPARE(list->getUnreadNotifications().size(), 1);

        list->requestMarkAsRead("1");
        QCOMPARE(network->requests.size(), 2);
        QCOMPARE(list->m_pendingMutations.size(), 1);

        auto* widget = qobject_cast<NotificationItemWidget*>(list->listWidget->itemWidget(list->listWidget->item(0)));
        QVERIFY(widget != nullptr);
        QVERIFY(widget->isLoading());

        network->requests[1].reply->complete("{}");

        // Authoritative model updated
        QCOMPARE(list->m_allNotifications.size(), 1);
        QCOMPARE(list->m_allNotifications[0].unread, false);
        QCOMPARE(list->getUnreadNotifications().size(), 0);
        QVERIFY(list->m_pendingMutations.isEmpty());

        // Under default filter mode (unread only), row is removed
        QCOMPARE(list->count(), 0);

        // Switch to "All" filter mode (4) to verify widget state
        list->setFilterMode(4);
        QCOMPARE(list->count(), 1);
        auto* allWidget =
            qobject_cast<NotificationItemWidget*>(list->listWidget->itemWidget(list->listWidget->item(0)));
        QVERIFY(allWidget != nullptr);
        QVERIFY(!allWidget->isLoading());
        QVERIFY(!allWidget->unreadIndicator->isVisible());
        QVERIFY(allWidget->doneButton->isEnabled());
    }

    void testMarkAsReadFailure() {
        GitHubClient client;
        auto* network = installNetwork(client);
        MainWindow window;
        prepareMain(window, client);

        window.onRefreshClicked();
        network->requests[0].reply->complete(
            "[{\"id\":\"1\",\"unread\":true,\"subject\":{\"title\":\"Test\"},\"repository\":{\"full_name\":\"test/"
            "repo\"}}]");

        auto* list = window.notificationListWidget;
        list->requestMarkAsRead("1");

        auto* widget = qobject_cast<NotificationItemWidget*>(list->listWidget->itemWidget(list->listWidget->item(0)));
        QVERIFY(widget != nullptr);
        QVERIFY(widget->isLoading());

        network->requests[1].reply->completeWithError(QNetworkReply::InternalServerError, "Read failed");

        // Authoritative model remains unchanged
        QCOMPARE(list->m_allNotifications.size(), 1);
        QCOMPARE(list->m_allNotifications[0].unread, true);
        QCOMPARE(list->getUnreadNotifications().size(), 1);
        QCOMPARE(list->count(), 1);
        QVERIFY(list->m_pendingMutations.isEmpty());

        // Widget busy state cleared, error shown, actionable
        QVERIFY(!widget->isLoading());
        QVERIFY(widget->errorLabel->isVisible() || !widget->errorLabel->text().isEmpty());
        QVERIFY(widget->errorLabel->text().contains("Read failed"));
        QVERIFY(widget->doneButton->isEnabled());
    }

    void testDoneSuccess() {
        GitHubClient client;
        auto* network = installNetwork(client);
        MainWindow window;
        prepareMain(window, client);

        window.onRefreshClicked();
        network->requests[0].reply->complete(
            "[{\"id\":\"1\",\"unread\":true,\"subject\":{\"title\":\"Test\"},\"repository\":{\"full_name\":\"test/"
            "repo\"}}]");

        auto* list = window.notificationListWidget;
        QCOMPARE(list->count(), 1);
        QVERIFY(list->knownNotificationIds.contains("1"));

        list->requestMarkAsDone("1");
        QCOMPARE(network->requests.size(), 2);
        auto* widget = qobject_cast<NotificationItemWidget*>(list->listWidget->itemWidget(list->listWidget->item(0)));
        QVERIFY(widget != nullptr);
        QVERIFY(widget->isLoading());

        network->requests[1].reply->complete("{}");

        // Authoritative model and known IDs cleaned up
        QVERIFY(list->m_allNotifications.isEmpty());
        QVERIFY(list->getUnreadNotifications().isEmpty());
        QCOMPARE(list->count(), 0);
        QVERIFY(!list->knownNotificationIds.contains("1"));
        QVERIFY(list->m_pendingMutations.isEmpty());
    }

    void testMarkAsDoneFailure() {
        GitHubClient client;
        auto* network = installNetwork(client);
        MainWindow window;
        prepareMain(window, client);

        window.onRefreshClicked();
        network->requests[0].reply->complete(
            "[{\"id\":\"1\",\"unread\":true,\"subject\":{\"title\":\"Test\"},\"repository\":{\"full_name\":\"test/"
            "repo\"}}]");

        auto* list = window.notificationListWidget;
        list->requestMarkAsDone("1");

        auto* widget = qobject_cast<NotificationItemWidget*>(list->listWidget->itemWidget(list->listWidget->item(0)));
        QVERIFY(widget != nullptr);
        QVERIFY(widget->isLoading());

        network->requests[1].reply->completeWithError(QNetworkReply::InternalServerError, "Server Error");

        QCOMPARE(list->count(), 1);
        QCOMPARE(list->m_allNotifications.size(), 1);
        QCOMPARE(list->m_allNotifications[0].unread, true);
        QVERIFY(list->knownNotificationIds.contains("1"));
        QVERIFY(list->m_pendingMutations.isEmpty());

        QVERIFY(!widget->isLoading());
        QVERIFY(widget->errorLabel->isVisible() || !widget->errorLabel->text().isEmpty());
        QVERIFY(widget->errorLabel->text().contains("1"));
        QVERIFY(widget->errorLabel->text().contains("done"));
        QVERIFY(widget->errorLabel->text().contains("Server Error"));
        QVERIFY(widget->doneButton->isEnabled());
    }

    void testComposedReadDonePartialFailure() {
        GitHubClient client;
        auto* network = installNetwork(client);
        MainWindow window;
        prepareMain(window, client);

        window.onRefreshClicked();
        network->requests[0].reply->complete(
            "[{\"id\":\"1\",\"unread\":true,\"subject\":{\"title\":\"Test\"},\"repository\":{\"full_name\":\"test/"
            "repo\"}}]");

        auto* list = window.notificationListWidget;
        QCOMPARE(list->count(), 1);
        QCOMPARE(list->getUnreadNotifications().size(), 1);

        // 1. Start composed read+done through NotificationListWidget
        list->requestMarkAsReadAndDone("1");
        QCOMPARE(network->requests.size(), 2);
        QCOMPARE(network->requests[1].reply->property("type").toString(), QString("read_and_done_stage1"));

        auto* widget = qobject_cast<NotificationItemWidget*>(list->listWidget->itemWidget(list->listWidget->item(0)));
        QVERIFY(widget != nullptr);
        QVERIFY(widget->isLoading());

        // 2. PATCH succeeds
        network->requests[1].reply->complete("{}");

        // 3. GitHubClient starts DELETE under the same logical request identity
        QCOMPARE(network->requests.size(), 3);
        QCOMPARE(network->requests[2].reply->property("type").toString(), QString("read_and_done_stage2"));
        QUuid reqId1 = network->requests[1].reply->property("reqId").toUuid();
        QUuid reqId2 = network->requests[2].reply->property("reqId").toUuid();
        QCOMPARE(reqId1, reqId2);

        // 4. DELETE fails
        network->requests[2].reply->completeWithError(QNetworkReply::InternalServerError, "Delete failed");

        // Required final state:
        // - notification remains in m_allNotifications
        QCOMPARE(list->m_allNotifications.size(), 1);
        QCOMPARE(list->m_allNotifications[0].id, QString("1"));
        // - unread == false (successful read committed and not rolled back)
        QCOMPARE(list->m_allNotifications[0].unread, false);
        // - it is NOT removed as Done
        QVERIFY(list->knownNotificationIds.contains("1"));
        // - unread/tray counts reflect the successful read
        QCOMPARE(list->getUnreadNotifications().size(), 0);
        // - unread-only presentation does not show it
        QCOMPARE(list->count(), 0);
        // - pending/busy state is cleared
        QVERIFY(list->m_pendingMutations.isEmpty());

        // Switch to "All" mode to verify visible state and actionability
        list->setFilterMode(4);
        QCOMPARE(list->count(), 1);
        auto* allWidget =
            qobject_cast<NotificationItemWidget*>(list->listWidget->itemWidget(list->listWidget->item(0)));
        QVERIFY(allWidget != nullptr);
        QVERIFY(!allWidget->isLoading());
        QVERIFY(!allWidget->unreadIndicator->isVisible());
        // - Done failure is understandable
        QVERIFY(allWidget->errorLabel->isVisible() || !allWidget->errorLabel->text().isEmpty());
        QVERIFY(allWidget->errorLabel->text().contains("1"));
        QVERIFY(allWidget->errorLabel->text().contains("read+done"));
        QVERIFY(allWidget->errorLabel->text().contains("Delete failed"));
        // - notification remains actionable
        QVERIFY(allWidget->doneButton->isEnabled());
    }

    void testBatchDismissalPartialFailure() {
        GitHubClient client;
        auto* network = installNetwork(client);
        MainWindow window;
        prepareMain(window, client);

        window.onRefreshClicked();
        network->requests[0].reply->complete(
            "[{\"id\":\"1\",\"unread\":true,\"subject\":{\"title\":\"Test1\"},\"repository\":{\"full_name\":\"repo\"}},"
            "{\"id\":\"2\",\"unread\":true,\"subject\":{\"title\":\"Test2\"},\"repository\":{\"full_name\":\"repo\"}}"
            "]");

        auto* list = window.notificationListWidget;
        QCOMPARE(list->count(), 2);
        QCOMPARE(list->getUnreadNotifications().size(), 2);

        window.dismissAllNotifications();
        QCOMPARE(network->requests.size(), 3);  // refresh, done1, done2

        network->requests[1].reply->complete("{}");
        network->requests[2].reply->completeWithError(QNetworkReply::InternalServerError, "Failed");

        // Authoritative model: successful ID 1 removed, failed ID 2 remains
        QCOMPARE(list->m_allNotifications.size(), 1);
        QCOMPARE(list->m_allNotifications[0].id, QString("2"));
        QCOMPARE(list->m_allNotifications[0].unread, true);
        QVERIFY(!list->knownNotificationIds.contains("1"));
        QVERIFY(list->knownNotificationIds.contains("2"));
        QVERIFY(list->m_pendingMutations.isEmpty());

        // Visible list
        QCOMPARE(list->count(), 1);
        auto* widget = qobject_cast<NotificationItemWidget*>(list->listWidget->itemWidget(list->listWidget->item(0)));
        QVERIFY(widget != nullptr);
        QCOMPARE(widget->getTitle(), QString("Test2"));
        QVERIFY(!widget->isLoading());
        QVERIFY(widget->errorLabel->isVisible() || !widget->errorLabel->text().isEmpty());
        QVERIFY(widget->errorLabel->text().contains("2"));
        QVERIFY(widget->errorLabel->text().contains("done"));
        QVERIFY(widget->errorLabel->text().contains("Failed"));
        QVERIFY(widget->doneButton->isEnabled());
    }

    void testUnreadOnlyFilterDuringMutation() {
        GitHubClient client;
        auto* network = installNetwork(client);
        MainWindow window;
        prepareMain(window, client);

        window.onRefreshClicked();
        network->requests[0].reply->complete(
            "[{\"id\":\"1\",\"unread\":true,\"subject\":{\"title\":\"Test\"},\"repository\":{\"full_name\":\"repo\"}}"
            "]");

        auto* list = window.notificationListWidget;
        // Default filter is mode 0 (All Unread)
        QCOMPARE(list->m_filterMode, 0);
        QCOMPARE(list->count(), 1);

        list->requestMarkAsRead("1");
        auto* widget = qobject_cast<NotificationItemWidget*>(list->listWidget->itemWidget(list->listWidget->item(0)));
        QVERIFY(widget != nullptr);
        QVERIFY(widget->isLoading());

        network->requests[1].reply->complete("{}");

        // Because it was marked read, it is filtered out of unread-only presentation
        QCOMPARE(list->count(), 0);
        QCOMPARE(list->m_allNotifications.size(), 1);
        QCOMPARE(list->m_allNotifications[0].unread, false);
        QCOMPARE(list->getUnreadNotifications().size(), 0);

        // Switching to "All" (mode 4) shows it with updated read status
        list->setFilterMode(4);
        QCOMPARE(list->count(), 1);
        auto* widgetAll =
            qobject_cast<NotificationItemWidget*>(list->listWidget->itemWidget(list->listWidget->item(0)));
        QVERIFY(widgetAll != nullptr);
        QVERIFY(!widgetAll->isLoading());
        QVERIFY(!widgetAll->unreadIndicator->isVisible());
    }

    void testRepoFilterTrayDismissAll() {
        GitHubClient client;
        auto* network = installNetwork(client);
        MainWindow window;
        prepareMain(window, client);

        window.onRefreshClicked();
        network->requests[0].reply->complete(
            "[{\"id\":\"1\",\"unread\":true,\"subject\":{\"title\":\"Test1\"},\"repository\":{\"full_name\":\"repoA\"}}"
            ","
            "{\"id\":\"2\",\"unread\":true,\"subject\":{\"title\":\"Test2\"},\"repository\":{\"full_name\":\"repoB\"}}"
            "]");

        auto* list = window.notificationListWidget;
        QCOMPARE(list->count(), 2);

        // Filter main window list to repoA
        list->setRepoFilter("repoA");
        QVERIFY(!list->listWidget->item(0)->isHidden());
        QVERIFY(list->listWidget->item(1)->isHidden());

        // Tray Dismiss All must operate on all loaded unread items (both repoA and repoB)
        window.dismissAllNotifications();
        QCOMPARE(network->requests.size(), 3);  // refresh, done1, done2

        network->requests[1].reply->complete("{}");
        network->requests[2].reply->complete("{}");

        // Both items authoritatively cleaned up even though repoB was hidden by filter
        QVERIFY(list->m_allNotifications.isEmpty());
        QVERIFY(list->getUnreadNotifications().isEmpty());
        QCOMPARE(list->count(), 0);
        QVERIFY(!list->knownNotificationIds.contains("1"));
        QVERIFY(!list->knownNotificationIds.contains("2"));
    }

    void testSearchFilterTrayDismissAll() {
        GitHubClient client;
        auto* network = installNetwork(client);
        MainWindow window;
        prepareMain(window, client);

        window.onRefreshClicked();
        network->requests[0].reply->complete(
            "[{\"id\":\"1\",\"unread\":true,\"subject\":{\"title\":\"Apple\"},\"repository\":{\"full_name\":\"repo\"}},"
            "{\"id\":\"2\",\"unread\":true,\"subject\":{\"title\":\"Banana\"},\"repository\":{\"full_name\":\"repo\"}}"
            "]");

        auto* list = window.notificationListWidget;
        QCOMPARE(list->count(), 2);

        // Search filter hiding "Banana"
        list->setSearchFilter("Apple");
        QVERIFY(!list->listWidget->item(0)->isHidden());
        QVERIFY(list->listWidget->item(1)->isHidden());

        // Tray Dismiss All must target all unread regardless of search filter
        window.dismissAllNotifications();
        QCOMPARE(network->requests.size(), 3);  // refresh, done1, done2

        network->requests[1].reply->complete("{}");
        network->requests[2].reply->complete("{}");

        // Both items authoritatively removed
        QVERIFY(list->m_allNotifications.isEmpty());
        QVERIFY(list->getUnreadNotifications().isEmpty());
        QCOMPARE(list->count(), 0);
        QVERIFY(!list->knownNotificationIds.contains("1"));
        QVERIFY(!list->knownNotificationIds.contains("2"));
    }

    void testTrayDismissAllPreservesSelection() {
        GitHubClient client;
        auto* network = installNetwork(client);
        MainWindow window;
        prepareMain(window, client);

        window.onRefreshClicked();
        network->requests[0].reply->complete(
            "[{\"id\":\"1\",\"unread\":true,\"subject\":{\"title\":\"Test1\"},\"repository\":{\"full_name\":\"repo\"}},"
            "{\"id\":\"2\",\"unread\":true,\"subject\":{\"title\":\"Test2\"},\"repository\":{\"full_name\":\"repo\"}}"
            "]");

        auto* list = window.notificationListWidget;
        list->setFilterMode(4);  // All mode so rows stay in view
        list->listWidget->item(1)->setSelected(true);
        QVERIFY(list->listWidget->item(1)->isSelected());
        QVERIFY(!list->listWidget->item(0)->isSelected());

        // Tray Dismiss All must NOT change selection (must not call selectAll)
        window.dismissAllNotifications();
        QVERIFY(list->listWidget->item(1)->isSelected());
        QVERIFY(!list->listWidget->item(0)->isSelected());
    }

    void testPendingDuplicateAction() {
        GitHubClient client;
        auto* network = installNetwork(client);
        MainWindow window;
        prepareMain(window, client);

        window.onRefreshClicked();
        network->requests[0].reply->complete(
            "[{\"id\":\"1\",\"unread\":true,\"subject\":{\"title\":\"Test\"},\"repository\":{\"full_name\":\"repo\"}}"
            "]");

        auto* list = window.notificationListWidget;
        list->requestMarkAsRead("1");
        QCOMPARE(network->requests.size(), 2);  // refresh, read

        // Prevent any second mutation for the same notification while one mutation is pending:
        // Same action
        list->requestMarkAsRead("1");
        QCOMPARE(network->requests.size(), 2);

        // Different action (done)
        list->requestMarkAsDone("1");
        QCOMPARE(network->requests.size(), 2);

        // Different action (read_and_done)
        list->requestMarkAsReadAndDone("1");
        QCOMPARE(network->requests.size(), 2);

        // Complete the pending request
        network->requests[1].reply->complete("{}");
        QVERIFY(list->m_pendingMutations.isEmpty());
    }

    void testSynchronousErrorRace() {
        GitHubClient client;
        auto* network = installNetwork(client);
        MainWindow window;
        prepareMain(window, client);

        window.onRefreshClicked();
        network->requests[0].reply->complete(
            "[{\"id\":\"1\",\"unread\":true,\"subject\":{\"title\":\"Test\"},\"repository\":{\"full_name\":\"repo\"},"
            "\"groupedNotifications\":[{\"id\":\"c1\",\"unread\":true,\"title\":\"Child\",\"type\":\"Issue\"}]}"
            "]");

        auto* list = window.notificationListWidget;
        auto* widget = qobject_cast<NotificationItemWidget*>(list->listWidget->itemWidget(list->listWidget->item(0)));
        QVERIFY(widget != nullptr);

        // Client will synchronously emit errorOccurred when m_token is empty
        client.setToken("");

        list->requestMarkAsRead("1");

        // Assert:
        // - no stranded entry remains in m_pendingMutations
        QVERIFY(list->m_pendingMutations.isEmpty());
        // - notification remains in authoritative model
        QCOMPARE(list->m_allNotifications.size(), 1);
        // - read/done state is unchanged
        QCOMPARE(list->m_allNotifications[0].unread, true);
        // - busy state is cleared
        QVERIFY(!widget->isLoading());
        // - mutation controls become actionable again
        QVERIFY(widget->doneButton->isEnabled());
        // - understandable error is shown
        QVERIFY(widget->errorLabel->isVisible() || !widget->errorLabel->text().isEmpty());
        QVERIFY(widget->errorLabel->text().contains("No token provided"));

        // Now test synchronous failure on child action
        list->requestChildMarkAsRead("1", "c1");
        QVERIFY(list->m_pendingMutations.isEmpty());
        QCOMPARE(list->m_allNotifications[0].groupedNotifications[0].unread, true);
    }

    void testRefreshDuringMutation() {
        GitHubClient client;
        auto* network = installNetwork(client);
        MainWindow window;
        prepareMain(window, client);

        window.onRefreshClicked();
        network->requests[0].reply->complete(
            "[{\"id\":\"1\",\"unread\":true,\"subject\":{\"title\":\"Test\"},\"repository\":{\"full_name\":\"repo\"}}"
            "]");

        auto* list = window.notificationListWidget;
        list->requestMarkAsRead("1");
        QCOMPARE(network->requests.size(), 2);
        QCOMPARE(list->m_pendingMutations.size(), 1);

        // While mutation is in flight, trigger a refresh
        window.onRefreshClicked();
        QCOMPARE(network->requests.size(), 3);

        // Server responds to refresh with item "1" still unread
        network->requests[2].reply->complete(
            "[{\"id\":\"1\",\"unread\":true,\"subject\":{\"title\":\"Test\"},\"repository\":{\"full_name\":\"repo\"}}"
            "]");

        // The refreshed/recreated item MUST remain pending/busy until the mutation resolves
        auto* refreshedWidget =
            qobject_cast<NotificationItemWidget*>(list->listWidget->itemWidget(list->listWidget->item(0)));
        QVERIFY(refreshedWidget != nullptr);
        QVERIFY(refreshedWidget->isLoading());
        QVERIFY(!refreshedWidget->doneButton->isEnabled());

        // Duplicate mutation must be rejected while still pending
        list->requestMarkAsDone("1");
        QCOMPARE(network->requests.size(), 3);

        // Resolve in-flight read mutation
        network->requests[1].reply->complete("{}");

        // Authoritative model updated, row removed from unread list
        QCOMPARE(list->count(), 0);
        QCOMPARE(list->m_allNotifications.size(), 1);
        QCOMPARE(list->m_allNotifications[0].unread, false);
        QVERIFY(list->m_pendingMutations.isEmpty());
    }

    void testPartiallyLoadedPaginatedTrayScope() {
        GitHubClient client;
        auto* network = installNetwork(client);
        MainWindow window;
        prepareMain(window, client);

        window.onRefreshClicked();
        auto* reply = network->requests[0].reply;
        reply->setRawHeader("Link", "<https://api.github.com/notifications?page=2>; rel=\"next\"");
        reply->complete(
            "[{\"id\":\"1\",\"unread\":true,\"subject\":{\"title\":\"Test\"},\"repository\":{\"full_name\":\"repo\"}}"
            "]");

        auto* list = window.notificationListWidget;
        QVERIFY(list->hasMore());
        QCOMPARE(list->getUnreadNotifications(-1).size(), 1);

        // Verify Dismiss All operates strictly on loaded unread notifications
        window.dismissAllNotifications();
        QCOMPARE(network->requests.size(), 2);  // refresh, done for loaded item 1
        QCOMPARE(network->requests[1].reply->property("type").toString(), QString("delete"));

        network->requests[1].reply->complete("{}");
        QVERIFY(list->m_allNotifications.isEmpty());
        QVERIFY(list->getUnreadNotifications().isEmpty());
    }

    void testChildActionsSuccess() {
        GitHubClient client;
        auto* network = installNetwork(client);
        MainWindow window;
        prepareMain(window, client);

        window.onRefreshClicked();
        network->requests[0].reply->complete(
            "[{\"id\":\"p1\",\"unread\":true,\"subject\":{\"title\":\"Parent\"},\"repository\":{\"full_name\":\"repo\"}"
            ","
            "\"groupedNotifications\":["
            "{\"id\":\"c1\",\"unread\":true,\"title\":\"Child1\",\"type\":\"Issue\"},"
            "{\"id\":\"c2\",\"unread\":true,\"title\":\"Child2\",\"type\":\"Issue\"}"
            "]}]");

        auto* list = window.notificationListWidget;
        list->setFilterMode(4);  // All mode so parent stays visible
        auto* widget = qobject_cast<NotificationItemWidget*>(list->listWidget->itemWidget(list->listWidget->item(0)));
        QVERIFY(widget != nullptr);

        // 1. Mark child 1 as read
        list->requestChildMarkAsRead("p1", "c1");
        QCOMPARE(network->requests.size(), 2);
        network->requests[1].reply->complete("{}");

        // Authoritative model updated
        QCOMPARE(list->m_allNotifications[0].groupedNotifications[0].unread, false);
        // Child 1 read button is hidden
        auto* c1Btn = widget->findChild<QToolButton*>("c1_readBtn");
        QVERIFY(!c1Btn || !c1Btn->isVisible());

        // 2. Mark child 2 as done
        list->requestChildMarkAsDone("p1", "c2");
        QCOMPARE(network->requests.last().reply->property("type").toString(), QString("delete"));
        network->requests.last().reply->complete("{}");

        // Authoritative model updated (c2 removed)
        QCOMPARE(list->m_allNotifications[0].groupedNotifications.size(), 1);
        QCOMPARE(list->m_allNotifications[0].groupedNotifications[0].id, QString("c1"));
        // Child 2 widget removed from UI
        QVERIFY(widget->findChild<QToolButton*>("c2_doneBtn") == nullptr);
    }

    void testMutationAuthError() {
        GitHubClient client;
        auto* network = installNetwork(client);
        MainWindow window;
        prepareMain(window, client);

        QSignalSpy authSpy(&client, &GitHubClient::authError);

        window.onRefreshClicked();
        network->requests[0].reply->complete(
            "[{\"id\":\"1\",\"unread\":true,\"subject\":{\"title\":\"Test\"},\"repository\":{\"full_name\":\"test/"
            "repo\"}}]");

        auto* list = window.notificationListWidget;
        QCOMPARE(list->count(), 1);

        // 1. Mark as Read 401 failure
        list->requestMarkAsRead("1");
        auto* widget = qobject_cast<NotificationItemWidget*>(list->listWidget->itemWidget(list->listWidget->item(0)));
        QVERIFY(widget != nullptr);
        QVERIFY(widget->isLoading());

        network->requests[1].reply->completeWithError(QNetworkReply::AuthenticationRequiredError, "Unauthorized", 401);

        QCOMPARE(authSpy.count(), 1);
        QCOMPARE(list->m_allNotifications.size(), 1);
        QCOMPARE(list->m_allNotifications[0].unread, true);
        QVERIFY(list->m_pendingMutations.isEmpty());
        QVERIFY(!widget->isLoading());
        QVERIFY(widget->errorLabel->isVisible() || !widget->errorLabel->text().isEmpty());
        QVERIFY(widget->errorLabel->text().contains("1"));
        QVERIFY(widget->errorLabel->text().contains("read"));
        QVERIFY(widget->errorLabel->text().contains("Invalid Token"));
        QVERIFY(widget->doneButton->isEnabled());

        // 2. Mark as Done 401 failure
        list->requestMarkAsDone("1");
        QVERIFY(widget->isLoading());

        network->requests[2].reply->completeWithError(QNetworkReply::AuthenticationRequiredError, "Unauthorized", 401);

        QCOMPARE(authSpy.count(), 2);
        QCOMPARE(list->m_allNotifications.size(), 1);
        QCOMPARE(list->m_allNotifications[0].unread, true);
        QVERIFY(list->knownNotificationIds.contains("1"));
        QVERIFY(list->m_pendingMutations.isEmpty());
        QVERIFY(!widget->isLoading());
        QVERIFY(widget->errorLabel->isVisible() || !widget->errorLabel->text().isEmpty());
        QVERIFY(widget->errorLabel->text().contains("1"));
        QVERIFY(widget->errorLabel->text().contains("done"));
        QVERIFY(widget->errorLabel->text().contains("Invalid Token"));
        QVERIFY(widget->doneButton->isEnabled());
    }

    void testComposedReadDoneStage2AuthError() {
        GitHubClient client;
        auto* network = installNetwork(client);
        MainWindow window;
        prepareMain(window, client);

        QSignalSpy authSpy(&client, &GitHubClient::authError);

        window.onRefreshClicked();
        network->requests[0].reply->complete(
            "[{\"id\":\"1\",\"unread\":true,\"subject\":{\"title\":\"Test\"},\"repository\":{\"full_name\":\"test/"
            "repo\"}}]");

        auto* list = window.notificationListWidget;
        QCOMPARE(list->count(), 1);
        QCOMPARE(list->getUnreadNotifications().size(), 1);

        // 1. Start composed read+done
        list->requestMarkAsReadAndDone("1");
        QCOMPARE(network->requests.size(), 2);
        QCOMPARE(network->requests[1].reply->property("type").toString(), QString("read_and_done_stage1"));

        auto* widget = qobject_cast<NotificationItemWidget*>(list->listWidget->itemWidget(list->listWidget->item(0)));
        QVERIFY(widget != nullptr);
        QVERIFY(widget->isLoading());

        // 2. PATCH succeeds
        network->requests[1].reply->complete("{}");

        // 3. Stage 2 DELETE is initiated
        QCOMPARE(network->requests.size(), 3);
        QCOMPARE(network->requests[2].reply->property("type").toString(), QString("read_and_done_stage2"));

        // 4. DELETE returns HTTP 401
        network->requests[2].reply->completeWithError(QNetworkReply::AuthenticationRequiredError, "Unauthorized", 401);

        // - routes through authError
        QCOMPARE(authSpy.count(), 1);

        // - read stage committed (unread == false)
        QCOMPARE(list->m_allNotifications.size(), 1);
        QCOMPARE(list->m_allNotifications[0].id, QString("1"));
        QCOMPARE(list->m_allNotifications[0].unread, false);
        QCOMPARE(list->getUnreadNotifications().size(), 0);

        // - NOT removed as Done
        QVERIFY(list->knownNotificationIds.contains("1"));

        // - pending state cleared
        QVERIFY(list->m_pendingMutations.isEmpty());

        // - unread-only view does not show it
        QCOMPARE(list->count(), 0);

        // Switch to "All" mode to verify visible state, actionable restoration, and contextual failure
        list->setFilterMode(4);
        QCOMPARE(list->count(), 1);
        auto* allWidget =
            qobject_cast<NotificationItemWidget*>(list->listWidget->itemWidget(list->listWidget->item(0)));
        QVERIFY(allWidget != nullptr);
        QVERIFY(!allWidget->isLoading());
        QVERIFY(allWidget->doneButton->isEnabled());
        QVERIFY(allWidget->errorLabel->isVisible() || !allWidget->errorLabel->text().isEmpty());
        QVERIFY(allWidget->errorLabel->text().contains("1"));
        QVERIFY(allWidget->errorLabel->text().contains("read+done"));
        QVERIFY(allWidget->errorLabel->text().contains("Invalid Token"));
    }

    void testChildActionFailure() {
        GitHubClient client;
        auto* network = installNetwork(client);
        MainWindow window;
        prepareMain(window, client);

        QSignalSpy authSpy(&client, &GitHubClient::authError);

        window.onRefreshClicked();
        network->requests[0].reply->complete(
            "[{\"id\":\"p1\",\"unread\":true,\"subject\":{\"title\":\"Parent\"},\"repository\":{\"full_name\":\"repo\"}"
            ","
            "\"groupedNotifications\":["
            "{\"id\":\"c1\",\"unread\":true,\"title\":\"Child1\",\"type\":\"Issue\"}"
            "]}]");

        auto* list = window.notificationListWidget;
        list->setFilterMode(4);  // All mode so parent stays visible
        auto* widget = qobject_cast<NotificationItemWidget*>(list->listWidget->itemWidget(list->listWidget->item(0)));
        QVERIFY(widget != nullptr);

        auto* c1ReadBtn = widget->findChild<QToolButton*>("c1_readBtn");
        auto* c1DoneBtn = widget->findChild<QToolButton*>("c1_doneBtn");
        QVERIFY(c1ReadBtn != nullptr);
        QVERIFY(c1DoneBtn != nullptr);

        // 1. Mark child 1 as read, fails with HTTP 401
        list->requestChildMarkAsRead("p1", "c1");
        QCOMPARE(network->requests.size(), 2);
        QVERIFY(!c1ReadBtn->isEnabled());
        QVERIFY(!c1DoneBtn->isEnabled());

        network->requests[1].reply->completeWithError(QNetworkReply::AuthenticationRequiredError, "Unauthorized", 401);

        QCOMPARE(authSpy.count(), 1);
        QVERIFY(list->m_pendingMutations.isEmpty());
        // Model unread preserved
        QCOMPARE(list->m_allNotifications[0].groupedNotifications[0].unread, true);
        // Actionable restoration
        QVERIFY(c1ReadBtn->isEnabled());
        QVERIFY(c1DoneBtn->isEnabled());
        // Visible contextual failure
        QVERIFY(widget->errorLabel->isVisible() || !widget->errorLabel->text().isEmpty());
        QVERIFY(widget->errorLabel->text().contains("c1"));
        QVERIFY(widget->errorLabel->text().contains("read"));
        QVERIFY(widget->errorLabel->text().contains("Invalid Token"));

        // 2. Mark child 1 as done, fails with 500 InternalServerError
        list->requestChildMarkAsDone("p1", "c1");
        QCOMPARE(network->requests.size(), 3);
        QVERIFY(!c1ReadBtn->isEnabled());
        QVERIFY(!c1DoneBtn->isEnabled());

        network->requests[2].reply->completeWithError(QNetworkReply::InternalServerError, "Server Error");

        QVERIFY(list->m_pendingMutations.isEmpty());
        // Child not removed from model
        QCOMPARE(list->m_allNotifications[0].groupedNotifications.size(), 1);
        // Actionable restoration
        QVERIFY(c1ReadBtn->isEnabled());
        QVERIFY(c1DoneBtn->isEnabled());
        // Visible contextual failure
        QVERIFY(widget->errorLabel->isVisible() || !widget->errorLabel->text().isEmpty());
        QVERIFY(widget->errorLabel->text().contains("c1"));
        QVERIFY(widget->errorLabel->text().contains("done"));
        QVERIFY(widget->errorLabel->text().contains("Server Error"));
    }

    void testDetailsErrorDoesNotPersistAsMutationError() {
        GitHubClient client;
        auto* network = installNetwork(client);
        MainWindow window;
        prepareMain(window, client);

        window.onRefreshClicked();
        network->requests[0].reply->complete(
            "[{\"id\":\"1\",\"unread\":true,\"subject\":{\"title\":\"Test\"},\"repository\":{\"full_name\":\"repo\"}}"
            "]");

        auto* list = window.notificationListWidget;
        QCOMPARE(list->count(), 1);
        auto* widget = qobject_cast<NotificationItemWidget*>(list->listWidget->itemWidget(list->listWidget->item(0)));
        QVERIFY(widget != nullptr);

        // Details error arrives
        list->updateError("1", "404 Not Found");
        QVERIFY(widget->errorLabel->isVisible() || !widget->errorLabel->text().isEmpty());
        QVERIFY(widget->errorLabel->text().contains("404 Not Found"));

        // Crucial: Details error must NOT enter m_mutationErrors
        QVERIFY(list->m_mutationErrors.isEmpty());

        // Rebuilding / filtering the list must not restore the transient details error
        list->setFilterMode(4);
        QCOMPARE(list->count(), 1);
        auto* rebuiltWidget =
            qobject_cast<NotificationItemWidget*>(list->listWidget->itemWidget(list->listWidget->item(0)));
        QVERIFY(rebuiltWidget != nullptr);
        QVERIFY(!rebuiltWidget->errorLabel->isVisible());
        QVERIFY(rebuiltWidget->errorLabel->text().isEmpty());

        // Conversely, a real mutation error DOES persist across rebuilds:
        list->requestMarkAsRead("1");
        network->requests[1].reply->completeWithError(QNetworkReply::InternalServerError, "Mutation failed");
        QCOMPARE(list->m_mutationErrors.size(), 1);
        list->setFilterMode(0);
        list->setFilterMode(4);
        auto* mutationErrWidget =
            qobject_cast<NotificationItemWidget*>(list->listWidget->itemWidget(list->listWidget->item(0)));
        QVERIFY(mutationErrWidget != nullptr);
        QVERIFY(mutationErrWidget->errorLabel->isVisible() || !mutationErrWidget->errorLabel->text().isEmpty());
        QVERIFY(mutationErrWidget->errorLabel->text().contains("Mutation failed"));
    }

    void testAuthoritativeRepoFilterChoices() {
        GitHubClient client;
        auto* network = installNetwork(client);
        MainWindow window;
        prepareMain(window, client);

        // 1. Initial refresh with 2 notifications from different repos: "org/repo-a" (unread) and "org/repo-b" (read)
        window.onRefreshClicked();
        network->requests[0].reply->complete(
            "[{\"id\":\"1\",\"unread\":true,\"subject\":{\"title\":\"Alpha\"},\"repository\":{\"full_name\":\"org/"
            "repo-a\"}},"
            "{\"id\":\"2\",\"unread\":false,\"subject\":{\"title\":\"Beta\"},\"repository\":{\"full_name\":\"org/"
            "repo-b\"}}]");

        auto* list = window.notificationListWidget;
        auto* repoCombo = window.repoFilterComboBox;

        // getAvailableRepos() must contain both "org/repo-a" and "org/repo-b"
        QStringList repos = list->getAvailableRepos();
        QCOMPARE(repos, QStringList({"org/repo-a", "org/repo-b"}));

        // Combo box has "All Repositories", "org/repo-a", "org/repo-b"
        QCOMPARE(repoCombo->count(), 3);
        QCOMPARE(repoCombo->itemText(0), QString("All Repositories"));
        QCOMPARE(repoCombo->itemText(1), QString("org/repo-a"));
        QCOMPARE(repoCombo->itemText(2), QString("org/repo-b"));

        // 2. Read/unread filter does not remove loaded repository choice
        // Default filter mode is 0 (All Unread), so repo-b item is not visible in listWidget,
        // but it MUST still be in getAvailableRepos() and repoCombo!
        QCOMPARE(list->getAvailableRepos(), QStringList({"org/repo-a", "org/repo-b"}));

        // 3. Repository hidden by search remains selectable
        list->setSearchFilter("Alpha");
        QCOMPARE(list->getAvailableRepos(), QStringList({"org/repo-a", "org/repo-b"}));
        list->setSearchFilter("");

        // 4. Select repo "org/repo-b"
        repoCombo->setCurrentIndex(2);
        QCOMPARE(repoCombo->currentText(), QString("org/repo-b"));

        // 5. Pagination append arriving with new repo "org/repo-c"
        Notification notifC;
        notifC.id = "3";
        notifC.unread = true;
        notifC.title = "Gamma";
        notifC.repository = "org/repo-c";
        list->setNotifications({notifC}, true /* append */, false);

        // Newly arriving repo present, selected repo preserved
        QCOMPARE(list->getAvailableRepos(), QStringList({"org/repo-a", "org/repo-b", "org/repo-c"}));
        // Selected repo was "org/repo-b", verify it is preserved
        QCOMPARE(repoCombo->currentText(), QString("org/repo-b"));

        // 6. Fallback to All Repositories only when selected repo genuinely disappears
        // New refresh with only "org/repo-a"
        Notification notifA;
        notifA.id = "4";
        notifA.unread = true;
        notifA.title = "Delta";
        notifA.repository = "org/repo-a";
        list->setNotifications({notifA}, false /* replace */, false);

        QCOMPARE(list->getAvailableRepos(), QStringList({"org/repo-a"}));
        // Since "org/repo-b" disappeared, repoCombo must fallback to "All Repositories"
        QCOMPARE(repoCombo->currentIndex(), 0);
        QCOMPARE(repoCombo->currentText(), QString("All Repositories"));
    }

    void testInstallNotifyRcIdempotence() {
        SettingsDialog dialog;
        dialog.installNotifyRc();

        QString targetDir =
            QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/knotifications6");
        QString targetPath = targetDir + QStringLiteral("/kgithub-notify.notifyrc");

        QVERIFY(QFile::exists(targetPath));
        QVERIFY(dialog.statusLabel->text().contains("Successfully installed"));

        // Repeated installation should succeed cleanly
        dialog.installNotifyRc();
        QVERIFY(QFile::exists(targetPath));
        QVERIFY(dialog.statusLabel->text().contains("Successfully installed"));
    }

    void testRepoListWindowFilterAtomicity() {
        GitHubClient client;
        RepoListWindow repos(&client);

        QJsonObject repo1;
        repo1["name"] = "alpha";
        repo1["fork"] = false;
        repo1["archived"] = false;
        repo1["owner"] = QJsonObject{{"login", "user1"}};

        QJsonObject repo2;
        repo2["name"] = "beta";
        repo2["fork"] = true;
        repo2["archived"] = false;
        repo2["owner"] = QJsonObject{{"login", "user1"}};

        QJsonObject repo3;
        repo3["name"] = "gamma";
        repo3["fork"] = false;
        repo3["archived"] = true;
        repo3["owner"] = QJsonObject{{"login", "user2"}};

        repos.m_allRepos = QJsonArray{repo1, repo2, repo3};

        // Initially filter is "fork:false AND archived:false", so only alpha matches
        repos.addReposToTable(repos.m_allRepos);
        QCOMPARE(repos.m_table->rowCount(), 1);
        QCOMPARE(repos.m_table->item(0, RepoListWindow::ColName)->text(), QString("alpha"));

        // Change filter to "fork:false" -> alpha and gamma match
        repos.m_filterEdit->setText("fork:false");
        QCOMPARE(repos.m_table->rowCount(), 2);
        QVERIFY(repos.m_filterEdit->styleSheet().isEmpty());
        QVERIFY(repos.m_filterEdit->toolTip().isEmpty());

        // Introduce syntax error: incomplete parenthesis -> should keep previous 2 rows untouched!
        repos.m_filterEdit->setText("fork:false AND (");
        QCOMPARE(repos.m_table->rowCount(), 2);
        QVERIFY(!repos.m_filterEdit->styleSheet().isEmpty());
        QVERIFY(repos.m_filterEdit->toolTip().contains("parenthesis", Qt::CaseInsensitive));

        // Another syntax error: unterminated quote -> still keeps 2 rows
        repos.m_filterEdit->setText("\"unterminated");
        QCOMPARE(repos.m_table->rowCount(), 2);
        QVERIFY(!repos.m_filterEdit->styleSheet().isEmpty());
        QVERIFY(repos.m_filterEdit->toolTip().contains("quote", Qt::CaseInsensitive));

        // Introduce unknown structured key error: state:open -> should keep previous 2 rows untouched!
        repos.m_filterEdit->setText("state:open");
        QCOMPARE(repos.m_table->rowCount(), 2);
        QVERIFY(!repos.m_filterEdit->styleSheet().isEmpty());
        QVERIFY(repos.m_filterEdit->toolTip().contains("Unknown filter key: 'state'", Qt::CaseInsensitive));

        // Another unknown key in complex expression -> still keeps 2 rows
        repos.m_filterEdit->setText("name:alpha AND invalid_key:bar");
        QCOMPARE(repos.m_table->rowCount(), 2);
        QVERIFY(!repos.m_filterEdit->styleSheet().isEmpty());
        QVERIFY(repos.m_filterEdit->toolTip().contains("Unknown filter key: 'invalid_key'", Qt::CaseInsensitive));

        // Restore to empty filter -> shows all 3
        repos.m_filterEdit->setText("");
        QCOMPARE(repos.m_table->rowCount(), 3);
        QVERIFY(repos.m_filterEdit->styleSheet().isEmpty());
        QVERIFY(repos.m_filterEdit->toolTip().isEmpty());
    }

    void testMainWindowAuthIncidentLifecycle() {
        GitHubClient client;
        auto* network = installNetwork(client);
        MainWindow window;
        prepareMain(window, client);

        QVERIFY(!window.m_authIncident.inIncident());

        // Step 1 & 2: First authenticated request fails due to authentication;
        // incident activates and exactly one notification is emitted.
        window.onRefreshClicked();
        QCOMPARE(network->requests.size(), 1);
        network->requests[0].reply->completeWithError(QNetworkReply::AuthenticationRequiredError, "Bad credentials",
                                                      401);

        QVERIFY(window.m_authIncident.inIncident());
        QCOMPARE(window.m_authIncident.latestReason(), QString("Invalid Token"));
        // Notification was emitted (willNotify is now false; shouldNotify returned true)
        QVERIFY(!window.m_authIncident.willNotify());

        // Step 3: Repeated auth failure does not emit another notification
        window.onRefreshClicked();
        QCOMPARE(network->requests.size(), 2);
        network->requests[1].reply->completeWithError(QNetworkReply::AuthenticationRequiredError,
                                                      "Still bad credentials", 401);

        QVERIFY(window.m_authIncident.inIncident());
        QCOMPARE(window.m_authIncident.latestReason(), QString("Invalid Token"));
        QVERIFY(!window.m_authIncident.willNotify());

        // Step 4: Replacement credentials are saved; must NOT clear or re-arm the incident
        client.setToken("replacement_token_ghp_xyz");
        QVERIFY(window.m_authIncident.inIncident());
        QVERIFY(!window.m_authIncident.willNotify());

        // Step 5: Another auth failure with replacement credentials still does not duplicate notification
        window.onRefreshClicked();
        QCOMPARE(network->requests.size(), 3);
        network->requests[2].reply->completeWithError(QNetworkReply::AuthenticationRequiredError,
                                                      "Bad replacement token", 401);

        QVERIFY(window.m_authIncident.inIncident());
        QCOMPARE(window.m_authIncident.latestReason(), QString("Invalid Token"));
        QVERIFY(!window.m_authIncident.willNotify());

        // Step 6: Ordinary/non-auth events (e.g. network failure) do not re-arm or clear incident
        window.onRefreshClicked();
        QCOMPARE(network->requests.size(), 4);
        network->requests[3].reply->completeWithError(QNetworkReply::HostNotFoundError, "Host not found", 0);

        QVERIFY(window.m_authIncident.inIncident());
        QCOMPARE(window.m_authIncident.latestReason(), QString("Invalid Token"));
        QVERIFY(!window.m_authIncident.willNotify());

        // Step 7 & 8: Genuinely authenticated GitHub request succeeds; incident clears
        window.onRefreshClicked();
        QCOMPARE(network->requests.size(), 5);
        network->requests[4].reply->complete("[]");

        QVERIFY(!window.m_authIncident.inIncident());
        QVERIFY(window.m_authIncident.latestReason().isEmpty());
        QVERIFY(!window.m_authIncident.willNotify());

        // Step 9: Later auth failure begins a new incident and emits one new notification
        window.onRefreshClicked();
        QCOMPARE(network->requests.size(), 6);
        network->requests[5].reply->completeWithError(QNetworkReply::AuthenticationRequiredError, "Token expired", 401);

        QVERIFY(window.m_authIncident.inIncident());
        QCOMPARE(window.m_authIncident.latestReason(), QString("Invalid Token"));
        // Exactly one new notification was emitted for the new incident:
        QVERIFY(!window.m_authIncident.willNotify());
    }

    static QByteArray makeWorkItemSearchResponse(int totalCount, const QStringList& titles) {
        QJsonArray items;
        for (const QString& title : titles) {
            QJsonObject item;
            item["title"] = title;
            item["html_url"] = "https://github.com/test/repo/issues/1";
            item["state"] = "open";
            item["created_at"] = "2026-01-01T00:00:00Z";
            item["user"] = QJsonObject{{"login", "author1"}};
            item["repository_url"] = "https://api.github.com/repos/test/repo";
            items.append(item);
        }
        QJsonObject root;
        root["total_count"] = totalCount;
        root["items"] = items;
        return QJsonDocument(root).toJson();
    }

    void testWorkItemRefreshWhilePage2InFlight() {
        GitHubClient client;
        FakeNetworkAccessManager fakeManager;
        fakeManager.autoEmitFinished = false;
        WorkItemWindow window(&client, "Issues", WorkItemWindow::EndpointIssues, "query-refresh-while-page-2-in-flight",
                              nullptr, &fakeManager);

        QCOMPARE(fakeManager.requests.size(), 1);
        QUuid gen1 = window.m_currentGenerationId;
        QVERIFY(!gen1.isNull());

        // Complete Page 1 of G1 with 1 item, total_count = 200
        fakeManager.requests[0].reply->complete(makeWorkItemSearchResponse(200, {"Issue 1 (G1)"}));

        // Page 2 of G1 should now be in flight
        QCOMPARE(fakeManager.requests.size(), 2);
        QCOMPARE(fakeManager.requests[1].reply->property("page").toInt(), 2);
        QCOMPARE(fakeManager.requests[1].reply->property("generationId").toString(), gen1.toString());

        // User triggers refresh -> starts G2
        window.startRefresh();
        QUuid gen2 = window.m_currentGenerationId;
        QVERIFY(!gen2.isNull());
        QVERIFY(gen2 != gen1);
        QCOMPARE(fakeManager.requests.size(), 3);
        QCOMPARE(fakeManager.requests[2].reply->property("page").toInt(), 1);
        QCOMPARE(fakeManager.requests[2].reply->property("generationId").toString(), gen2.toString());

        // Now Page 2 of G1 arrives from network
        fakeManager.requests[1].reply->complete(makeWorkItemSearchResponse(200, {"Issue 2 (stale G1)"}));

        // Verify that G1's page 2 was dropped and did not commit
        QCOMPARE(window.m_table->rowCount(), 0);
        QVERIFY(!window.m_stagedPages.contains(2));

        // Complete Page 1 of G2 with total_count = 1
        fakeManager.requests[2].reply->complete(makeWorkItemSearchResponse(1, {"Issue 1 (G2)"}));

        // G2 commits
        QCOMPARE(window.m_table->rowCount(), 1);
        QCOMPARE(window.m_table->item(0, 1)->text(), QString("Issue 1 (G2)"));
    }

    void testWorkItemOldReplyAfterNewGenerationPage1() {
        GitHubClient client;
        FakeNetworkAccessManager fakeManager;
        fakeManager.autoEmitFinished = false;
        WorkItemWindow window(&client, "Issues", WorkItemWindow::EndpointIssues, "query-old-reply-after-new-generation",
                              nullptr, &fakeManager);

        QCOMPARE(fakeManager.requests.size(), 1);  // Page 1 of G1 in flight

        // User refreshes to G2 before G1 arrives
        window.startRefresh();
        QCOMPARE(fakeManager.requests.size(), 2);  // Page 1 of G2 in flight

        // Page 1 of G2 arrives first and commits
        fakeManager.requests[1].reply->complete(makeWorkItemSearchResponse(1, {"G2 Fresh Item"}));
        QCOMPARE(window.m_table->rowCount(), 1);
        QCOMPARE(window.m_table->item(0, 1)->text(), QString("G2 Fresh Item"));

        // Now old Page 1 of G1 arrives
        fakeManager.requests[0].reply->complete(makeWorkItemSearchResponse(1, {"G1 Stale Item"}));

        // Table still displays G2 Fresh Item
        QCOMPARE(window.m_table->rowCount(), 1);
        QCOMPARE(window.m_table->item(0, 1)->text(), QString("G2 Fresh Item"));
    }

    void testWorkItemStaleErrorAfterNewRefresh() {
        GitHubClient client;
        FakeNetworkAccessManager fakeManager;
        fakeManager.autoEmitFinished = false;
        WorkItemWindow window(&client, "Issues", WorkItemWindow::EndpointIssues, "query-stale-error-after-new-refresh",
                              nullptr, &fakeManager);

        QCOMPARE(fakeManager.requests.size(), 1);

        // User refreshes to G2
        window.startRefresh();
        QUuid gen2 = window.m_currentGenerationId;
        QCOMPARE(fakeManager.requests.size(), 2);

        // Stale error from G1 arrives
        fakeManager.requests[0].reply->completeWithError(QNetworkReply::HostNotFoundError, "Stale Network Error", 0);

        // G2 generation should NOT be cancelled
        QCOMPARE(window.m_currentGenerationId, gen2);
        QVERIFY(!window.m_statusLabel->text().contains("Error"));

        // G2 completes successfully
        fakeManager.requests[1].reply->complete(makeWorkItemSearchResponse(1, {"Active Item"}));
        QCOMPARE(window.m_table->rowCount(), 1);
        QCOMPARE(window.m_table->item(0, 1)->text(), QString("Active Item"));
    }

    void testWorkItemDuplicatePageDelivery() {
        GitHubClient client;
        FakeNetworkAccessManager fakeManager;
        fakeManager.autoEmitFinished = false;
        WorkItemWindow window(&client, "Issues", WorkItemWindow::EndpointIssues, "query-duplicate-page-delivery",
                              nullptr, &fakeManager);

        QCOMPARE(fakeManager.requests.size(), 1);
        fakeManager.requests[0].reply->complete(makeWorkItemSearchResponse(1, {"Single Item"}));

        QCOMPARE(window.m_table->rowCount(), 1);
        QVERIFY(window.m_currentGenerationId.isNull());

        // Capture full application state after successful commit
        int expectedRowCount = window.m_table->rowCount();
        QString expectedStatus = window.m_statusLabel->text();
        QJsonArray expectedData = window.m_allData;
        int expectedReqCount = fakeManager.requests.size();
        QString cachePath = window.getCacheFilePath();
        QByteArray cacheContent;
        {
            QFile cf(cachePath);
            if (cf.open(QIODevice::ReadOnly)) cacheContent = cf.readAll();
        }

        // Simulate duplicate delivery after completion
        window.onReplyFinished(fakeManager.requests[0].reply);

        // Verify duplicate delivery was a true no-op (no row republish, no cache rewrite, no status change, no new
        // requests)
        QCOMPARE(window.m_table->rowCount(), expectedRowCount);
        QCOMPARE(window.m_statusLabel->text(), expectedStatus);
        QCOMPARE(window.m_allData, expectedData);
        QCOMPARE(fakeManager.requests.size(), expectedReqCount);
        QVERIFY(window.m_stagedPages.isEmpty());
        QVERIFY(window.m_currentGenerationId.isNull());
        {
            QFile cf(cachePath);
            if (cf.open(QIODevice::ReadOnly)) {
                QCOMPARE(cf.readAll(), cacheContent);
            }
        }
    }

    void testWorkItemRefreshLaterPageFailureThenSuccessAtomicallyUpdates() {
        GitHubClient client;
        FakeNetworkAccessManager fakeManager;
        fakeManager.autoEmitFinished = false;
        WorkItemWindow window(&client, "Issues", WorkItemWindow::EndpointIssues, "query-later-failure-then-success",
                              nullptr, &fakeManager);

        // 1. Initial good dataset (G1) succeeds and is displayed and cached
        QCOMPARE(fakeManager.requests.size(), 1);
        fakeManager.requests[0].reply->complete(makeWorkItemSearchResponse(2, {"Good Item 1", "Good Item 2"}));
        QCOMPARE(window.m_table->rowCount(), 2);
        QCOMPARE(window.m_table->item(0, 1)->text(), QString("Good Item 1"));
        QCOMPARE(window.m_table->item(1, 1)->text(), QString("Good Item 2"));

        QString cachePath = window.getCacheFilePath();
        QByteArray initialCacheContent;
        {
            QFile cf(cachePath);
            QVERIFY(cf.open(QIODevice::ReadOnly));
            initialCacheContent = cf.readAll();
        }

        // 2. Refresh generation G2 starts
        window.startRefresh();
        QUuid gen2 = window.m_currentGenerationId;
        QVERIFY(!gen2.isNull());
        QCOMPARE(fakeManager.requests.size(), 2);

        // 3. Page 1 of G2 arrives with 1 item, total_count = 200 -> Page 2 scheduled
        fakeManager.requests[1].reply->complete(makeWorkItemSearchResponse(200, {"Pending Item 1"}));
        QCOMPARE(fakeManager.requests.size(), 3);

        // 4. Later page (Page 2) fails!
        fakeManager.requests[2].reply->completeWithError(QNetworkReply::InternalServerError, "Server Error", 500);

        // Old displayed and cached data remain completely untouched!
        QCOMPARE(window.m_table->rowCount(), 2);
        QCOMPARE(window.m_table->item(0, 1)->text(), QString("Good Item 1"));
        QCOMPARE(window.m_table->item(1, 1)->text(), QString("Good Item 2"));
        {
            QFile cf(cachePath);
            QVERIFY(cf.open(QIODevice::ReadOnly));
            QCOMPARE(cf.readAll(), initialCacheContent);
        }
        QVERIFY(window.m_stagedPages.isEmpty());
        QVERIFY(window.m_currentGenerationId.isNull());

        // 5. Subsequent fresh generation G3 starts
        window.startRefresh();
        QUuid gen3 = window.m_currentGenerationId;
        QVERIFY(!gen3.isNull());
        QVERIFY(gen3 != gen2);
        QCOMPARE(fakeManager.requests.size(), 4);

        // 6. G3 Page 1 arrives (total_count = 200) -> schedules Page 2
        fakeManager.requests[3].reply->complete(makeWorkItemSearchResponse(200, {"Fresh Item 1"}));
        QCOMPARE(fakeManager.requests.size(), 5);

        // G3 Page 2 arrives successfully with 1 item
        fakeManager.requests[4].reply->complete(makeWorkItemSearchResponse(200, {"Fresh Item 2"}));

        // G3 completes and atomically publishes to table and cache!
        QCOMPARE(window.m_table->rowCount(), 2);
        QCOMPARE(window.m_table->item(0, 1)->text(), QString("Fresh Item 1"));
        QCOMPARE(window.m_table->item(1, 1)->text(), QString("Fresh Item 2"));
        QVERIFY(window.m_currentGenerationId.isNull());
        {
            QFile cf(cachePath);
            QVERIFY(cf.open(QIODevice::ReadOnly));
            QByteArray freshCache = cf.readAll();
            QVERIFY(freshCache != initialCacheContent);
            QVERIFY(freshCache.contains("Fresh Item 1"));
            QVERIFY(freshCache.contains("Fresh Item 2"));
        }
    }

    void testWorkItemLaterPageFailurePreservesData() {
        GitHubClient client;
        FakeNetworkAccessManager fakeManager;
        fakeManager.autoEmitFinished = false;
        WorkItemWindow window(&client, "Issues", WorkItemWindow::EndpointIssues, "query-later-page-failure", nullptr,
                              &fakeManager);

        // Initial fetch: 1 item committed
        fakeManager.requests[0].reply->complete(makeWorkItemSearchResponse(1, {"Initial Item"}));
        QCOMPARE(window.m_table->rowCount(), 1);
        QCOMPARE(window.m_table->item(0, 1)->text(), QString("Initial Item"));

        // User starts refresh -> G2
        window.startRefresh();
        QCOMPARE(fakeManager.requests.size(), 2);  // G2 page 1

        // G2 Page 1 arrives with 1 item, total_count = 200
        fakeManager.requests[1].reply->complete(makeWorkItemSearchResponse(200, {"New Page 1"}));
        QCOMPARE(fakeManager.requests.size(), 3);  // G2 page 2 in flight

        // Table still preserves "Initial Item" while G2 is in progress
        QCOMPARE(window.m_table->rowCount(), 1);
        QCOMPARE(window.m_table->item(0, 1)->text(), QString("Initial Item"));

        // G2 Page 2 fails!
        fakeManager.requests[2].reply->completeWithError(QNetworkReply::InternalServerError, "Server error", 500);

        // Staged data discarded, generation cancelled
        QVERIFY(window.m_stagedPages.isEmpty());
        QVERIFY(window.m_currentGenerationId.isNull());

        // Existing table data preserved!
        QCOMPARE(window.m_table->rowCount(), 1);
        QCOMPARE(window.m_table->item(0, 1)->text(), QString("Initial Item"));
        QVERIFY(window.m_statusLabel->text().contains("Error"));
    }

    void testWorkItemDuplicateNextPagePrevention() {
        GitHubClient client;
        FakeNetworkAccessManager fakeManager;
        fakeManager.autoEmitFinished = false;
        WorkItemWindow window(&client, "Issues", WorkItemWindow::EndpointIssues, "query-duplicate-next-page-prevention",
                              nullptr, &fakeManager);

        QCOMPARE(fakeManager.requests.size(), 1);
        fakeManager.requests[0].reply->complete(makeWorkItemSearchResponse(200, {"Page 1 Item"}));

        // Page 2 is now in flight
        QCOMPARE(fakeManager.requests.size(), 2);
        QVERIFY(window.m_inFlightPages.contains(2));

        // Attempting duplicate fetch of page 2 should be a no-op
        window.fetchPage(window.m_currentGenerationId, 2);
        QCOMPARE(fakeManager.requests.size(), 2);
    }

    void testWorkItemDestructionWithInFlightRequest() {
        GitHubClient client;
        FakeNetworkAccessManager fakeManager;
        fakeManager.autoEmitFinished = false;
        auto* window = new WorkItemWindow(&client, "Issues", WorkItemWindow::EndpointIssues,
                                          "query-destruction-with-in-flight-request", nullptr, &fakeManager);

        QCOMPARE(fakeManager.requests.size(), 1);
        QVERIFY(!fakeManager.requests[0].reply->isFinished());

        // Delete window while request is in flight
        delete window;

        // Completing or interacting with reply afterwards must not crash
        fakeManager.requests[0].reply->complete(makeWorkItemSearchResponse(1, {"Orphaned Item"}));
    }
};

QTEST_MAIN(TestRequestConsumers)
#include "TestRequestConsumers.moc"
