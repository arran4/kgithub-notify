#ifndef KGHN_DESKTOPENTRYHELPER_H
#define KGHN_DESKTOPENTRYHELPER_H

#include <QString>
#include <QStringList>

enum class DesktopEntryStatus { Missing, ValidUsable, PresentMismatched, PresentUnusable };

struct DesktopEntryDiagnosis {
    DesktopEntryStatus status = DesktopEntryStatus::Missing;
    QString foundPath;
    QString expectedExec;
    QString currentExecInEntry;
    QString reason;
    bool isUsable = false;
};

class DesktopEntryHelper {
   public:
    // Layer 1: Exec command-line argument quoting & escaping
    static QString formatExecArgument(const QString& arg);

    // Layer 2: Desktop Entry string-value escaping
    static QString serializeStringValue(const QString& execCommandLine);

    // Layer 2 inverse: Desktop Entry string-value unescaping
    static QString deserializeStringValue(const QString& serializedString);

    // Layer 1 inverse: Exec command-line first argument parsing
    static QString parseExecFirstArgument(const QString& commandLine);

    static QString escapeExec(const QString& executablePath, bool background = false);
    static QString unquoteExec(const QString& execLine);

    static DesktopEntryDiagnosis diagnose(const QString& desktopFileName = QString(),
                                          const QStringList& searchDirs = QStringList(),
                                          const QString& expectedExecutable = QString());

    static bool registerDesktopEntry(const QString& targetDir = QString(), bool overwrite = false,
                                     QString* error = nullptr);

    static bool writeAutostartEntry(bool enable, bool background, const QString& configLocation = QString(),
                                    QString* error = nullptr);

    static bool isAutostartEnabled(const QString& configLocation = QString());
};

#endif  // KGHN_DESKTOPENTRYHELPER_H
