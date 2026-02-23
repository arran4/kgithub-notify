#include <QtTest>
#include <QSignalSpy>
#include "../src/GitHubClient.h"
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

    void testNotificationsDispatch() {
        GitHubClient client;
        QSignalSpy spy(&client, &GitHubClient::notificationsReceived);

        QByteArray json = "[{\"id\":\"1\", \"subject\":{\"title\":\"Test\", \"url\":\"http://api.github.com/repos/foo/bar\", \"type\":\"Issue\"}, \"repository\":{\"full_name\":\"foo/bar\"}, \"updated_at\":\"2023-01-01T00:00:00Z\", \"unread\":true}]";
        MockNetworkReply *reply = new MockNetworkReply(json, &client);
        reply->setProperty("type", "notifications");
        reply->setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 200);

        QMetaObject::invokeMethod(&client, "onReplyFinished", Qt::DirectConnection, Q_ARG(QNetworkReply*, reply));

        QCOMPARE(spy.count(), 1);
        QList<Notification> notifications = spy.takeFirst().at(0).value<QList<Notification>>();
        QCOMPARE(notifications.size(), 1);
        QCOMPARE(notifications[0].title, QString("Test"));
    }

    void testDetailsDispatch() {
        GitHubClient client;
        QSignalSpy spy(&client, &GitHubClient::detailsReceived);

        QByteArray json = "{\"html_url\":\"http://github.com/foo/bar\", \"user\":{\"login\":\"user\", \"avatar_url\":\"url\"}}";
        MockNetworkReply *reply = new MockNetworkReply(json, &client);
        reply->setProperty("type", "details");
        reply->setProperty("notificationId", "123");
        reply->setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 200);

        QMetaObject::invokeMethod(&client, "onReplyFinished", Qt::DirectConnection, Q_ARG(QNetworkReply*, reply));

        QCOMPARE(spy.count(), 1);
        QList<QVariant> args = spy.takeFirst();
        QCOMPARE(args.at(0).toString(), QString("123"));
        QCOMPARE(args.at(1).toString(), QString("user"));
    }

    void testVerificationDispatch() {
        GitHubClient client;
        QSignalSpy spy(&client, &GitHubClient::tokenVerified);

        QByteArray json = "{\"login\":\"user\"}";
        MockNetworkReply *reply = new MockNetworkReply(json, &client);
        reply->setProperty("type", "verification");
        reply->setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 200);

        QMetaObject::invokeMethod(&client, "onReplyFinished", Qt::DirectConnection, Q_ARG(QNetworkReply*, reply));

        QCOMPARE(spy.count(), 1);
        QList<QVariant> args = spy.takeFirst();
        QCOMPARE(args.at(0).toBool(), true);
        QVERIFY(args.at(1).toString().contains("user"));
    }
};

QTEST_MAIN(TestGitHubClient)
#include "TestGitHubClient.moc"
