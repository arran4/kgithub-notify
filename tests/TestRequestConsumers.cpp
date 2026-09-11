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
#include "../src/NotificationItemWidget.h"
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
};

QTEST_MAIN(TestRequestConsumers)
#include "TestRequestConsumers.moc"
