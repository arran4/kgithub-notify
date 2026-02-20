#include <QTest>
#include <QSignalSpy>
#include <QSettings>
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
        QVERIFY(true); // Just verifying it constructs without crash
    }

    void testTokenHandling() {
        // Since setToken is a setter without getter, we can't easily verify state without modifying class.
        // However, we can verify SettingsDialog persistence.

        QSettings settings("Kgithub-notify-test", "Kgithub-notify-test");
        settings.clear();

        QString testToken = "ghp_test12345";
        settings.setValue("token", testToken);

        // Check if SettingsDialog reads it correctly
        QCOMPARE(SettingsDialog::getToken(), testToken);
    }

    void testUrlConversion() {
        // Issue
        QString issueApi = "https://api.github.com/repos/owner/repo/issues/1";
        QString issueHtml = "https://github.com/owner/repo/issues/1";
        QCOMPARE(GitHubClient::apiToHtmlUrl(issueApi), issueHtml);

        // Pull Request
        QString prApi = "https://api.github.com/repos/owner/repo/pulls/1";
        QString prHtml = "https://github.com/owner/repo/pull/1";
        QCOMPARE(GitHubClient::apiToHtmlUrl(prApi), prHtml);

        // Commit
        QString commitApi = "https://api.github.com/repos/owner/repo/commits/6dcb09b5b57875f334f61aebed695e2e4193db5e";
        QString commitHtml = "https://github.com/owner/repo/commit/6dcb09b5b57875f334f61aebed695e2e4193db5e";
        QCOMPARE(GitHubClient::apiToHtmlUrl(commitApi), commitHtml);

        // General Repo URL (if ever used)
        QString repoApi = "https://api.github.com/repos/owner/repo";
        QString repoHtml = "https://github.com/owner/repo";
        QCOMPARE(GitHubClient::apiToHtmlUrl(repoApi), repoHtml);
    }
};

QTEST_MAIN(TestGitHubClient)
#include "TestMain.moc"
