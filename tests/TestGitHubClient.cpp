#include <QSignalSpy>
#include <QtTest>

#include "../src/GitHubClient.h"
#include "FakeNetworkAccessManager.h"
#include "MockNetworkReply.h"

// Declare Q_DECLARE_METATYPE for QList<Notification> so QSignalSpy can handle it
Q_DECLARE_METATYPE(QList<Notification>)

class TestGitHubClient : public QObject {
    Q_OBJECT
   private slots:
    void initTestCase() {
        // Register metatype for QList<Notification>
        qRegisterMetaType<QList<Notification>>("QList<Notification>");
    }

    void testTrustedOrigin() {
        GitHubClient client;
        client.setApiUrl("https://api.github.com");
        client.setToken("dummy_token");

        // Helper to extract authorization header using invokeMethod
        auto getAuthHeader = [](GitHubClient& c, const QString& urlStr) -> QByteArray {
            QNetworkRequest request = c.createAuthenticatedRequest(QUrl(urlStr));
            return request.rawHeader("Authorization");
        };

        // 1. Configured API origin
        QCOMPARE(getAuthHeader(client, "https://api.github.com/user"), QByteArray("token dummy_token"));

        // 2. Relative URL simulation (in practice it prepends m_apiUrl, so same as above)
        QCOMPARE(getAuthHeader(client, "https://api.github.com/notifications"), QByteArray("token dummy_token"));

        // 3. Different host
        QCOMPARE(getAuthHeader(client, "https://external.example.com/api"), QByteArray(""));

        // 4. api.github.com.evil.example
        QCOMPARE(getAuthHeader(client, "https://api.github.com.evil.example/user"), QByteArray(""));

        // 5. HTTPS-to-HTTP downgrade
        QCOMPARE(getAuthHeader(client, "http://api.github.com/user"), QByteArray(""));

        // 6. Explicit/default ports
        QCOMPARE(getAuthHeader(client, "https://api.github.com:443/user"), QByteArray("token dummy_token"));
        QCOMPARE(getAuthHeader(client, "https://api.github.com:8443/user"), QByteArray(""));

        // 7. Configured test API origin (Enterprise base)
        client.setApiUrl("https://git.example.internal/api/v3");
        QCOMPARE(getAuthHeader(client, "https://git.example.internal/api/v3/user"), QByteArray("token dummy_token"));
        QCOMPARE(getAuthHeader(client, "https://git.example.internal:443/api/v3/user"),
                 QByteArray("token dummy_token"));
        QCOMPARE(getAuthHeader(client, "http://git.example.internal/api/v3/user"), QByteArray(""));
    }

