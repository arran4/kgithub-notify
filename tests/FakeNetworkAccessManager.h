#ifndef FAKENETWORKACCESSMANAGER_H
#define FAKENETWORKACCESSMANAGER_H

#include <QList>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

class FakeNetworkReply : public QNetworkReply {
    Q_OBJECT
   public:
    explicit FakeNetworkReply(QObject* parent = nullptr) : QNetworkReply(parent) {
        setOpenMode(QIODevice::ReadOnly);
        QMetaObject::invokeMethod(this, "finished", Qt::QueuedConnection);
    }
    void abort() override {}
    qint64 readData(char*, qint64) override { return 0; }
};

class FakeNetworkAccessManager : public QNetworkAccessManager {
    Q_OBJECT
   public:
    explicit FakeNetworkAccessManager(QObject* parent = nullptr) : QNetworkAccessManager(parent) {}

    struct RequestRecord {
        Operation op;
        QNetworkRequest request;
    };
    QList<RequestRecord> requests;

   protected:
    QNetworkReply* createRequest(Operation op, const QNetworkRequest& request,
                                 QIODevice* outgoingData = nullptr) override {
        Q_UNUSED(outgoingData);
        requests.append({op, request});
        return new FakeNetworkReply(this);
    }
};

#endif  // FAKENETWORKACCESSMANAGER_H
