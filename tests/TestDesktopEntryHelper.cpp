#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>
#include <QtTest>

#include "../src/utils/DesktopEntryHelper.h"

class TestDesktopEntryHelper : public QObject {
    Q_OBJECT
   private slots:
    void testEscapeExecNormal() {
        QString exec = "/usr/bin/kgithub-notify";
        QCOMPARE(DesktopEntryHelper::escapeExec(exec, false), QString("/usr/bin/kgithub-notify"));
        QCOMPARE(DesktopEntryHelper::escapeExec(exec, true), QString("/usr/bin/kgithub-notify --background"));
    }

    void testEscapeExecSpaces() {
        QString exec = "/opt/my apps/kgithub-notify";
        QCOMPARE(DesktopEntryHelper::escapeExec(exec, false), QString("\"/opt/my apps/kgithub-notify\""));
        QCOMPARE(DesktopEntryHelper::escapeExec(exec, true), QString("\"/opt/my apps/kgithub-notify\" --background"));
    }

    void testEscapeExecQuotes() {
        QString exec = "/opt/\"quoted\"/app";
        QCOMPARE(DesktopEntryHelper::escapeExec(exec, false), QString("\"/opt/\\\"quoted\\\"/app\""));
    }

    void testEscapeExecBackslashes() {
        QString exec = "/opt\\path\\app";
        QCOMPARE(DesktopEntryHelper::escapeExec(exec, false), QString("\"/opt\\\\path\\\\app\""));
    }

    void testEscapeExecDollar() {
        QString exec = "/opt/$HOME/app";
        QCOMPARE(DesktopEntryHelper::escapeExec(exec, false), QString("\"/opt/\\$HOME/app\""));
    }

    void testEscapeExecBackticks() {
        QString exec = "/opt/`cmd`/app";
        QCOMPARE(DesktopEntryHelper::escapeExec(exec, false), QString("\"/opt/\\`cmd\\`/app\""));
    }

    void testEscapeExecLiteralPercent() {
        QString exec = "/opt/%20/app";
        QCOMPARE(DesktopEntryHelper::escapeExec(exec, false), QString("/opt/%%20/app"));
    }

    void testEscapeExecComplexCombination() {
        QString exec = "/opt/my apps/%20/\"test\"/$var/`run`/bin";
        QString escaped = DesktopEntryHelper::escapeExec(exec, true);
        QVERIFY(escaped.startsWith('"'));
        QVERIFY(escaped.contains("%%20"));
        QVERIFY(escaped.contains("\\\"test\\\""));
        QVERIFY(escaped.contains("\\$var"));
        QVERIFY(escaped.contains("\\`run\\`"));
        QVERIFY(escaped.endsWith("\" --background"));

        QString unquoted = DesktopEntryHelper::unquoteExec(escaped);
        QCOMPARE(unquoted, exec);
    }