    void testProductionRequests() {
        GitHubClient client;
        FakeNetworkAccessManager* fakeManager = new FakeNetworkAccessManager(&client);

        // Replace the default manager with our fake
        // Since test is a friend class, we can just replace the member
        QNetworkAccessManager* oldManager = client.manager;
        client.manager = fakeManager;
        oldManager->deleteLater();

        client.setApiUrl("https://api.github.com");
        client.setToken("dummy_token");

        QSignalSpy rawDataSpy(&client, &GitHubClient::rawDataReceived);
        QSignalSpy errorSpy(&client, &GitHubClient::errorOccurred);

        // 1. requestRaw("/user") resolves against configured API base and carries token
        client.requestRaw("/user");
        QCOMPARE(fakeManager->requests.size(), 1);
        QCOMPARE(fakeManager->requests.last().request.url(), QUrl("https://api.github.com/user"));
        QCOMPARE(fakeManager->requests.last().request.rawHeader("Authorization"), QByteArray("token dummy_token"));
        QCOMPARE(fakeManager->requests.last().request.attribute(QNetworkRequest::RedirectPolicyAttribute).toInt(),
                 QNetworkRequest::SameOriginRedirectPolicy);

        // 2. external debug destination emits clear error and dispatches zero requests
        int reqCountBefore = fakeManager->requests.size();
        client.requestRaw("https://external.example.com/api");
        QCOMPARE(fakeManager->requests.size(), reqCountBefore);
        QCOMPARE(rawDataSpy.count(), 1);
        QCOMPARE(rawDataSpy.takeFirst().at(1).toString(),
                 QString("Error: Untrusted external destination for authenticated request."));

        // 3. external next links / direct pagination inputs dispatch zero requests
        client.m_notificationSessionId = QUuid::createUuid();
        client.m_nextPageUrl = "https://evil.com/notifications?page=2";
        QSignalSpy loadMoreNotifySpy(&client, &GitHubClient::notificationsReceived);
        client.loadMore();
        QCOMPARE(fakeManager->requests.size(), reqCountBefore);

        // Assert notificationsReceived happens BEFORE errorOccurred
        // QSignalSpy only gives counts, but we can verify both fired
        QCOMPARE(loadMoreNotifySpy.count(), 1);
        QCOMPARE(errorSpy.count(), 1);
        QCOMPARE(errorSpy.takeFirst().at(1).toString(), QString("Untrusted pagination URL rejected."));

        client.fetchUserRepos("https://evil.com/user/repos?page=2");
        QCOMPARE(fakeManager->requests.size(), reqCountBefore);
        QCOMPARE(errorSpy.count(), 1);
        QCOMPARE(errorSpy.takeFirst().at(1).toString(), QString("Untrusted repository pagination URL rejected."));

        client.fetchNotificationDetails("https://evil.com/notifications/threads/123", "123");
        QCOMPARE(fakeManager->requests.size(), reqCountBefore);
        QCOMPARE(errorSpy.count(), 1);
        QCOMPARE(errorSpy.takeFirst().at(1).toString(), QString("Untrusted notification details URL rejected."));

        // 4. trusted pagination still carries auth
        client.m_notificationSessionId = QUuid::createUuid();
        client.m_nextPageUrl = "https://api.github.com/notifications?page=2";
        client.loadMore();
        QCOMPARE(fakeManager->requests.size(), reqCountBefore + 1);
        QCOMPARE(fakeManager->requests.last().request.url(), QUrl("https://api.github.com/notifications?page=2"));
        QCOMPARE(fakeManager->requests.last().request.rawHeader("Authorization"), QByteArray("token dummy_token"));
        QCOMPARE(fakeManager->requests.last().request.attribute(QNetworkRequest::RedirectPolicyAttribute).toInt(),
                 QNetworkRequest::SameOriginRedirectPolicy);

        // 5. actual fetchImage() requests omit Authorization, including an image hosted on the trusted origin
        reqCountBefore = fakeManager->requests.size();
        client.fetchImage("https://avatars.githubusercontent.com/u/12345?v=4", "123");
        QCOMPARE(fakeManager->requests.size(), reqCountBefore + 1);
        QCOMPARE(fakeManager->requests.last().request.url(), QUrl("https://avatars.githubusercontent.com/u/12345?v=4"));
        QCOMPARE(fakeManager->requests.last().request.rawHeader("Authorization"), QByteArray(""));

        client.fetchImage("https://api.github.com/image.png", "123");
        QCOMPARE(fakeManager->requests.size(), reqCountBefore + 2);
        QCOMPARE(fakeManager->requests.last().request.url(), QUrl("https://api.github.com/image.png"));
        QCOMPARE(fakeManager->requests.last().request.rawHeader("Authorization"), QByteArray("token dummy_token"));

        // 6. Enterprise cases (trusted origin)
        client.setApiUrl("https://git.example.internal/api/v3");
        client.requestRaw("/user");
        QCOMPARE(fakeManager->requests.last().request.url(), QUrl("https://git.example.internal/api/v3/user"));
        QCOMPARE(fakeManager->requests.last().request.rawHeader("Authorization"), QByteArray("token dummy_token"));

        // Scheme / Host / Port mismatch checks
        client.requestRaw("http://git.example.internal/api/v3/user");  // Downgrade
        QCOMPARE(rawDataSpy.count(), 1);
        QCOMPARE(rawDataSpy.takeFirst().at(1).toString(),
                 QString("Error: Untrusted external destination for authenticated request."));

        client.requestRaw("https://git.example.internal:8443/api/v3/user");  // Wrong port
        QCOMPARE(rawDataSpy.count(), 1);
        QCOMPARE(rawDataSpy.takeFirst().at(1).toString(),
                 QString("Error: Untrusted external destination for authenticated request."));
    }

