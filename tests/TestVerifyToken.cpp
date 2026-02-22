#include <QSignalSpy>
#include <QTest>
#include <QTcpServer>
#include <QTcpSocket>

#include "../src/GitHubClient.h"

class TestVerifyToken : public QObject {
    Q_OBJECT

   private slots:
    void initTestCase() {
        server = new QTcpServer(this);
        QVERIFY(server->listen(QHostAddress::LocalHost));
        serverPort = server->serverPort();
        connect(server, &QTcpServer::newConnection, this, &TestVerifyToken::handleNewConnection);
    }

    void handleNewConnection() {
        QTcpSocket *socket = server->nextPendingConnection();
        if (!socket) return;
        connect(socket, &QTcpSocket::readyRead, [this, socket]() {
            // Read request to verify path
            QByteArray request = socket->readAll();
            QString requestStr(request);

            // Minimal response with proper headers
            // Connection: close is important for QNetworkAccessManager to finish the request quickly in tests
            if (requestStr.contains("GET /user")) {
                QByteArray response = "HTTP/1.1 200 OK\r\n"
                                      "Content-Type: application/json\r\n"
                                      "Connection: close\r\n"
                                      "\r\n"
                                      "{\"login\": \"testuser\"}";
                socket->write(response);
            } else {
                QByteArray response = "HTTP/1.1 404 Not Found\r\n"
                                      "Content-Length: 0\r\n"
                                      "Connection: close\r\n"
                                      "\r\n";
                socket->write(response);
            }
            socket->flush();
            socket->disconnectFromHost();
        });
        connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
    }

    void cleanupTestCase() {
        server->close();
    }

    void testVerifyTokenSuccess() {
        GitHubClient client;
        client.setToken("valid_token");
        QString url = QString("http://localhost:%1").arg(serverPort);
        client.setApiUrl(url);

        QSignalSpy spy(&client, &GitHubClient::tokenVerified);

        client.verifyToken();

        // Wait up to 5s, but it should be much faster
        QVERIFY2(spy.wait(5000), "Timeout waiting for tokenVerified signal");
        QCOMPARE(spy.count(), 1);

        QList<QVariant> args = spy.takeFirst();
        bool valid = args.at(0).toBool();
        QString message = args.at(1).toString();

        if (!valid) {
             qDebug() << "Verification failed with message:" << message;
        }

        QVERIFY2(valid, "Token should be verified as valid");
        QVERIFY2(message.contains("testuser"), "Message should contain username 'testuser'");
    }

   private:
    QTcpServer *server;
    int serverPort;
};

QTEST_MAIN(TestVerifyToken)
#include "TestVerifyToken.moc"
