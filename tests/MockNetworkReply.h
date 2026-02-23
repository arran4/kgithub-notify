#ifndef MOCKNETWORKREPLY_H
#define MOCKNETWORKREPLY_H

#include <QNetworkReply>
#include <QBuffer>

class MockNetworkReply : public QNetworkReply {
    Q_OBJECT
public:
    MockNetworkReply(const QByteArray &data, QObject *parent = nullptr) : QNetworkReply(parent) {
        setOpenMode(QIODevice::ReadOnly);
        m_buffer.setData(data);
        m_buffer.open(QIODevice::ReadOnly);
    }

    void abort() override {}

    qint64 readData(char *data, qint64 maxlen) override {
        return m_buffer.read(data, maxlen);
    }

    qint64 bytesAvailable() const override {
        return m_buffer.bytesAvailable() + QNetworkReply::bytesAvailable();
    }

    void setAttribute(QNetworkRequest::Attribute code, const QVariant &value) {
        QNetworkReply::setAttribute(code, value);
    }

    void setError(NetworkError code, const QString &errorString) {
        QNetworkReply::setError(code, errorString);
    }

private:
    QBuffer m_buffer;
};

#endif // MOCKNETWORKREPLY_H
