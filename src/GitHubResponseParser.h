#ifndef GITHUBRESPONSEPARSER_H
#define GITHUBRESPONSEPARSER_H

#include <QByteArray>
#include <QList>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QString>
#include "GitHubClient.h"

struct ParsedNotificationDetails {
    QString authorName;
    QString avatarUrl;
    QString htmlUrl;
};

struct ParsedVerification {
    bool valid;
    QString message;
};

class GitHubResponseParser {
public:
    static QList<Notification> parseNotifications(const QByteArray& data, QString& error);
    static ParsedNotificationDetails parseDetails(const QByteArray& data, QString& error);
    static ParsedVerification parseUserVerification(const QByteArray& data, int statusCode, QString& error);
};

#endif // GITHUBRESPONSEPARSER_H
