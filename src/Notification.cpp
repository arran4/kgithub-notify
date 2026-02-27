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
    obj["unread"] = unread;
    obj["inInbox"] = inInbox;
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
    n.unread = obj["unread"].toBool();
    n.inInbox = obj["inInbox"].toBool();
    return n;
}
