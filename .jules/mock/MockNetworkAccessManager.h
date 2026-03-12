#ifndef MOCKNETWORKACCESSMANAGER_H
#define MOCKNETWORKACCESSMANAGER_H

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QJsonDocument>

// A simple mock reply to inject mock data without real network IO.
class MockReply : public QNetworkReply {
    Q_OBJECT
public:
    MockReply(QObject *parent, const QByteArray &data, const QNetworkRequest &req);
    ~MockReply() override;

    void abort() override {}
    qint64 bytesAvailable() const override;
    bool isSequential() const override { return true; }

protected:
    qint64 readData(char *data, qint64 maxlen) override;

private:
    QByteArray m_data;
    qint64 m_offset;
};

class MockNetworkAccessManager : public QNetworkAccessManager {
    Q_OBJECT
public:
    MockNetworkAccessManager(QObject *parent = nullptr);

protected:
    QNetworkReply *createRequest(Operation op, const QNetworkRequest &request, QIODevice *outgoingData) override;
};

#endif // MOCKNETWORKACCESSMANAGER_H
