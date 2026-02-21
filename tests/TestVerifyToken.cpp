#include <QProcess>
#include <QSignalSpy>
#include <QTest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QApplication>

#include "../src/GitHubClient.h"

class TestVerifyToken : public QObject {
    Q_OBJECT

   private slots:
    void initTestCase() {
        // Setup simple HTTP server in Python
        QFile scriptFile("verify_server.py");
        if (scriptFile.open(QIODevice::WriteOnly)) {
            QTextStream out(&scriptFile);
            out << "import http.server, socketserver, json\n"
                << "class Handler(http.server.BaseHTTPRequestHandler):\n"
                << "    def do_GET(self):\n"
                << "        if self.path == '/user':\n"
                << "            self.send_response(200)\n"
                << "            self.send_header('Content-type', 'application/json')\n"
                << "            self.end_headers()\n"
                << "            self.wfile.write(json.dumps({'login': 'testuser'}).encode())\n"
                << "        else:\n"
                << "            self.send_response(404)\n"
                << "            self.end_headers()\n"
                << "\n"
                << "with socketserver.TCPServer(('', 0), Handler) as httpd:\n"
                << "    print(httpd.server_address[1], flush=True)\n"
                << "    httpd.serve_forever()\n";
            scriptFile.close();
        }

        serverProcess = new QProcess(this);
        serverProcess->start("python3", QStringList() << "-u" << "verify_server.py");

        QVERIFY(serverProcess->waitForStarted());
        if (!serverProcess->waitForReadyRead(5000)) {
            QFAIL("Server failed to start");
        }
        QByteArray portData = serverProcess->readLine().trimmed();
        serverPort = portData.toInt();
        QVERIFY(serverPort > 0);
    }

    void cleanupTestCase() {
        if (serverProcess) {
            serverProcess->terminate();
            serverProcess->waitForFinished();
        }
    }

    void testVerifyTokenSuccess() {
        GitHubClient client;
        client.setToken("valid_token");
        QString url = QString("http://localhost:%1").arg(serverPort);
        client.setApiUrl(url);

        QSignalSpy spy(&client, &GitHubClient::tokenVerified);

        client.verifyToken();

        QVERIFY2(spy.wait(5000), "Timeout waiting for tokenVerified signal");
        QCOMPARE(spy.count(), 1);

        QList<QVariant> args = spy.takeFirst();
        bool valid = args.at(0).toBool();
        QString message = args.at(1).toString();

        QVERIFY2(valid, "Token should be verified as valid");
        QVERIFY2(message.contains("testuser"), "Message should contain username 'testuser'");
    }

   private:
    QProcess *serverProcess;
    int serverPort;
};

QTEST_MAIN(TestVerifyToken)
#include "TestVerifyToken.moc"
