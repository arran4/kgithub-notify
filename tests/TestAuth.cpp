#include <QSignalSpy>
#include <QTest>
#include <QTcpServer>
#include <QTcpSocket>

#include "../src/GitHubClient.h"

class TestAuth : public QObject {
    Q_OBJECT

   private slots:
    void initTestCase() {
        server = new QTcpServer(this);
        QVERIFY(server->listen(QHostAddress::LocalHost));
        serverPort = server->serverPort();
        connect(server, &QTcpServer::newConnection, this, &TestAuth::handleNewConnection);
    }

    void handleNewConnection() {
        QTcpSocket *socket = server->nextPendingConnection();
        connect(socket, &QTcpSocket::readyRead, [this, socket]() {
            // Read request
            socket->readAll();

            // Send 401 response
            QByteArray response = "HTTP/1.1 401 Unauthorized\r\n"
                                  "Content-Length: 0\r\n"
                                  "\r\n";
            socket->write(response);
            socket->disconnectFromHost();
        });
        connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
    }

    void cleanupTestCase() {
        server->close();
    }

    void testAuthError401() {
        GitHubClient client;
        client.setToken("invalid_token");
        client.setApiUrl(QString("http://localhost:%1").arg(serverPort));

        QSignalSpy spy(&client, &GitHubClient::authError);
        QSignalSpy errorSpy(&client, &GitHubClient::errorOccurred);

        client.checkNotifications();

        if (!spy.wait(5000)) {
            if (errorSpy.count() > 0) {
                qDebug() << "Error occurred:" << errorSpy.takeFirst().at(0).toString();
            } else {
                qDebug() << "Timeout waiting for signal";
            }
        }

        QVERIFY2(spy.count() == 1, "Expected authError signal");
        QCOMPARE(spy.takeFirst().at(0).toString(), QString("Invalid Token"));
    }

    void testNoTokenError() {
        GitHubClient client;
        // No token set

        QSignalSpy spy(&client, &GitHubClient::authError);
        client.checkNotifications();

        // Should be immediate (sync)
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(0).toString(), QString("No token provided"));
    }

   private:
    QTcpServer *server;
    int serverPort;
};

QTEST_MAIN(TestAuth)
#include "TestAuth.moc"
