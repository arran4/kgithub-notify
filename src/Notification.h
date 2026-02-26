#ifndef NOTIFICATION_H
#define NOTIFICATION_H

#include <QString>
#include <QJsonObject>

struct Notification {
    QString id;
    QString title;
    QString type;
    QString repository;
    QString url; // API URL
    QString htmlUrl; // HTML URL (cached)
    QString updatedAt;
    QString lastReadAt;
    bool unread;

    QJsonObject toJson() const;
    static Notification fromJson(const QJsonObject &obj);
};

#endif // NOTIFICATION_H
