#include <QProcess>
#include <QSignalSpy>
#include <QTest>

#include "../src/GitHubClient.h"

class TestAuth : public QObject {
    Q_OBJECT

   private slots:
    void initTestCase() {
        // Write python script
        QFile scriptFile("server.py");
        if (scriptFile.open(QIODevice::WriteOnly)) {
            QTextStream out(&scriptFile);
            out << "import http.server, socketserver, sys\n"
                << "class Handler(http.server.SimpleHTTPRequestHandler):\n"
                << "    def do_GET(self):\n"
                << "        self.send_response(401)\n"
                << "        self.end_headers()\n"
                << "with socketserver.TCPServer(('', 0), Handler) as httpd:\n"
                << "    print(httpd.server_address[1], flush=True)\n"
                << "    httpd.serve_forever()\n";
            scriptFile.close();
        }

        // Start python server
        serverProcess = new QProcess(this);
        serverProcess->start("python3", QStringList() << "-u" << "server.py");

        QVERIFY(serverProcess->waitForStarted());
        if (!serverProcess->waitForReadyRead(5000)) {
            qDebug() << "Server failed to start. Stderr:" << serverProcess->readAllStandardError();
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

    void testAuthError401() {
        GitHubClient client;
        client.setToken("invalid_token");  // Must set token so it doesn't fail with "No token provided"
        client.setApiUrl(QString("http://localhost:%1").arg(serverPort));

        QSignalSpy spy(&client, &GitHubClient::authError);
        QSignalSpy errorSpy(&client, &GitHubClient::errorOccurred);

        client.checkNotifications();

        if (!spy.wait(5000)) {
            if (errorSpy.count() > 0) {
                qDebug() << "Error occurred:" << errorSpy.takeFirst().at(0).toString();
            } else {
                qDebug() << "Timeout waiting for signal. Server output:" << serverProcess->readAllStandardError();
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
    QProcess *serverProcess;
    int serverPort;
};

QTEST_MAIN(TestAuth)
#include "TestAuth.moc"
