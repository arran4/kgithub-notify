#ifndef KGHN_URLHELPER_H
#define KGHN_URLHELPER_H

#include <QDesktopServices>
#include <QString>
#include <QUrl>

class UrlHelper {
   public:
    static bool isSafeWebUrl(const QUrl& url) {
        if (!url.isValid() || url.isEmpty()) {
            return false;
        }
        QString scheme = url.scheme().toLower();
        if (scheme != QLatin1String("http") && scheme != QLatin1String("https")) {
            return false;
        }
        if (url.host().isEmpty()) {
            return false;
        }
        return true;
    }

    static bool isSafeWebUrl(const QString& urlString) {
        QString trimmed = urlString.trimmed();
        if (trimmed.isEmpty()) {
            return false;
        }
        QUrl url(trimmed);
        return isSafeWebUrl(url);
    }

    static bool openUrl(const QUrl& url) {
        if (!isSafeWebUrl(url)) {
            return false;
        }
        return QDesktopServices::openUrl(url);
    }

    static bool openUrl(const QString& urlString) {
        QString trimmed = urlString.trimmed();
        if (!isSafeWebUrl(trimmed)) {
            return false;
        }
        return QDesktopServices::openUrl(QUrl(trimmed));
    }
};

#endif  // KGHN_URLHELPER_H
