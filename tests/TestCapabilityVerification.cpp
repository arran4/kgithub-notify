#include <QSignalSpy>
#include <QtTest>

#include "../src/GitHubClient.h"
#include "FakeNetworkAccessManager.h"

class TestGitHubClient : public QObject {
    Q_OBJECT

   private slots:
    void permission403WithRateHeadersIsUnavailable() {
        GitHubClient client;
        qRegisterMetaType<TokenCapabilities>();
        QSignalSpy spy(&client, &GitHubClient::tokenVerified);

        QNetworkAccessManager* oldManager = client.manager;
        auto* manager = new FakeNetworkAccessManager(&client);
        manager->autoEmitFinished = false;
        client.manager = manager;
        oldManager->deleteLater();

        client.setToken("dummy_token");
        client.verifyToken();
        QCoreApplication::processEvents();

        manager->requests[0].reply->complete("{\"login\":\"user\"}");

        QTRY_COMPARE(manager->requests.size(), 2);
        manager->requests[1].reply->setRawHeader("X-RateLimit-Remaining", "4999");
        manager->requests[1].reply->completeWithError(QNetworkReply::ContentAccessDenied, "Forbidden", 403);

        QTRY_COMPARE(manager->requests.size(), 3);
        manager->requests[2].reply->setRawHeader("X-RateLimit-Remaining", "4998");
        manager->requests[2].reply->completeWithError(QNetworkReply::ContentAccessDenied, "Forbidden", 403);

        QTRY_COMPARE(spy.count(), 1);
        const QList<QVariant> args = spy.takeFirst();
        QCOMPARE(args.at(1).toBool(), true);

        const TokenCapabilities caps = args.at(2).value<TokenCapabilities>();
        QCOMPARE(caps.hasRepoMetadata, CapabilityStatus::Unavailable);
        QCOMPARE(caps.hasNotifications, CapabilityStatus::Unavailable);
    }
};

QTEST_MAIN(TestGitHubClient)
#include "TestCapabilityVerification.moc"
