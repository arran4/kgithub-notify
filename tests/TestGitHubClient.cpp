#include <QtTest>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include "../src/GitHubClient.h"

// Mock Network Reply
class MockNetworkReply : public QNetworkReply {
    Q_OBJECT
public:
    MockNetworkReply(const QNetworkRequest &req, QNetworkAccessManager::Operation op, const QByteArray &content, QObject *parent = nullptr)
        : QNetworkReply(parent), m_content(content), m_offset(0) {
        setRequest(req);
        setOperation(op);
        setUrl(req.url());
        open(ReadOnly | Unbuffered);
    }

    void abort() override {}
    qint64 bytesAvailable() const override { return m_content.size() - m_offset + QNetworkReply::bytesAvailable(); }
    bool isSequential() const override { return true; }

    void setReplyError(QNetworkReply::NetworkError code, const QString &message) {
        setError(code, message);
    }

protected:
    qint64 readData(char *data, qint64 maxSize) override {
        if (m_offset >= m_content.size()) return -1;
        qint64 count = qMin(maxSize, (qint64)m_content.size() - m_offset);
        memcpy(data, m_content.constData() + m_offset, count);
        m_offset += count;
        return count;
    }

private:
    QByteArray m_content;
    qint64 m_offset;
};

// Mock Network Access Manager
class MockNetworkAccessManager : public QNetworkAccessManager {
    Q_OBJECT
public:
    struct RequestInfo {
        QNetworkAccessManager::Operation op;
        QNetworkRequest request;
    };
    QList<RequestInfo> requests;

    QByteArray nextResponseContent;
    QNetworkReply::NetworkError nextError = QNetworkReply::NoError;
    int requestCount = 0;

    MockNetworkAccessManager(QObject *parent = nullptr) : QNetworkAccessManager(parent) {}

protected:
    QNetworkReply* createRequest(QNetworkAccessManager::Operation op, const QNetworkRequest &req, QIODevice *outgoingData = nullptr) override {
        Q_UNUSED(outgoingData);
        requestCount++;
        requests.append({op, req});

        MockNetworkReply *reply = new MockNetworkReply(req, op, nextResponseContent, this);
        if (nextError != QNetworkReply::NoError) {
            reply->setReplyError(nextError, "Simulated Error");
        }

        // Schedule finish signal
        QTimer::singleShot(0, reply, &QNetworkReply::finished);

        return reply;
    }
};

class TestGitHubClient : public QObject {
    Q_OBJECT
private slots:
    void testMarkAsRead() {
        GitHubClient client;
        client.setToken("test_token");

        MockNetworkAccessManager *mockManager = new MockNetworkAccessManager(&client);

        // Replace the real manager with our mock
        delete client.manager;
        client.manager = mockManager;

        // Connect the new manager
        connect(mockManager, &QNetworkAccessManager::finished, &client, &GitHubClient::onReplyFinished);

        // Setup initial state
        QSignalSpy spyCheck(&client, &GitHubClient::loadingStarted); // CheckNotifications emits loadingStarted

        // 1. Call markAsRead
        client.markAsRead("123");

        // Wait for event loop to process the mock reply
        QTest::qWait(100);

        // Verify we have 2 requests (PATCH then GET)
        QCOMPARE(mockManager->requests.size(), 2);

        // Verify PATCH request
        const auto &req1 = mockManager->requests[0];
        QCOMPARE(req1.op, QNetworkAccessManager::CustomOperation);
        QCOMPARE(req1.request.url().toString(), QString("https://api.github.com/notifications/threads/123"));
        QCOMPARE(req1.request.attribute(QNetworkRequest::CustomVerbAttribute).toString(), QString("PATCH"));

        // Verify the second request (GET /notifications)
        const auto &req2 = mockManager->requests[1];
        QCOMPARE(req2.op, QNetworkAccessManager::GetOperation);
        QCOMPARE(req2.request.url().toString(), QString("https://api.github.com/notifications"));

        // Also verify loadingStarted signal was emitted
        QCOMPARE(spyCheck.count(), 1);
    }
};

QTEST_MAIN(TestGitHubClient)
#include "TestGitHubClient.moc"
