#include "GitHubResponseParser.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QList>

QList<Notification> GitHubResponseParser::parseNotifications(const QByteArray& data, QString& error) {
    QJsonDocument doc = QJsonDocument::fromJson(data);

    if (!doc.isArray()) {
        error = "Invalid JSON response (expected array)";
        return {};
    }

    QJsonArray array = doc.array();
    QList<Notification> notifications;

    for (const QJsonValue &value : array) {
        if (!value.isObject()) continue;

        QJsonObject obj = value.toObject();
        Notification n;
        n.id = obj["id"].toString();

        QJsonObject subject = obj["subject"].toObject();
        n.title = subject["title"].toString();
        n.type = subject["type"].toString();
        n.url = subject["url"].toString(); // API URL
        n.htmlUrl = GitHubClient::apiToHtmlUrl(n.url);

        QJsonObject repo = obj["repository"].toObject();
        n.repository = repo["full_name"].toString();

        n.updatedAt = obj["updated_at"].toString();
        n.unread = obj["unread"].toBool();

        notifications.append(n);
    }
    return notifications;
}

ParsedNotificationDetails GitHubResponseParser::parseDetails(const QByteArray& data, QString& error) {
    QJsonDocument doc = QJsonDocument::fromJson(data);
    ParsedNotificationDetails details;

    if (!doc.isObject()) {
        error = "Invalid JSON response (expected object)";
        return details;
    }

    QJsonObject obj = doc.object();
    if (obj.contains("user")) {
        QJsonObject user = obj["user"].toObject();
        details.authorName = user["login"].toString();
        details.avatarUrl = user["avatar_url"].toString();
    } else if (obj.contains("author")) {
        QJsonObject author = obj["author"].toObject();
        details.authorName = author["login"].toString();
        details.avatarUrl = author["avatar_url"].toString();
    }

    details.htmlUrl = obj["html_url"].toString();
    return details;
}

ParsedVerification GitHubResponseParser::parseUserVerification(const QByteArray& data, int statusCode, QString& error) {
    ParsedVerification result = {false, ""};

    if (statusCode == 200) {
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            QString login = obj["login"].toString();
            result.valid = true;
            result.message = "Token valid for user: " + login;
        } else {
             // Fallback if JSON is weird but status is 200
             result.valid = true;
             result.message = "Token valid";
        }
    } else if (statusCode == 401) {
        result.valid = false;
        result.message = "Invalid Token";
    } else {
        result.valid = false;
        result.message = error.isEmpty() ? QString("HTTP Error %1").arg(statusCode) : error;
    }

    return result;
}
