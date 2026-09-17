#include "DesktopEntryHelper.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QStandardPaths>
#include <QTextStream>

QString DesktopEntryHelper::escapeExec(const QString& executablePath, bool background) {
    if (executablePath.isEmpty()) {
        return background ? QStringLiteral("--background") : QString();
    }

    bool needsQuotes = false;
    for (const QChar& c : executablePath) {
        if (c.isSpace() || c == QChar('"') || c == QChar('\'') || c == QChar('\\') ||
            c == QChar('>') || c == QChar('<') || c == QChar('&') || c == QChar(';') ||
            c == QChar('|') || c == QChar('$') || c == QChar('*') || c == QChar('?') ||
            c == QChar('!') || c == QChar('`')) {
            needsQuotes = true;
            break;
        }
    }

    QString result;
    if (needsQuotes) {
        result.append(QChar('"'));
        for (const QChar& c : executablePath) {
            if (c == QChar('"')) {
                result.append(QChar('\\'));
                result.append(QChar('"'));
            } else if (c == QChar('\\')) {
                result.append(QChar('\\'));
                result.append(QChar('\\'));
            } else if (c == QChar('$')) {
                result.append(QChar('\\'));
                result.append(QChar('$'));
            } else if (c == QChar('`')) {
                result.append(QChar('\\'));
                result.append(QChar('`'));
            } else if (c == QChar('%')) {
                result.append(QStringLiteral("%%"));
            } else {
                result.append(c);
            }
        }
        result.append(QChar('"'));
    } else {
        for (const QChar& c : executablePath) {
            if (c == QChar('%')) {
                result.append(QStringLiteral("%%"));
            } else {
                result.append(c);
            }
        }
    }

    if (background) {
        result.append(QStringLiteral(" --background"));
    }

    return result;
}

QString DesktopEntryHelper::unquoteExec(const QString& execLine) {
    QString trimmed = execLine.trimmed();
    if (trimmed.isEmpty()) return QString();

    QString result;
    if (trimmed.startsWith(QChar('"'))) {
        int i = 1;
        bool escaping = false;
        while (i < trimmed.length()) {
            QChar c = trimmed.at(i);
            if (escaping) {
                result.append(c);
                escaping = false;
            } else if (c == QChar('\\')) {
                escaping = true;
            } else if (c == QChar('"')) {
                break;
            } else if (c == QChar('%') && i + 1 < trimmed.length() && trimmed.at(i + 1) == QChar('%')) {
                result.append(QChar('%'));
                i++;
            } else {
                result.append(c);
            }
            i++;
        }
    } else {
        int spaceIdx = -1;
        for (int i = 0; i < trimmed.length(); ++i) {
            if (trimmed.at(i).isSpace()) {
                spaceIdx = i;
                break;
            }
        }
        QString firstToken = (spaceIdx == -1) ? trimmed : trimmed.left(spaceIdx);
        for (int i = 0; i < firstToken.length(); ++i) {
            if (firstToken.at(i) == QChar('%') && i + 1 < firstToken.length() && firstToken.at(i + 1) == QChar('%')) {
                result.append(QChar('%'));
                i++;
            } else {
                result.append(firstToken.at(i));
            }
        }
    }

    return result;
}