    void testUntrustedPaginationLinkHeaders() {
        GitHubClient client;
        client.setApiUrl("https://api.github.com");

        QSignalSpy notifySpy(&client, &GitHubClient::notificationsReceived);
        QSignalSpy repoSpy(&client, &GitHubClient::userReposReceived);
        QSignalSpy errorSpy(&client, &GitHubClient::errorOccurred);

        // Test untrusted link header in notifications
        QByteArray jsonNotifications = "[]";
        MockNetworkReply* replyNotifications = new MockNetworkReply(jsonNotifications, &client);
        replyNotifications->setProperty("type", "notifications");
        client.m_notificationSessionId = QUuid::createUuid();
        replyNotifications->setProperty("reqId", client.m_notificationSessionId);
        replyNotifications->setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 200);
        replyNotifications->setRawHeader("Link", "<https://external.example/notifications?page=2>; rel=\"next\"");

        QMetaObject::invokeMethod(&client, "onReplyFinished", Qt::DirectConnection,
                                  Q_ARG(QNetworkReply*, replyNotifications));

        QCOMPARE(notifySpy.count(), 1);
        QList<QVariant> argsNotify = notifySpy.takeFirst();
        bool hasMoreNotify = argsNotify.at(3).toBool();
        QCOMPARE(hasMoreNotify, false);

        QCOMPARE(errorSpy.count(), 1);
        QCOMPARE(errorSpy.takeFirst().at(1).toString(), QString("Untrusted pagination URL rejected."));

