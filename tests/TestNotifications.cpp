#include <QTest>
#include <QProcess>
#include <QSignalSpy>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include "../src/GitHubClient.h"

// Ensure QList<Notification> is registered for QSignalSpy
Q_DECLARE_METATYPE(QList<Notification>)

class TestNotifications : public QObject {
    Q_OBJECT

public:
    TestNotifications() : serverProcess(nullptr) {}

private slots:
    void initTestCase() {
        qRegisterMetaType<QList<Notification>>("QList<Notification>");

        // Create a python script that returns a JSON array of notifications
        QFile scriptFile("mock_notifications_server.py");
        if (scriptFile.open(QIODevice::WriteOnly)) {
            QTextStream out(&scriptFile);
            out << "import http.server, socketserver, json, sys\n"
                << "class Handler(http.server.BaseHTTPRequestHandler):\n"
                << "    def do_GET(self):\n"
                << "        if self.path == '/notifications':\n"
                << "            self.send_response(200)\n"
                << "            self.send_header('Content-type', 'application/json')\n"
                << "            self.end_headers()\n"
                << "            response = [{\n"
                << "                'id': '1',\n"
                << "                'unread': True,\n"
                << "                'reason': 'subscribed',\n"
                << "                'updated_at': '2023-10-26T00:00:00Z',\n"
                << "                'last_read_at': None,\n"
                << "                'subject': {\n"
                << "                    'title': 'Test Issue',\n"
                << "                    'url': 'https://api.github.com/repos/test/repo/issues/1',\n"
                << "                    'latest_comment_url': 'https://api.github.com/repos/test/repo/issues/comments/1',\n"
                << "                    'type': 'Issue'\n"
                << "                },\n"
                << "                'repository': {\n"
                << "                    'id': 1296269,\n"
                << "                    'name': 'repo',\n"
                << "                    'full_name': 'test/repo',\n"
                << "                    'owner': {\n"
                << "                        'login': 'test',\n"
                << "                        'id': 1,\n"
                << "                        'avatar_url': 'https://github.com/images/error/octocat_happy.gif',\n"
                << "                        'url': 'https://api.github.com/users/test',\n"
                << "                    }\n"
                << "                },\n"
                << "                'url': 'https://api.github.com/notifications/threads/1',\n"
                << "                'subscription_url': 'https://api.github.com/notifications/threads/1/subscription'\n"
                << "            }]\n"
                << "            self.wfile.write(json.dumps(response).encode())\n"
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
        serverProcess->start("python3", QStringList() << "-u" << "mock_notifications_server.py");

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
        QFile::remove("mock_notifications_server.py");
    }

    void testCheckNotificationsSuccess() {
        GitHubClient client;
        client.setToken("dummy_token");
        client.setApiUrl(QString("http://localhost:%1").arg(serverPort));

        QSignalSpy spy(&client, &GitHubClient::notificationsReceived);

        // Also spy on errorOccurred just in case
        QSignalSpy errorSpy(&client, &GitHubClient::errorOccurred);

        client.checkNotifications();

        if (!spy.wait(5000)) {
             qDebug() << "Timeout waiting for notificationsReceived.";
             if (errorSpy.count() > 0) {
                 qDebug() << "Error occurred:" << errorSpy.takeFirst().at(0).toString();
             }
        }

        QVERIFY2(spy.count() == 1, "Expected notificationsReceived signal");

        QList<QVariant> args = spy.takeFirst();
        QList<Notification> notifications = args.at(0).value<QList<Notification>>();

        QCOMPARE(notifications.size(), 1);

        Notification n = notifications.first();
        QCOMPARE(n.id, QString("1"));
        QCOMPARE(n.title, QString("Test Issue"));
        QCOMPARE(n.type, QString("Issue"));
        QCOMPARE(n.repository, QString("test/repo"));
        // API URL from mock data
        QCOMPARE(n.url, QString("https://api.github.com/repos/test/repo/issues/1"));
        QCOMPARE(n.updatedAt, QString("2023-10-26T00:00:00Z"));
        QCOMPARE(n.unread, true);
    }

private:
    QProcess *serverProcess;
    int serverPort;
};

QTEST_MAIN(TestNotifications)
#include "TestNotifications.moc"