DesktopEntryDiagnosis DesktopEntryHelper::diagnose(const QString& desktopFileName,
                                                  const QStringList& searchDirs,
                                                  const QString& expectedExecutable) {
    DesktopEntryDiagnosis diag;
    QString filename = desktopFileName;
    if (filename.isEmpty()) {
        QString base = QGuiApplication::desktopFileName();
        if (base.isEmpty()) base = QStringLiteral("kgithub-notify");
        filename = base.endsWith(QLatin1String(".desktop")) ? base : base + QStringLiteral(".desktop");
    }

    QString expected = expectedExecutable.isEmpty() ? QCoreApplication::applicationFilePath() : expectedExecutable;
    diag.expectedExec = expected;

    QStringList dirs =
        searchDirs.isEmpty() ? QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation) : searchDirs;

    QString foundPath;
    for (const QString& d : dirs) {
        QString candidate = d + QLatin1Char('/') + filename;
        if (QFileInfo::exists(candidate)) {
            foundPath = candidate;
            break;
        }
    }

    if (foundPath.isEmpty()) {
        diag.status = DesktopEntryStatus::Missing;
        diag.reason = QStringLiteral("Desktop entry '%1' not found in standard application locations").arg(filename);
        diag.isUsable = false;
        return diag;
    }

    diag.foundPath = foundPath;

    QFile file(foundPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        diag.status = DesktopEntryStatus::PresentUnusable;
        diag.reason = QStringLiteral("Failed to open desktop file: %1").arg(file.errorString());
        diag.isUsable = false;
        return diag;
    }

    QByteArray content = file.readAll();
    file.close();

    if (content.isEmpty()) {
        diag.status = DesktopEntryStatus::PresentUnusable;
        diag.reason = QStringLiteral("Desktop file is empty");
        diag.isUsable = false;
        return diag;
    }

    QString text = QString::fromUtf8(content);
    if (!text.contains(QStringLiteral("[Desktop Entry]"))) {
        diag.status = DesktopEntryStatus::PresentUnusable;
        diag.reason = QStringLiteral("Desktop file is missing [Desktop Entry] group header");
        diag.isUsable = false;
        return diag;
    }

    QString execLine;
    for (const QString& line : text.split(QLatin1Char('\n'))) {
        QString trimmed = line.trimmed();
        if (trimmed.startsWith(QStringLiteral("Exec="))) {
            execLine = trimmed.mid(5).trimmed();
            break;
        }
    }

    if (execLine.isEmpty()) {
        diag.status = DesktopEntryStatus::PresentUnusable;
        diag.reason = QStringLiteral("Desktop file is missing Exec= key");
        diag.isUsable = false;
        return diag;
    }

    QString execPath = unquoteExec(execLine);
    diag.currentExecInEntry = execPath;

    QFileInfo execInfo(execPath);
    bool execExists = execInfo.exists();

    bool matchesExpected = (QFileInfo(expected) == execInfo || expected == execPath);

    if (matchesExpected) {
        diag.status = DesktopEntryStatus::ValidUsable;
        diag.reason = QStringLiteral("Desktop entry matches current executable and is usable");
        diag.isUsable = true;
    } else if (execExists && execInfo.isExecutable()) {
        diag.status = DesktopEntryStatus::PresentMismatched;
        diag.reason = QStringLiteral("Desktop entry points to a different executable: %1 (expected %2)")
                          .arg(execPath, expected);
        diag.isUsable = true;
    } else {
        diag.status = DesktopEntryStatus::PresentUnusable;
        diag.reason = QStringLiteral("Desktop entry Exec target '%1' does not exist or is not executable")
                          .arg(execPath);
        diag.isUsable = false;
    }

    return diag;
}

bool DesktopEntryHelper::registerDesktopEntry(const QString& targetDir, bool overwrite, QString* error) {
    QString dirPath =
        targetDir.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation) : targetDir;

    QDir dir(dirPath);
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        if (error) *error = QStringLiteral("Failed to create directory: %1").arg(dirPath);
        return false;
    }

    QString base = QGuiApplication::desktopFileName();
    if (base.isEmpty()) base = QStringLiteral("kgithub-notify");
    QString desktopFileName = base.endsWith(QLatin1String(".desktop")) ? base : base + QStringLiteral(".desktop");

    QString targetPath = dirPath + QLatin1Char('/') + desktopFileName;
    if (QFile::exists(targetPath) && !overwrite) {
        if (error)
            *error = QStringLiteral("Desktop entry already exists and overwrite not requested: %1").arg(targetPath);
        return false;
    }

    QString execCmd = escapeExec(QCoreApplication::applicationFilePath(), false);

    QString content;
    QTextStream out(&content);
    out << "[Desktop Entry]\n";
    out << "Type=Application\n";
    out << "Name=KGithubNotify\n";
    out << "GenericName=GitHub Notifications\n";
    out << "Comment=GitHub Notification System Tray\n";
    out << "Exec=" << execCmd << "\n";
    out << "Icon=kgithub-notify\n";
    out << "Categories=Development;Utility;Qt;KDE;\n";
    out << "StartupWMClass=kgithub-notify\n";
    out << "Terminal=false\n";

    QString tempPath = targetPath + QStringLiteral(".tmp.%1").arg(QCoreApplication::applicationPid());
    QFile tempFile(tempPath);
    if (!tempFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) *error = QStringLiteral("Failed to open temporary file for writing: %1").arg(tempFile.errorString());
        return false;
    }

    QByteArray utf8 = content.toUtf8();
    if (tempFile.write(utf8) != utf8.size()) {
        tempFile.close();
        QFile::remove(tempPath);
        if (error) *error = QStringLiteral("Failed to write desktop entry content completely");
        return false;
    }
    tempFile.close();

    QFile::setPermissions(tempPath, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::ReadOther);

    if (QFile::exists(targetPath) && !QFile::remove(targetPath)) {
        QFile::remove(tempPath);
        if (error) *error = QStringLiteral("Failed to replace existing desktop entry: %1").arg(targetPath);
        return false;
    }

    if (!QFile::rename(tempPath, targetPath)) {
        QFile::remove(tempPath);
        if (error) *error = QStringLiteral("Failed to move temporary file to: %1").arg(targetPath);
        return false;
    }

    return true;
}

