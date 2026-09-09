#ifndef FAKENETWORKACCESSMANAGER_H
#define FAKENETWORKACCESSMANAGER_H

#include <QList>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include "MockNetworkReply.h" // We will reuse or just modify FakeNetworkReply. Wait, let's just make FakeNetworkReply controllable.

class ControlledFakeReply : public QNetworkReply {
    Q_OBJECT
   public:
    explicit ControlledFakeReply(QObject* parent = nullptr) : QNetworkReply(parent) {
        setOpenMode(QIODevice::ReadOnly);
        // Do NOT immediately queue finished.
    }

    void abort() override {}

    qint64 readData(char* data, qint64 maxlen) override {
        qint64 len = qMin(maxlen, static_cast<qint64>(m_data.size()));
        memcpy(data, m_data.constData(), len);
        m_data.remove(0, len);
        return len;
    }

    qint64 bytesAvailable() const override {
        return m_data.size() + QNetworkReply::bytesAvailable();
    }

    void setReplyData(const QByteArray& data) {
        m_data = data;
    }

    void complete(const QByteArray& data, int httpStatus = 200) {
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, httpStatus);
        setReplyData(data);
        emit readyRead();
        emit finished();
    }

    void completeWithError(NetworkError code, const QString& errorString) {
        setError(code, errorString);
        emit finished();
    }

    QByteArray m_data;
};

class FakeNetworkAccessManager : public QNetworkAccessManager {
    Q_OBJECT
   public:
    explicit FakeNetworkAccessManager(QObject* parent = nullptr) : QNetworkAccessManager(parent) {}

    struct RequestRecord {
        Operation op;
        QNetworkRequest request;
        ControlledFakeReply* reply;
    };
    QList<RequestRecord> requests;
    bool autoEmitFinished = true; // backward compatibility

   protected:
    QNetworkReply* createRequest(Operation op, const QNetworkRequest& request,
                                 QIODevice* outgoingData = nullptr) override {
        Q_UNUSED(outgoingData);
        ControlledFakeReply* reply = new ControlledFakeReply(this);
        requests.append({op, request, reply});
        if (autoEmitFinished) {
            QMetaObject::invokeMethod(reply, "finished", Qt::QueuedConnection);
        }
        return reply;
    }
};

#endif  // FAKENETWORKACCESSMANAGER_H
