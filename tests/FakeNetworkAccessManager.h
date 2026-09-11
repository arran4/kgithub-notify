#ifndef FAKENETWORKACCESSMANAGER_H
#define FAKENETWORKACCESSMANAGER_H

#include <QList>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <cstring>

class ControlledFakeReply : public QNetworkReply {
    Q_OBJECT
   public:
    ControlledFakeReply(const QNetworkRequest& request, QNetworkAccessManager::Operation operation, QObject* parent)
        : QNetworkReply(parent) {
        setRequest(request);
        setUrl(request.url());
        setOperation(operation);
        setOpenMode(QIODevice::ReadOnly);
    }

    void abort() override { completeWithError(OperationCanceledError, "Request cancelled"); }

    qint64 readData(char* data, qint64 maxlen) override {
        qint64 len = qMin(maxlen, static_cast<qint64>(m_data.size()));
        memcpy(data, m_data.constData(), len);
        m_data.remove(0, len);
        return len;
    }

    qint64 bytesAvailable() const override { return m_data.size() + QNetworkReply::bytesAvailable(); }

    void setReplyData(const QByteArray& data) { m_data = data; }

    void complete(const QByteArray& data, int httpStatus = 200) {
        if (isFinished()) return;
        setFinished(true);
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, httpStatus);
        setReplyData(data);
        emit readyRead();
        emit finished();
    }

    void completeWithError(NetworkError code, const QString& errorString, int httpStatus = 0) {
        if (isFinished()) return;
        setFinished(true);
        if (httpStatus != 0) {
            setAttribute(QNetworkRequest::HttpStatusCodeAttribute, httpStatus);
        } else if (code == AuthenticationRequiredError) {
            setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 401);
        }
        setError(code, errorString);
        emit errorOccurred(code);
        emit finished();
    }

    using QNetworkReply::setRawHeader;

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
    bool autoEmitFinished = true;  // backward compatibility

   protected:
    QNetworkReply* createRequest(Operation op, const QNetworkRequest& request,
                                 QIODevice* outgoingData = nullptr) override {
        Q_UNUSED(outgoingData);
        ControlledFakeReply* reply = new ControlledFakeReply(request, op, this);
        requests.append({op, request, reply});
        if (autoEmitFinished) {
            QMetaObject::invokeMethod(reply, [reply]() { reply->complete(QByteArray()); }, Qt::QueuedConnection);
        }
        return reply;
    }
};

#endif  // FAKENETWORKACCESSMANAGER_H