bool DesktopEntryHelper::writeAutostartEntry(bool enable, bool background, const QString& configLocation,
                                            QString* error) {
    QString configPath =
        configLocation.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) : configLocation;

    QDir dir(configPath);
    QString autostartDir = dir.filePath(QStringLiteral("autostart"));
    QDir aDir(autostartDir);
    if (enable && !aDir.exists() && !aDir.mkpath(QStringLiteral("."))) {
        if (error) *error = QStringLiteral("Failed to create autostart directory: %1").arg(autostartDir);
        return false;
    }

    QString targetPath = aDir.filePath(QStringLiteral("kgithub-notify.desktop"));

    if (!enable) {
        if (QFile::exists(targetPath) && !QFile::remove(targetPath)) {
            if (error) *error = QStringLiteral("Failed to remove autostart entry: %1").arg(targetPath);
            return false;
        }
        return true;
    }

    QString execCmd = escapeExec(QCoreApplication::applicationFilePath(), background);

    QString content;
    QTextStream out(&content);
    out << "[Desktop Entry]\n";
    out << "Type=Application\n";
    out << "Name=KGitHub Notify\n";
    out << "Comment=GitHub Notification System Tray\n";
    out << "Exec=" << execCmd << "\n";
    out << "Icon=kgithub-notify\n";
    out << "Categories=Development;Utility;Qt;KDE;\n";
    out << "StartupWMClass=Kgithub-notify\n";
    out << "Terminal=false\n";
    out << "X-KDE-autostart-after=panel\n";

    QString tempPath = targetPath + QStringLiteral(".tmp.%1").arg(QCoreApplication::applicationPid());
    QFile tempFile(tempPath);
    if (!tempFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) *error = QStringLiteral("Failed to open temporary autostart file: %1").arg(tempFile.errorString());
        return false;
    }

    QByteArray utf8 = content.toUtf8();
    if (tempFile.write(utf8) != utf8.size()) {
        tempFile.close();
        QFile::remove(tempPath);
        if (error) *error = QStringLiteral("Failed to write autostart content completely");
        return false;
    }
    tempFile.close();

    if (QFile::exists(targetPath) && !QFile::remove(targetPath)) {
        QFile::remove(tempPath);
        if (error) *error = QStringLiteral("Failed to replace existing autostart entry: %1").arg(targetPath);
        return false;
    }

    if (!QFile::rename(tempPath, targetPath)) {
        QFile::remove(tempPath);
        if (error) *error = QStringLiteral("Failed to move temporary file to autostart target: %1").arg(targetPath);
        return false;
    }

    return true;
}

bool DesktopEntryHelper::isAutostartEnabled(const QString& configLocation) {
    QString configPath =
        configLocation.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) : configLocation;
    QString path = configPath + QStringLiteral("/autostart/kgithub-notify.desktop");
    return QFile::exists(path);
}
