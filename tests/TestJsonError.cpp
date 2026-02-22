#include <QTest>
#include <QSignalSpy>
#include <QDebug>
#include <QTcpServer>
#include <QTcpSocket>

#include "../src/GitHubClient.h"

class TestJsonError : public QObject {
    Q_OBJECT

public:
    TestJsonError() : server(nullptr) {}

private slots:
    void initTestCase() {
        server = new QTcpServer(this);
        QVERIFY(server->listen(QHostAddress::LocalHost));
        serverPort = server->serverPort();
        connect(server, &QTcpServer::newConnection, this, &TestJsonError::handleNewConnection);
    }

    void handleNewConnection() {
        QTcpSocket *socket = server->nextPendingConnection();
        connect(socket, &QTcpSocket::readyRead, [this, socket]() {
            socket->readAll();

            // Send JSON object instead of array
            QByteArray response = "HTTP/1.1 200 OK\r\n"
                                  "Content-Type: application/json\r\n"
                                  "Content-Length: 25\r\n"
                                  "\r\n"
                                  "{\"error\": \"not an array\"}";
            socket->write(response);
            socket->disconnectFromHost();
        });
        connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
    }

    void cleanupTestCase() {
        server->close();
    }

    void testInvalidJsonResponse() {
        GitHubClient client;
        client.setToken("dummy_token");
        client.setApiUrl(QString("http://localhost:%1").arg(serverPort));

        QSignalSpy spy(&client, &GitHubClient::errorOccurred);
        client.checkNotifications();

        if (!spy.wait(5000)) {
             qDebug() << "Timeout waiting for signal";
        }

        QVERIFY2(spy.count() == 1, "Expected errorOccurred signal");
        QCOMPARE(spy.takeFirst().at(0).toString(), QString("Invalid JSON response (expected array)"));
    }

private:
    QTcpServer *server;
    int serverPort;
};

QTEST_MAIN(TestJsonError)
#include "TestJsonError.moc"
