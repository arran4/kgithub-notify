#include <QTest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QList>
#include "../src/GitHubResponseParser.h"

class TestGitHubResponseParser : public QObject {
    Q_OBJECT

private slots:
    void testParseNotifications() {
        QJsonArray jsonArray;
        QJsonObject notifObj;
        notifObj["id"] = "1";

        QJsonObject subject;
        subject["title"] = "Test Title";
        subject["type"] = "Issue";
        subject["url"] = "https://api.github.com/repos/owner/repo/issues/1";
        notifObj["subject"] = subject;

        QJsonObject repo;
        repo["full_name"] = "owner/repo";
        notifObj["repository"] = repo;

        notifObj["updated_at"] = "2023-01-01T00:00:00Z";
        notifObj["unread"] = true;

        jsonArray.append(notifObj);

        QJsonDocument doc(jsonArray);
        QByteArray data = doc.toJson();

        QString error;
        QList<Notification> notifications = GitHubResponseParser::parseNotifications(data, error);

        QVERIFY(error.isEmpty());
        QCOMPARE(notifications.size(), 1);
        QCOMPARE(notifications[0].title, "Test Title");
        QCOMPARE(notifications[0].type, "Issue");
        QCOMPARE(notifications[0].repository, "owner/repo");
    }

    void testParseNotificationsEmpty() {
        QByteArray data = "[]";
        QString error;
        QList<Notification> notifications = GitHubResponseParser::parseNotifications(data, error);
        QVERIFY(error.isEmpty());
        QVERIFY(notifications.isEmpty());
    }

    void testParseNotificationsInvalidJson() {
        QByteArray data = "{\"error\": \"not an array\"}";
        QString error;
        QList<Notification> notifications = GitHubResponseParser::parseNotifications(data, error);

        QVERIFY(!error.isEmpty());
        QCOMPARE(error, "Invalid JSON response (expected array)");
        QVERIFY(notifications.isEmpty());
    }

    void testParseDetails() {
        QJsonObject detailsObj;
        QJsonObject user;
        user["login"] = "testuser";
        user["avatar_url"] = "http://example.com/avatar.png";
        detailsObj["user"] = user;
        detailsObj["html_url"] = "http://example.com/issue/1";

        QJsonDocument doc(detailsObj);
        QByteArray data = doc.toJson();

        QString error;
        ParsedNotificationDetails details = GitHubResponseParser::parseDetails(data, error);

        QVERIFY(error.isEmpty());
        QCOMPARE(details.authorName, "testuser");
        QCOMPARE(details.avatarUrl, "http://example.com/avatar.png");
        QCOMPARE(details.htmlUrl, "http://example.com/issue/1");
    }

    void testParseUserVerificationSuccess() {
        QJsonObject userObj;
        userObj["login"] = "verifiedUser";

        QJsonDocument doc(userObj);
        QByteArray data = doc.toJson();

        QString error;
        ParsedVerification result = GitHubResponseParser::parseUserVerification(data, 200, error);

        QVERIFY(result.valid);
        QCOMPARE(result.message, "Token valid for user: verifiedUser");
    }

    void testParseUserVerification401() {
        QByteArray data = "";
        QString error = "Authentication Required";
        ParsedVerification result = GitHubResponseParser::parseUserVerification(data, 401, error);

        QVERIFY(!result.valid);
        QCOMPARE(result.message, "Invalid Token");
    }

    void testParseUserVerificationOtherError() {
        QByteArray data = "";
        QString error = "Not Found";
        ParsedVerification result = GitHubResponseParser::parseUserVerification(data, 404, error);

        QVERIFY(!result.valid);
        QCOMPARE(result.message, "Not Found");
    }
};

QTEST_MAIN(TestGitHubResponseParser)
#include "TestGitHubResponseParser.moc"