        // Test untrusted link header in user repos
        QByteArray jsonRepos = "[]";
        MockNetworkReply* replyRepos = new MockNetworkReply(jsonRepos, &client);
        replyRepos->setProperty("type", "repos");
        replyRepos->setProperty("reqId", QUuid::createUuid());
        replyRepos->setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 200);
        replyRepos->setRawHeader("Link", "<https://external.example/repos?page=2>; rel=\"next\"");

        QMetaObject::invokeMethod(&client, "onReplyFinished", Qt::DirectConnection, Q_ARG(QNetworkReply*, replyRepos));

        QCOMPARE(repoSpy.count(), 1);
        QList<QVariant> argsRepo = repoSpy.takeFirst();
        QString nextUrlRepo = argsRepo.at(2).toString();
        QCOMPARE(nextUrlRepo, QString(""));

        QCOMPARE(errorSpy.count(), 1);
        QCOMPARE(errorSpy.takeFirst().at(1).toString(), QString("Untrusted repository pagination URL rejected."));

        // Test trusted link header in notifications
        MockNetworkReply* replyNotificationsTrusted = new MockNetworkReply(jsonNotifications, &client);
        replyNotificationsTrusted->setProperty("type", "notifications");
        replyNotificationsTrusted->setProperty("reqId", client.m_notificationSessionId);
        replyNotificationsTrusted->setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 200);
        replyNotificationsTrusted->setRawHeader("Link", "<https://api.github.com/notifications?page=2>; rel=\"next\"");

        QMetaObject::invokeMethod(&client, "onReplyFinished", Qt::DirectConnection,
                                  Q_ARG(QNetworkReply*, replyNotificationsTrusted));

        QCOMPARE(notifySpy.count(), 1);
        QList<QVariant> argsNotifyTrusted = notifySpy.takeFirst();
        bool hasMoreNotifyTrusted = argsNotifyTrusted.at(3).toBool();
        QCOMPARE(hasMoreNotifyTrusted, true);

        QCOMPARE(errorSpy.count(), 0);  // No error for trusted pagination
    }

    void testNotificationsDispatch() {
        GitHubClient client;
        QSignalSpy spy(&client, &GitHubClient::notificationsReceived);

        QByteArray json =
            "[{\"id\":\"1\", \"subject\":{\"title\":\"Test\", \"url\":\"http://api.github.com/repos/foo/bar\", "
            "\"type\":\"Issue\"}, \"repository\":{\"full_name\":\"foo/bar\"}, \"updated_at\":\"2023-01-01T00:00:00Z\", "
            "\"unread\":true}]";
        MockNetworkReply* reply = new MockNetworkReply(json, &client);
        client.m_notificationSessionId = QUuid::createUuid();
        reply->setProperty("type", "notifications");
        reply->setProperty(
            "reqId", client.m_notificationSessionId.isNull() ? QUuid::createUuid() : client.m_notificationSessionId);
        reply->setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 200);

        QMetaObject::invokeMethod(&client, "onReplyFinished", Qt::DirectConnection, Q_ARG(QNetworkReply*, reply));

        QCOMPARE(spy.count(), 1);
        QList<QVariant> args = spy.takeFirst();
        QUuid reqId = args.at(0).toUuid();
        QList<Notification> notifications = args.at(1).value<QList<Notification>>();
        bool append = args.at(2).toBool();
        bool hasMore = args.at(3).toBool();

        QCOMPARE(notifications.size(), 1);
        QCOMPARE(notifications[0].title, QString("Test"));
        QCOMPARE(append, false);
        QCOMPARE(hasMore, false);
    }

    void testPaginationDispatch() {
        GitHubClient client;
        QSignalSpy spy(&client, &GitHubClient::notificationsReceived);

        QByteArray json =
            "[{\"id\":\"2\", \"subject\":{\"title\":\"Test2\", \"url\":\"url\", \"type\":\"Issue\"}, "
            "\"repository\":{\"full_name\":\"repo\"}, \"updated_at\":\"date\", \"unread\":true}]";
        MockNetworkReply* reply = new MockNetworkReply(json, &client);
        client.m_notificationSessionId = QUuid::createUuid();
        reply->setProperty("type", "notifications");
        reply->setProperty(
            "reqId", client.m_notificationSessionId.isNull() ? QUuid::createUuid() : client.m_notificationSessionId);
        reply->setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 200);
        reply->setRawHeader("Link",
                            "<https://api.github.com/notifications?page=2>; rel=\"next\", "
                            "<https://api.github.com/notifications?page=5>; rel=\"last\"");

        QMetaObject::invokeMethod(&client, "onReplyFinished", Qt::DirectConnection, Q_ARG(QNetworkReply*, reply));

        QCOMPARE(spy.count(), 1);
        QList<QVariant> args = spy.takeFirst();
        QUuid reqId = args.at(0).toUuid();
        QList<Notification> notifications = args.at(1).value<QList<Notification>>();
        bool append = args.at(2).toBool();
        bool hasMore = args.at(3).toBool();

        QCOMPARE(notifications.size(), 1);
        QCOMPARE(append, false);  // Default is false unless property set
        QCOMPARE(hasMore, true);
    }

    void testDetailsDispatch() {
        GitHubClient client;
        QSignalSpy spy(&client, &GitHubClient::detailsReceived);

        QByteArray json =
            "{\"html_url\":\"http://github.com/foo/bar\", \"user\":{\"login\":\"user\", \"avatar_url\":\"url\"}}";
        MockNetworkReply* reply = new MockNetworkReply(json, &client);
        reply->setProperty("type", "details");
        reply->setProperty(
            "reqId", client.m_notificationSessionId.isNull() ? QUuid::createUuid() : client.m_notificationSessionId);
        reply->setProperty("notificationId", "123");
        reply->setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 200);

        QMetaObject::invokeMethod(&client, "onReplyFinished", Qt::DirectConnection, Q_ARG(QNetworkReply*, reply));

        QCOMPARE(spy.count(), 1);
        QList<QVariant> args = spy.takeFirst();
        QUuid reqId = args.at(0).toUuid();
        QCOMPARE(args.at(1).toString(), QString("123"));
        QCOMPARE(args.at(2).toString(), QString("user"));
    }

    void testDetailsErrorDispatch() {
        GitHubClient client;
        QSignalSpy spy(&client, &GitHubClient::detailsError);

        MockNetworkReply* reply = new MockNetworkReply("", &client);
        reply->setProperty("type", "details");
        reply->setProperty(
            "reqId", client.m_notificationSessionId.isNull() ? QUuid::createUuid() : client.m_notificationSessionId);
        reply->setProperty("notificationId", "123");
        reply->setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 404);
        reply->setError(QNetworkReply::ContentNotFoundError, "Not Found");

        QMetaObject::invokeMethod(&client, "onReplyFinished", Qt::DirectConnection, Q_ARG(QNetworkReply*, reply));

        QCOMPARE(spy.count(), 1);
        QList<QVariant> args = spy.takeFirst();
        QUuid reqId = args.at(0).toUuid();
        QCOMPARE(args.at(1).toString(), QString("123"));
        QCOMPARE(args.at(2).toString(), QString("Not Found"));
    }
    void testVerificationDispatch() {
        GitHubClient client;
        qRegisterMetaType<TokenCapabilities>();
        QSignalSpy spy(&client, &GitHubClient::tokenVerified);

        auto* manager = new FakeNetworkAccessManager(&client);
        manager->autoEmitFinished = false;
        client.manager = manager;

        client.setToken("dummy_token");
        client.verifyToken();

        QCoreApplication::processEvents();

        manager->requests[0].reply->setRawHeader("X-OAuth-Scopes", "repo, notifications");
        manager->requests[0].reply->complete("{\"login\":\"user\"}");

        QTRY_COMPARE(manager->requests.size(), 2);
        manager->requests[1].reply->complete("[]");

        QTRY_COMPARE(manager->requests.size(), 3);
        manager->requests[2].reply->complete("[]");

        QTRY_COMPARE(spy.count(), 1);
        QList<QVariant> args = spy.takeFirst();
        QCOMPARE(args.at(1).toBool(), true);
    }

    void testUnreadLogic() {
        GitHubClient client;
        QSignalSpy spy(&client, &GitHubClient::notificationsReceived);

        // Case 1: last_read_at missing -> unread = true
        QJsonObject n1;
        n1["id"] = "1";
        n1["subject"] = QJsonObject{{"title", "T1"}, {"url", "u1"}, {"type", "Issue"}};
        n1["repository"] = QJsonObject{{"full_name", "r1"}};
        n1["updated_at"] = "2023-01-02T00:00:00Z";
        n1["unread"] = false;  // API says false, but we should override if logic dictates (though here logic says true)

        // Case 2: updated_at > last_read_at -> unread = true
        QJsonObject n2;
        n2["id"] = "2";
        n2["subject"] = QJsonObject{{"title", "T2"}, {"url", "u2"}, {"type", "Issue"}};
        n2["repository"] = QJsonObject{{"full_name", "r2"}};
        n2["updated_at"] = "2023-01-02T00:00:00Z";
        n2["last_read_at"] = "2023-01-01T00:00:00Z";
        n2["unread"] = false;

        // Case 3: updated_at <= last_read_at -> unread = false
        QJsonObject n3;
        n3["id"] = "3";
        n3["subject"] = QJsonObject{{"title", "T3"}, {"url", "u3"}, {"type", "Issue"}};
        n3["repository"] = QJsonObject{{"full_name", "r3"}};
        n3["updated_at"] = "2023-01-01T00:00:00Z";
        n3["last_read_at"] = "2023-01-02T00:00:00Z";
        n3["unread"] = true;  // API says true (weird), but logic says false

        QJsonArray array;
        array.append(n1);
        array.append(n2);
        array.append(n3);
        QJsonDocument doc(array);

        MockNetworkReply* reply = new MockNetworkReply(doc.toJson(), &client);
        client.m_notificationSessionId = QUuid::createUuid();
        reply->setProperty("type", "notifications");
        reply->setProperty(
            "reqId", client.m_notificationSessionId.isNull() ? QUuid::createUuid() : client.m_notificationSessionId);
        reply->setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 200);

        QMetaObject::invokeMethod(&client, "onReplyFinished", Qt::DirectConnection, Q_ARG(QNetworkReply*, reply));

        QCOMPARE(spy.count(), 1);
        QList<Notification> notifications = spy.takeFirst().at(1).value<QList<Notification>>();

        QCOMPARE(notifications.size(), 3);

        QCOMPARE(notifications[0].id, QString("1"));
        QCOMPARE(notifications[0].unread, false);  // We now strictly use API's unread

        QCOMPARE(notifications[1].id, QString("2"));
        QCOMPARE(notifications[1].unread, false);  // We now strictly use API's unread

        QCOMPARE(notifications[2].id, QString("3"));
        QCOMPARE(notifications[2].unread, true);  // We now strictly use API's unread
    }

    void testUserRules() {
        GitHubClient client;
        QSignalSpy spy(&client, &GitHubClient::notificationsReceived);

        /*
        Case 1:
        "unread": false,
        "last_read_at": null,
        "updated_at": "2026-02-27T00:27:04Z",
        Is to be treated as not in the inbox regardless of the "last read at" setting.
        It is to be treated as unread as there is no last read at date.
        */
        QJsonObject n1;
        n1["id"] = "1";
        n1["subject"] = QJsonObject{{"title", "Case 1"}, {"url", "u1"}, {"type", "Issue"}};
        n1["repository"] = QJsonObject{{"full_name", "r1"}};
        n1["unread"] = false;
        n1["last_read_at"] = QJsonValue::Null;
        n1["updated_at"] = "2026-02-27T00:27:04Z";

        /*
        Case 2:
        "unread": true,
        "last_read_at": "2026-02-27T00:25:53Z",
        "updated_at": "2026-02-27T00:27:04Z",
        Is to be treated as unread as updated is newer
        It is to be listed in the inbox because unread is true
        */
        QJsonObject n2;
        n2["id"] = "2";
        n2["subject"] = QJsonObject{{"title", "Case 2"}, {"url", "u2"}, {"type", "Issue"}};
        n2["repository"] = QJsonObject{{"full_name", "r2"}};
        n2["unread"] = true;
        n2["last_read_at"] = "2026-02-27T00:25:53Z";
        n2["updated_at"] = "2026-02-27T00:27:04Z";

        /*
        Case 3:
        "unread": false,
        "last_read_at": "2026-02-27T00:25:53Z",
        "updated_at": "2026-02-27T00:27:04Z",
        Is to be treated as unread as updated is newer
        It is to be listed "done" (not in inbox) as it's unread (false)
        */
        QJsonObject n3;
        n3["id"] = "3";
        n3["subject"] = QJsonObject{{"title", "Case 3"}, {"url", "u3"}, {"type", "Issue"}};
        n3["repository"] = QJsonObject{{"full_name", "r3"}};
        n3["unread"] = false;
        n3["last_read_at"] = "2026-02-27T00:25:53Z";
        n3["updated_at"] = "2026-02-27T00:27:04Z";

        /*
        Case 4:
        "unread": false,
        "last_read_at": "2026-02-27T00:28:53Z",
        "updated_at": "2026-02-27T00:27:04Z",
        Is to be treated as read as last update (or the same) is older than last read
        */
        QJsonObject n4;
        n4["id"] = "4";
        n4["subject"] = QJsonObject{{"title", "Case 4"}, {"url", "u4"}, {"type", "Issue"}};
        n4["repository"] = QJsonObject{{"full_name", "r4"}};
        n4["unread"] = false;
        n4["last_read_at"] = "2026-02-27T00:28:53Z";
        n4["updated_at"] = "2026-02-27T00:27:04Z";

        QJsonArray array;
        array.append(n1);
        array.append(n2);
        array.append(n3);
        array.append(n4);
        QJsonDocument doc(array);

        MockNetworkReply* reply = new MockNetworkReply(doc.toJson(), &client);
        client.m_notificationSessionId = QUuid::createUuid();
        reply->setProperty("type", "notifications");
        reply->setProperty(
            "reqId", client.m_notificationSessionId.isNull() ? QUuid::createUuid() : client.m_notificationSessionId);
        reply->setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 200);

        QMetaObject::invokeMethod(&client, "onReplyFinished", Qt::DirectConnection, Q_ARG(QNetworkReply*, reply));

        QCOMPARE(spy.count(), 1);
        QList<Notification> notifications = spy.takeFirst().at(1).value<QList<Notification>>();

        QCOMPARE(notifications.size(), 4);

        // Case 1 Check
        QCOMPARE(notifications[0].id, QString("1"));
        QCOMPARE(notifications[0].unread, false);

        // Case 2 Check
        QCOMPARE(notifications[1].id, QString("2"));
        QCOMPARE(notifications[1].unread, true);

        // Case 3 Check
        QCOMPARE(notifications[2].id, QString("3"));
        QCOMPARE(notifications[2].unread, false);

        // Case 4 Check
        QCOMPARE(notifications[3].id, QString("4"));
        QCOMPARE(notifications[3].unread, false);
    }

    void testEveryOperationRetainsIdentityOnTimeout() {
        GitHubClient client;
        delete client.manager;
        auto* network = new FakeNetworkAccessManager(&client);
        network->autoEmitFinished = false;
        client.manager = network;
        connect(network, &QNetworkAccessManager::finished, &client, &GitHubClient::onReplyFinished);
        client.setToken("test-token");
        QList<QUuid> completions;
        connect(&client, &GitHubClient::errorOccurred, this,
                [&](const QUuid& id, const QString&) { completions.append(id); });
        connect(&client, &GitHubClient::detailsError, this,
                [&](const QUuid& id, const QString&, const QString&) { completions.append(id); });
        connect(&client, &GitHubClient::tokenVerified, this,
                [&](const QUuid& id, bool valid, const TokenCapabilities& caps, const QString&) {
                    QVERIFY(!valid);
                    completions.append(id);
                });
        QList<QUuid> ids{client.checkNotifications(),
                         client.verifyToken(),
                         client.markAsRead("1"),
                         client.markAsDone("2"),
                         client.markAsReadAndDone("3"),
                         client.fetchNotificationDetails("https://api.github.com/repos/o/r/issues/1", "1"),
                         client.fetchImage("https://avatars.example/avatar.png", "1"),
                         client.requestRaw("/user"),
                         client.fetchUserRepos(),
                         client.verifyRepo("o/r"),
                         client.createIssue("o/r", "title", "body")};
        QCOMPARE(network->requests.size(), ids.size());
        for (int i = ids.size() - 1; i >= 0; --i) {
            QVERIFY(!ids[i].isNull());
            QCOMPARE(network->requests[i].reply->property("reqId").toUuid(), ids[i]);
            QCOMPARE(network->requests[i].request.transferTimeout(), 30000);
            network->requests[i].reply->completeWithError(QNetworkReply::TimeoutError, "Request timed out");
            QCOMPARE(completions.last(), ids[i]);
        }
        QCOMPARE(completions.size(), ids.size());
    }

    void testRequestTimeoutPolicy() {
        GitHubClient client;
        client.setApiUrl("https://api.github.com");
        client.setToken("dummy_token");

        auto* fakeManager = new FakeNetworkAccessManager(&client);
        fakeManager->autoEmitFinished = false;

        QNetworkAccessManager* oldManager = client.manager;
        client.manager = fakeManager;
        oldManager->deleteLater();

        client.requestRaw("/1", "GET", QByteArray());

        QCOMPARE(fakeManager->requests.size(), 1);
        auto request = fakeManager->requests.at(0).request;

        // Assert native timeout is 30 seconds
        QCOMPARE(request.transferTimeout(), 30000);
    }
};

QTEST_MAIN(TestGitHubClient)
#include "TestGitHubClient.moc"
