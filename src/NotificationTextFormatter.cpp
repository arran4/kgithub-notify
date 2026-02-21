#include "NotificationTextFormatter.h"
#include <QDateTime>
#include <QLocale>

TextWithFormat NotificationTextFormatter::formatTitle(const Notification& n) {
    return { n.title, Qt::PlainText };
}

TextWithFormat NotificationTextFormatter::formatType(const Notification& n) {
    return { QString("Type: %1").arg(n.type), Qt::PlainText };
}

TextWithFormat NotificationTextFormatter::formatRepo(const Notification& n) {
    return { QString("Repo: <b>%1</b>").arg(n.repository.toHtmlEscaped()), Qt::RichText };
}

TextWithFormat NotificationTextFormatter::formatUrl(const Notification& n) {
    QString htmlUrl = GitHubClient::apiToHtmlUrl(n.url);
    return formatUrlStr(htmlUrl);
}

TextWithFormat NotificationTextFormatter::formatUrlStr(const QString& htmlUrl) {
    return { QString("<a href=\"%1\">Open on GitHub</a>").arg(htmlUrl.toHtmlEscaped()), Qt::RichText };
}

TextWithFormat NotificationTextFormatter::formatDate(const Notification& n) {
    QDateTime dt = QDateTime::fromString(n.updatedAt, Qt::ISODate);
    QString dateStr = dt.isValid()
                          ? QLocale::system().toString(dt, QLocale::ShortFormat)
                          : n.updatedAt;
    return { "Date: " + dateStr, Qt::PlainText };
}
