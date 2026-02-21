#ifndef NOTIFICATIONTEXTFORMATTER_H
#define NOTIFICATIONTEXTFORMATTER_H

#include <QString>
#include <QVariant> // For Qt::TextFormat
#include <QTextFormat> // For Qt::TextFormat enum
#include "GitHubClient.h"

struct TextWithFormat {
    QString text;
    Qt::TextFormat format;
};

class NotificationTextFormatter {
public:
    static TextWithFormat formatTitle(const Notification& n);
    static TextWithFormat formatType(const Notification& n);
    static TextWithFormat formatRepo(const Notification& n);
    static TextWithFormat formatUrl(const Notification& n);
    static TextWithFormat formatUrlStr(const QString& htmlUrl);
    static TextWithFormat formatDate(const Notification& n);
};

#endif // NOTIFICATIONTEXTFORMATTER_H
