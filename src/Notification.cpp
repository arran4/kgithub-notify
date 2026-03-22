#include "Notification.h"

QJsonObject Notification::toJson() const {
    QJsonObject obj;
    obj["id"] = id;
    obj["title"] = title;
    obj["type"] = type;
    obj["repository"] = repository;
    obj["url"] = url;
    obj["htmlUrl"] = htmlUrl;
    obj["updatedAt"] = updatedAt;
    obj["lastReadAt"] = lastReadAt;
    obj["reason"] = reason;
    obj["unread"] = unread;
    obj["rawJson"] = rawJson;
    return obj;
}

Notification Notification::fromJson(const QJsonObject &obj) {
    Notification n;
    n.id = obj["id"].toString();
    n.title = obj["title"].toString();
    n.type = obj["type"].toString();
    n.repository = obj["repository"].toString();
    n.url = obj["url"].toString();
    n.htmlUrl = obj["htmlUrl"].toString();
    n.updatedAt = obj["updatedAt"].toString();
    n.lastReadAt = obj["lastReadAt"].toString();
    n.reason = obj["reason"].toString();
    n.unread = obj["unread"].toBool();
    n.rawJson = obj["rawJson"].toObject();
    return n;
}
