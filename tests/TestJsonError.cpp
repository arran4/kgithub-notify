#include <QTest>
#include <QProcess>
#include <QSignalSpy>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include "../src/GitHubClient.h"

class TestJsonError : public QObject {
    Q_OBJECT

public:
    TestJsonError() : serverProcess(nullptr) {}

private slots:
    void initTestCase() {
        // Create a python script that returns a JSON object instead of an array
        QFile scriptFile("server_json_error.py");
        if (scriptFile.open(QIODevice::WriteOnly)) {
            QTextStream out(&scriptFile);
            out << "import http.server, socketserver, sys\n"
                << "class Handler(http.server.SimpleHTTPRequestHandler):\n"
                << "    def do_GET(self):\n"
                << "        self.send_response(200)\n"
                << "        self.send_header('Content-type', 'application/json')\n"
                << "        self.end_headers()\n"
                << "        self.wfile.write(b'{\"error\": \"not an array\"}')\n"
                << "with socketserver.TCPServer(('', 0), Handler) as httpd:\n"
                << "    print(httpd.server_address[1], flush=True)\n"
                << "    httpd.serve_forever()\n";
            scriptFile.close();
        }

        serverProcess = new QProcess(this);
        serverProcess->start("python3", QStringList() << "-u" << "server_json_error.py");

        QVERIFY(serverProcess->waitForStarted());

        // Wait for port number
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
        QFile::remove("server_json_error.py");
    }

    void testInvalidJsonResponse() {
        GitHubClient client;
        client.setToken("dummy_token");
        client.setApiUrl(QString("http://localhost:%1").arg(serverPort));

        QSignalSpy spy(&client, &GitHubClient::errorOccurred);
        client.checkNotifications();

        if (!spy.wait(5000)) {
             qDebug() << "Timeout waiting for signal. Server output:" << serverProcess->readAllStandardError();
        }

        QVERIFY2(spy.count() == 1, "Expected errorOccurred signal");
        QCOMPARE(spy.takeFirst().at(0).toString(), QString("Invalid JSON response (expected array)"));
    }

private:
    QProcess *serverProcess;
    int serverPort;
};

QTEST_MAIN(TestJsonError)
#include "TestJsonError.moc"
