#include "DesktopEntryHelper.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTextStream>
#include <algorithm>

QString DesktopEntryHelper::formatExecArgument(const QString& arg) {
    bool needsQuotes = arg.isEmpty() || std::any_of(arg.begin(), arg.end(), [](const QChar& c) {
                           return c.isSpace() || c == QChar('"') || c == QChar('\'') || c == QChar('\\') ||
                                  c == QChar('>') || c == QChar('<') || c == QChar('~') || c == QChar('|') ||
                                  c == QChar('&') || c == QChar(';') || c == QChar('$') || c == QChar('*') ||
                                  c == QChar('?') || c == QChar('#') || c == QChar('(') || c == QChar(')') ||
                                  c == QChar('`');
                       });

    QString result;
    if (needsQuotes) {
        result.append(QChar('"'));
        for (const QChar& c : arg) {
            if (c == QChar('"') || c == QChar('`') || c == QChar('$') || c == QChar('\\')) {
                result.append(QChar('\\'));
                result.append(c);
            } else if (c == QChar('%')) {
                result.append(QStringLiteral("%%"));
            } else {
                result.append(c);
            }
        }
        result.append(QChar('"'));
    } else {
        for (const QChar& c : arg) {
            if (c == QChar('%')) {
                result.append(QStringLiteral("%%"));
            } else {
                result.append(c);
            }
        }
    }
    return result;
}

QString DesktopEntryHelper::serializeStringValue(const QString& execCommandLine) {
    QString result;
    result.reserve(execCommandLine.size());
    for (const QChar& c : execCommandLine) {
        if (c == QChar('\\')) {
            result.append(QStringLiteral("\\\\"));
        } else if (c == QChar('\t')) {
            result.append(QStringLiteral("\\t"));
        } else if (c == QChar('\n')) {
            result.append(QStringLiteral("\\n"));
        } else if (c == QChar('\r')) {
            result.append(QStringLiteral("\\r"));
        } else {
            result.append(c);
        }
    }
    return result;
}

QString DesktopEntryHelper::deserializeStringValue(const QString& serializedString) {
    QString result;
    result.reserve(serializedString.size());
    int len = serializedString.length();
    int i = 0;
    while (i < len) {
        QChar c = serializedString.at(i);
        if (c == QChar('\\') && i + 1 < len) {
            QChar next = serializedString.at(i + 1);
            if (next == QChar('s')) {
                result.append(QChar(' '));
                i += 2;
            } else if (next == QChar('n')) {
                result.append(QChar('\n'));
                i += 2;
            } else if (next == QChar('t')) {
                result.append(QChar('\t'));
                i += 2;
            } else if (next == QChar('r')) {
                result.append(QChar('\r'));
                i += 2;
            } else if (next == QChar('\\')) {
                result.append(QChar('\\'));
                i += 2;
            } else {
                // Preserve backslash and next char for Exec layer
                result.append(c);
                result.append(next);
                i += 2;
            }
        } else {
            result.append(c);
            i++;
        }
    }
    return result;
}

QString DesktopEntryHelper::parseExecFirstArgument(const QString& commandLine) {
    QString trimmed = commandLine.trimmed();
    if (trimmed.isEmpty()) return QString();

    QString result;
    if (trimmed.startsWith(QChar('"'))) {
        int i = 1;
        int len = trimmed.length();
        while (i < len) {
            QChar c = trimmed.at(i);
            if (c == QChar('\\') && i + 1 < len) {
                result.append(trimmed.at(i + 1));
                i += 2;
            } else if (c == QChar('"')) {
                break;
            } else if (c == QChar('%') && i + 1 < len && trimmed.at(i + 1) == QChar('%')) {
                result.append(QChar('%'));
                i += 2;
            } else {
                result.append(c);
                i++;
            }
        }
    } else {
        int i = 0;
        int len = trimmed.length();
        while (i < len) {
            QChar c = trimmed.at(i);
            if (c.isSpace()) {
                break;
            }
            if (c == QChar('\\') && i + 1 < len) {
                result.append(trimmed.at(i + 1));
                i += 2;
            } else if (c == QChar('%') && i + 1 < len && trimmed.at(i + 1) == QChar('%')) {
                result.append(QChar('%'));
                i += 2;
            } else {
                result.append(c);
                i++;
            }
        }
    }
    return result;
}

QString DesktopEntryHelper::escapeExec(const QString& executablePath, bool background) {
    if (executablePath.isEmpty()) {
        return background ? QStringLiteral("--background") : QString();
    }

    QString execCmd = formatExecArgument(executablePath);
    if (background) {
        execCmd.append(QStringLiteral(" --background"));
    }

    return serializeStringValue(execCmd);
}

QString DesktopEntryHelper::unquoteExec(const QString& execLine) {
    QString trimmed = execLine.trimmed();
    if (trimmed.isEmpty()) return QString();

    QString inMemoryCommandLine = deserializeStringValue(trimmed);
    return parseExecFirstArgument(inMemoryCommandLine);
}

DesktopEntryDiagnosis DesktopEntryHelper::diagnose(const QString& desktopFileName, const QStringList& searchDirs,
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
        diag.reason =
            QStringLiteral("Desktop entry points to a different executable: %1 (expected %2)").arg(execPath, expected);
        diag.isUsable = true;
    } else {
        diag.status = DesktopEntryStatus::PresentUnusable;
        diag.reason =
            QStringLiteral("Desktop entry Exec target '%1' does not exist or is not executable").arg(execPath);
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

    if (QFile::exists(targetPath)) {
        QFile existingFile(targetPath);
        if (existingFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString existingContent = QString::fromUtf8(existingFile.readAll());
            existingFile.close();
            if (existingContent == content) {
                return true;
            }
        }
        if (!overwrite) {
            if (error) {
                *error = QStringLiteral(
                             "Desktop entry '%1' already exists with different contents. "
                             "Use --register-desktop-overwrite to replace.")
                             .arg(targetPath);
            }
            return false;
        }
    }

    QSaveFile file(targetPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) *error = QStringLiteral("Failed to open file for writing: %1").arg(file.errorString());
        return false;
    }

    QByteArray utf8 = content.toUtf8();
    if (file.write(utf8) != utf8.size()) {
        file.cancelWriting();
        if (error)
            *error = QStringLiteral("Failed to write desktop entry content completely: %1").arg(file.errorString());
        return false;
    }

    if (!file.commit()) {
        if (error) *error = QStringLiteral("Failed to commit desktop entry file: %1").arg(file.errorString());
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

    QSaveFile file(targetPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) *error = QStringLiteral("Failed to open autostart file for writing: %1").arg(file.errorString());
        return false;
    }

    QByteArray utf8 = content.toUtf8();
    if (file.write(utf8) != utf8.size()) {
        file.cancelWriting();
        if (error) *error = QStringLiteral("Failed to write autostart content completely: %1").arg(file.errorString());
        return false;
    }

    if (!file.commit()) {
        if (error) *error = QStringLiteral("Failed to commit autostart file: %1").arg(file.errorString());
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