    void testDiagnosisMissing() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        DesktopEntryDiagnosis diag = DesktopEntryHelper::diagnose("nonexistent.desktop", {tempDir.path()});
        QCOMPARE(diag.status, DesktopEntryStatus::Missing);
        QVERIFY(!diag.isUsable);
    }

    void testDiagnosisValidUsable() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        QString appPath = QCoreApplication::applicationFilePath();
        QString desktopPath = tempDir.path() + "/test.desktop";
        QFile file(desktopPath);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream out(&file);
        out << "[Desktop Entry]\n";
        out << "Type=Application\n";
        out << "Name=TestApp\n";
        out << "Exec=" << DesktopEntryHelper::escapeExec(appPath, false) << "\n";
        file.close();

        DesktopEntryDiagnosis diag = DesktopEntryHelper::diagnose("test.desktop", {tempDir.path()}, appPath);
        QCOMPARE(diag.status, DesktopEntryStatus::ValidUsable);
        QVERIFY(diag.isUsable);
        QCOMPARE(diag.foundPath, desktopPath);
    }

    void testDiagnosisPresentMismatched() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // Use an existing executable on linux such as /bin/sh
        QString otherExec = "/bin/sh";
        if (!QFile::exists(otherExec)) otherExec = "/usr/bin/sh";

        QString desktopPath = tempDir.path() + "/test.desktop";
        QFile file(desktopPath);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream out(&file);
        out << "[Desktop Entry]\n";
        out << "Type=Application\n";
        out << "Name=TestApp\n";
        out << "Exec=" << otherExec << "\n";
        file.close();

        DesktopEntryDiagnosis diag =
            DesktopEntryHelper::diagnose("test.desktop", {tempDir.path()}, "/custom/other/path");
        QCOMPARE(diag.status, DesktopEntryStatus::PresentMismatched);
        QVERIFY(diag.isUsable);
    }

    void testDiagnosisPresentUnusable() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        QString desktopPath = tempDir.path() + "/test.desktop";
        QFile file(desktopPath);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream out(&file);
        out << "[Desktop Entry]\n";
        out << "Type=Application\n";
        out << "Name=TestApp\n";
        out << "Exec=/nonexistent/binary/path\n";
        file.close();

        DesktopEntryDiagnosis diag = DesktopEntryHelper::diagnose("test.desktop", {tempDir.path()});
        QCOMPARE(diag.status, DesktopEntryStatus::PresentUnusable);
        QVERIFY(!diag.isUsable);
    }

    void testAutostartLifecycle() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        QString err;
        // Enable autostart with background
        bool ok = DesktopEntryHelper::writeAutostartEntry(true, true, tempDir.path(), &err);
        QVERIFY(ok);
        QVERIFY(DesktopEntryHelper::isAutostartEnabled(tempDir.path()));

        QString autostartFile = tempDir.path() + "/autostart/kgithub-notify.desktop";
        QVERIFY(QFile::exists(autostartFile));

        QFile file(autostartFile);
        QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
        QString content = QString::fromUtf8(file.readAll());
        file.close();

        QVERIFY(content.contains("Exec="));
        QVERIFY(content.contains("--background"));

        // Disable autostart
        ok = DesktopEntryHelper::writeAutostartEntry(false, false, tempDir.path(), &err);
        QVERIFY(ok);
        QVERIFY(!DesktopEntryHelper::isAutostartEnabled(tempDir.path()));
        QVERIFY(!QFile::exists(autostartFile));
    }

    void testEscapeExecAllReservedCharacters() {
        struct TestCase {
            QString input;
            bool mustBeQuoted;
            QString mustContain;
        };

        QList<TestCase> cases = {
            {"/usr/bin/app with space", true, "with space"},
            {"/usr/bin/app\twith\ttab", true, "with\ttab"},
            {"/usr/bin/app\"with\"quotes", true, "\\\"with\\\""},
            {"/usr/bin/app'with'single", true, "'with'single"},
            {"/usr/bin/app\\with\\backslash", true, "\\\\with\\\\"},
            {"/usr/bin/app`with`backtick", true, "\\`with\\`"},
            {"/usr/bin/app$with$dollar", true, "\\$with\\$"},
            {"/usr/bin/app%20with%percent", false, "%%20with%%percent"},
            {"/usr/bin/app~with~tilde", true, "~with~tilde"},
            {"/usr/bin/app#with#hash", true, "#with#hash"},
            {"/usr/bin/app(with)parens", true, "(with)parens"},
        };

        for (const auto& tc : cases) {
            QString escaped = DesktopEntryHelper::escapeExec(tc.input, false);
            if (tc.mustBeQuoted) {
                QVERIFY2(escaped.startsWith('"') && escaped.endsWith('"'),
                         qPrintable(QString("Expected quotes for %1, got %2").arg(tc.input, escaped)));
            }
            QVERIFY2(escaped.contains(tc.mustContain),
                     qPrintable(QString("Expected %1 to contain %2").arg(escaped, tc.mustContain)));

            QString roundTrip = DesktopEntryHelper::unquoteExec(escaped);
            QCOMPARE(roundTrip, tc.input);
        }
    }

    void testRegistrationMissingCreatesEntry() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        QString err;
        bool ok = DesktopEntryHelper::registerDesktopEntry(tempDir.path(), false, &err);
        QVERIFY(ok);
        QVERIFY(err.isEmpty());

        QString desktopFile = tempDir.path() + "/kgithub-notify.desktop";
        QVERIFY(QFile::exists(desktopFile));

        QFile file(desktopFile);
        QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
        QString content = QString::fromUtf8(file.readAll());
        file.close();

        QVERIFY(content.contains("[Desktop Entry]"));
        QVERIFY(content.contains("Type=Application"));
        QVERIFY(content.contains("Name=KGithubNotify"));
        QVERIFY(content.contains("Exec="));
        QVERIFY(content.contains("StartupWMClass=kgithub-notify"));
    }

    void testRegistrationMatchingIsIdempotent() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        QString err;
        // First registration creates entry
        QVERIFY(DesktopEntryHelper::registerDesktopEntry(tempDir.path(), false, &err));

        QString desktopFile = tempDir.path() + "/kgithub-notify.desktop";
        QFile file(desktopFile);
        QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
        QString firstContent = QString::fromUtf8(file.readAll());
        file.close();

        // Second registration should detect matching entry and succeed without overwrite flag
        QVERIFY(DesktopEntryHelper::registerDesktopEntry(tempDir.path(), false, &err));
        QVERIFY(err.isEmpty());

        QFile file2(desktopFile);
        QVERIFY(file2.open(QIODevice::ReadOnly | QIODevice::Text));
        QString secondContent = QString::fromUtf8(file2.readAll());
        file2.close();

        QCOMPARE(firstContent, secondContent);
    }

    void testRegistrationRefusesMismatchedWithoutOverwrite() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        QString desktopFile = tempDir.path() + "/kgithub-notify.desktop";
        QString customContent =
            "[Desktop Entry]\nType=Application\nName=UserCustomApp\nExec=/custom/bin\nComment=Custom user desktop "
            "file\n";

        QFile file(desktopFile);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        file.write(customContent.toUtf8());
        file.close();

        QString err;
        // Attempting to register without overwrite must fail
        bool ok = DesktopEntryHelper::registerDesktopEntry(tempDir.path(), false, &err);
        QVERIFY(!ok);
        QVERIFY(err.contains("already exists with different contents"));
        QVERIFY(err.contains("--register-desktop-overwrite"));

        // Verify the file was left completely untouched
        QFile checkFile(desktopFile);
        QVERIFY(checkFile.open(QIODevice::ReadOnly | QIODevice::Text));
        QString currentContent = QString::fromUtf8(checkFile.readAll());
        checkFile.close();
        QCOMPARE(currentContent, customContent);
    }

    void testRegistrationOverwritesWithExplicitFlag() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        QString desktopFile = tempDir.path() + "/kgithub-notify.desktop";
        QString customContent = "[Desktop Entry]\nType=Application\nName=OldMismatched\nExec=/old/bin\n";

        QFile file(desktopFile);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        file.write(customContent.toUtf8());
        file.close();

        QString err;
        // Explicit overwrite performs replacement
        bool ok = DesktopEntryHelper::registerDesktopEntry(tempDir.path(), true, &err);
        QVERIFY(ok);
        QVERIFY(err.isEmpty());

        QFile checkFile(desktopFile);
        QVERIFY(checkFile.open(QIODevice::ReadOnly | QIODevice::Text));
        QString newContent = QString::fromUtf8(checkFile.readAll());
        checkFile.close();

        QVERIFY(newContent != customContent);
        QVERIFY(newContent.contains("Name=KGithubNotify"));
        QVERIFY(newContent.contains("Exec="));
    }

    void testDiagnoseAndStartupAreReadOnly() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // When directory is empty, diagnose writes nothing
        DesktopEntryDiagnosis diag1 = DesktopEntryHelper::diagnose("kgithub-notify.desktop", {tempDir.path()});
        QCOMPARE(diag1.status, DesktopEntryStatus::Missing);
        QCOMPARE(QDir(tempDir.path()).entryList(QDir::NoDotAndDotDot | QDir::AllEntries).size(), 0);

        // When directory contains a mismatched entry, diagnose writes nothing
        QString desktopFile = tempDir.path() + "/kgithub-notify.desktop";
        QFile file(desktopFile);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        file.write("[Desktop Entry]\nType=Application\nName=Test\nExec=/bin/sh\n");
        file.close();

        QDateTime mtimeBefore = QFileInfo(desktopFile).lastModified();
        DesktopEntryDiagnosis diag2 = DesktopEntryHelper::diagnose("kgithub-notify.desktop", {tempDir.path()});
        QCOMPARE(diag2.status, DesktopEntryStatus::PresentMismatched);
        QCOMPARE(QFileInfo(desktopFile).lastModified(), mtimeBefore);
    }
};

QTEST_MAIN(TestDesktopEntryHelper)
#include "TestDesktopEntryHelper.moc"
