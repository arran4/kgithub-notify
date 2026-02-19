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
};

QTEST_MAIN(TestGitHubClient)
#include "TestMain.moc"
