#include <QSignalSpy>
#include <QTest>

#include "../src/GitHubClient.h"
#include "../src/SettingsDialog.h"

class TestGitHubClient : public QObject {
    Q_OBJECT

   private slots:
    void initTestCase() {
        // Setup settings for testing
        QCoreApplication::setOrganizationName("Kgithub-notify-test");
        QCoreApplication::setApplicationName("Kgithub-notify-test");
    }

    void testConstruction() {
        GitHubClient client;
        QVERIFY(true);  // Just verifying it constructs without crash
    }


    void testUrlConversion() {
        // Issue
        QString issueApi = "https://api.github.com/repos/owner/repo/issues/123";
        QString issueHtml = GitHubClient::apiToHtmlUrl(issueApi);
        QCOMPARE(issueHtml, QString("https://github.com/owner/repo/issues/123"));

        // PR
        QString prApi = "https://api.github.com/repos/owner/repo/pulls/123";
        QString prHtml = GitHubClient::apiToHtmlUrl(prApi);
        QCOMPARE(prHtml, QString("https://github.com/owner/repo/pull/123"));

        // Commit
        QString commitApi = "https://api.github.com/repos/owner/repo/commits/sha123";
        QString commitHtml = GitHubClient::apiToHtmlUrl(commitApi);
        QCOMPARE(commitHtml, QString("https://github.com/owner/repo/commit/sha123"));

        // With ID
        QString id = "NT_kwDOAa";
        QString issueHtmlWithId = GitHubClient::apiToHtmlUrl(issueApi, id);
        // The URL encoding might vary, but it should contain the query
        QVERIFY(issueHtmlWithId.contains("notification_referrer_id=" + id));
        QVERIFY(issueHtmlWithId.startsWith("https://github.com/owner/repo/issues/123"));
    }
};

QTEST_MAIN(TestGitHubClient)
#include "TestMain.moc"
