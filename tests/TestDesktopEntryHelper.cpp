#include <KConfigGroup>
#include <KDesktopFile>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTextStream>
#include <QtTest>

#include "../src/utils/DesktopEntryHelper.h"

class TestDesktopEntryHelper : public QObject {
    Q_OBJECT
   private slots:
    void testEscapeExecNormal() {
        QString exec = QStringLiteral("/usr/bin/kgithub-notify");
        QCOMPARE(DesktopEntryHelper::escapeExec(exec, false), QStringLiteral("/usr/bin/kgithub-notify"));
        QCOMPARE(DesktopEntryHelper::escapeExec(exec, true), QStringLiteral("/usr/bin/kgithub-notify --background"));
        QCOMPARE(DesktopEntryHelper::unquoteExec(QStringLiteral("/usr/bin/kgithub-notify")), exec);
        QCOMPARE(DesktopEntryHelper::unquoteExec(QStringLiteral("/usr/bin/kgithub-notify --background")), exec);
    }

    void testEscapeExecSpaces() {
        QString exec = QStringLiteral("/opt/my apps/kgithub-notify");
        QString expectedNoBg = QString::fromUtf8(R"("/opt/my apps/kgithub-notify")");
        QString expectedBg = QString::fromUtf8(R"("/opt/my apps/kgithub-notify" --background)");
        QCOMPARE(DesktopEntryHelper::escapeExec(exec, false), expectedNoBg);
        QCOMPARE(DesktopEntryHelper::escapeExec(exec, true), expectedBg);
        QCOMPARE(DesktopEntryHelper::unquoteExec(expectedNoBg), exec);
        QCOMPARE(DesktopEntryHelper::unquoteExec(expectedBg), exec);
    }

    void testEscapeExecQuotes() {
        QString exec = QString::fromUtf8(R"(/opt/"quoted"/app)");
        // Layer 1: quotes argument, escapes " as \" -> "/opt/\"quoted\"/app"
        // Layer 2: escapes \ as \\ -> "/opt/\\"quoted\\"/app"
        QString expected = QString::fromUtf8(R"("/opt/\\"quoted\\"/app")");
        QCOMPARE(DesktopEntryHelper::escapeExec(exec, false), expected);
        QCOMPARE(DesktopEntryHelper::unquoteExec(expected), exec);
    }

    void testEscapeExecBackslashes() {
        QString exec = QString::fromUtf8(R"(/opt\path\app)");
        // Layer 1: quotes argument, escapes \ as \\ -> "/opt\\path\\app" (2 backslashes each)
        // Layer 2: escapes each \ as \\ -> "/opt\\\\path\\\\app" (4 backslashes each)
        QString expected = QString::fromUtf8(R"("/opt\\\\path\\\\app")");
        QCOMPARE(DesktopEntryHelper::escapeExec(exec, false), expected);
        QCOMPARE(DesktopEntryHelper::unquoteExec(expected), exec);
    }

    void testEscapeExecDollar() {
        QString exec = QString::fromUtf8(R"(/opt/$HOME/app)");
        // Layer 1: quotes argument, escapes $ as \$ -> "/opt/\$HOME/app"
        // Layer 2: escapes \ as \\ -> "/opt/\\$HOME/app"
        QString expected = QString::fromUtf8(R"("/opt/\\$HOME/app")");
        QCOMPARE(DesktopEntryHelper::escapeExec(exec, false), expected);
        QCOMPARE(DesktopEntryHelper::unquoteExec(expected), exec);
    }

    void testEscapeExecBackticks() {
        QString exec = QString::fromUtf8(R"(/opt/`cmd`/app)");
        // Layer 1: quotes argument, escapes ` as \` -> "/opt/\`cmd\`/app"
        // Layer 2: escapes \ as \\ -> "/opt/\\`cmd\\`/app"
        QString expected = QString::fromUtf8(R"("/opt/\\`cmd\\`/app")");
        QCOMPARE(DesktopEntryHelper::escapeExec(exec, false), expected);
        QCOMPARE(DesktopEntryHelper::unquoteExec(expected), exec);
    }

    void testEscapeExecLiteralPercent() {
        QString exec = QStringLiteral("/opt/%20/app");
        // Layer 1: % -> %% (no quoting needed)
        // Layer 2: no backslashes
        QString expected = QStringLiteral("/opt/%%20/app");
        QCOMPARE(DesktopEntryHelper::escapeExec(exec, false), expected);
        QCOMPARE(DesktopEntryHelper::unquoteExec(expected), exec);
    }

    void testEscapeExecComplexCombination() {
        QString exec = QString::fromUtf8(R"RAW(/opt/my apps/%20/"test"/$var/`run`/bin\dir)RAW");
        // Layer 1: quotes argument, escapes ", $, `, \, %
        // Layer 2: escapes backslash as double-backslash
        QString expectedBg =
            QString::fromUtf8(R"RAW("/opt/my apps/%%20/\\"test\\"/\\$var/\\`run\\`/bin\\\\dir" --background)RAW");
        QString expectedNoBg = QString::fromUtf8(R"RAW("/opt/my apps/%%20/\\"test\\"/\\$var/\\`run\\`/bin\\\\dir")RAW");

        QCOMPARE(DesktopEntryHelper::escapeExec(exec, true), expectedBg);
        QCOMPARE(DesktopEntryHelper::escapeExec(exec, false), expectedNoBg);
        QCOMPARE(DesktopEntryHelper::unquoteExec(expectedBg), exec);
        QCOMPARE(DesktopEntryHelper::unquoteExec(expectedNoBg), exec);
    }

    void testEscapeExecAllReservedCharacters() {
        struct TestCase {
            QString input;
            bool mustBeQuoted;
            QString mustContain;
        };

        QList<TestCase> cases = {
            {QStringLiteral("/usr/bin/app with space"), true, QStringLiteral("with space")},
            {QStringLiteral("/usr/bin/app\twith\ttab"), true, QString::fromUtf8(R"(with\ttab)")},
            {QString::fromUtf8(R"RAW(/usr/bin/app"with"quotes)RAW"), true, QString::fromUtf8(R"RAW(\\"with\\")RAW")},
            {QStringLiteral("/usr/bin/app'with'single"), true, QStringLiteral("'with'single")},
            {QString::fromUtf8(R"RAW(/usr/bin/app\with\backslash)RAW"), true,
             QString::fromUtf8(R"RAW(\\\\with\\\\)RAW")},
            {QString::fromUtf8(R"RAW(/usr/bin/app`with`backtick)RAW"), true, QString::fromUtf8(R"RAW(\\`)RAW")},
            {QString::fromUtf8(R"RAW(/usr/bin/app$with$dollar)RAW"), true, QString::fromUtf8(R"RAW(\\$with\\$)RAW")},
            {QStringLiteral("/usr/bin/app%20with%percent"), false, QStringLiteral("%%20with%%percent")},
            {QStringLiteral("/usr/bin/app~with~tilde"), true, QStringLiteral("~with~tilde")},
            {QStringLiteral("/usr/bin/app#with#hash"), true, QStringLiteral("#with#hash")},
            {QStringLiteral("/usr/bin/app(with)parens"), true, QStringLiteral("(with)parens")},
        };

        for (const auto& tc : cases) {
            QString escaped = DesktopEntryHelper::escapeExec(tc.input, false);
            if (tc.mustBeQuoted) {
                QVERIFY2(escaped.startsWith('"') && escaped.endsWith('"'),
                         qPrintable(QStringLiteral("Expected quotes for %1, got %2").arg(tc.input, escaped)));
            }
            QVERIFY2(escaped.contains(tc.mustContain),
                     qPrintable(QStringLiteral("Expected %1 to contain %2").arg(escaped, tc.mustContain)));

            QString roundTrip = DesktopEntryHelper::unquoteExec(escaped);
            QCOMPARE(roundTrip, tc.input);
        }
    }

    void testKDesktopFileIntegration() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        QStringList testPaths = {
            QStringLiteral("/usr/bin/simple-app"),
            QStringLiteral("/opt/my apps/space-app"),
            QString::fromUtf8(R"(/opt/"quoted"/app)"),
            QString::fromUtf8(R"(/opt\path\app)"),
            QString::fromUtf8(R"(/opt/$HOME/app)"),
            QString::fromUtf8(R"(/opt/`cmd`/app)"),
            QStringLiteral("/opt/%20/app"),
            QString::fromUtf8(R"(/opt/my apps/%20/"test"/$var/`run`/bin\dir)"),
        };

        QStringList expectedDecodedPaths = {
            QStringLiteral("/usr/bin/simple-app --background"),
            QString::fromUtf8(R"("/opt/my apps/space-app" --background)"),
            QString::fromUtf8(R"("/opt/\"quoted\"/app" --background)"),
            QString::fromUtf8(R"("/opt\\path\\app" --background)"),
            QString::fromUtf8(R"("/opt/\$HOME/app" --background)"),
            QString::fromUtf8(R"("/opt/\`cmd\`/app" --background)"),
            QStringLiteral("/opt/%%20/app --background"),
            QString::fromUtf8(R"("/opt/my apps/%%20/\"test\"/\$var/\`run\`/bin\\dir" --background)"),
        };

        QCOMPARE(testPaths.size(), expectedDecodedPaths.size());

        for (int i = 0; i < testPaths.size(); ++i) {
            const QString& path = testPaths.at(i);
            QString desktopFilePath = tempDir.path() + QStringLiteral("/integration_%1.desktop").arg(i);

            QString escapedExec = DesktopEntryHelper::escapeExec(path, true);

            QFile file(desktopFilePath);
            QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
            QTextStream out(&file);
            out << "[Desktop Entry]\n";
            out << "Type=Application\n";
            out << "Name=TestApp\n";
            out << "Exec=" << escapedExec << "\n";
            file.close();

            // 1. Verify KDE's KDesktopFile correctly parses the desktop entry string layer
            KDesktopFile df(desktopFilePath);
            QVERIFY(df.desktopGroup().exists());
            QString kExec = df.desktopGroup().readEntry("Exec");

            // KDesktopFile decodes the Desktop Entry string value escaping layer (Layer 2)
            // Hardcode expected output or provide independent oracle to prevent self-reference
            QCOMPARE(kExec, expectedDecodedPaths.at(i));

            // 2. Verify DesktopEntryHelper::parseExecFirstArgument correctly decodes Layer 1
            QString parsedExecutable = DesktopEntryHelper::parseExecFirstArgument(kExec);
            QCOMPARE(parsedExecutable, path);

            // 3. Verify DesktopEntryHelper::unquoteExec also inverts both layers from raw file line
            QString unquotedDirect = DesktopEntryHelper::unquoteExec(escapedExec);
            QCOMPARE(unquotedDirect, path);
        }
    }

    void testManuallyAuthoredDesktopEntries() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // Create a real mock binary in tempDir so QFileInfo::exists() is true
        QString dummyBinary = tempDir.path() + QStringLiteral("/my-app");
        {
            QFile bin(dummyBinary);
            QVERIFY(bin.open(QIODevice::WriteOnly));
            bin.write("#!/bin/sh\nexit 0\n");
            bin.close();
            bin.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
        }

        // 1. Manually authored with standard unquoted Exec and field codes
        {
            QString dfPath = tempDir.path() + QStringLiteral("/manual1.desktop");
            QFile file(dfPath);
            QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
            QTextStream out(&file);
            out << "[Desktop Entry]\n";
            out << "Type=Application\n";
            out << "Name=Manual1\n";
            out << "Exec=" << dummyBinary << " %u %F\n";
            file.close();

            DesktopEntryDiagnosis diag =
                DesktopEntryHelper::diagnose(QStringLiteral("manual1.desktop"), {tempDir.path()}, dummyBinary);
            QCOMPARE(diag.status, DesktopEntryStatus::ValidUsable);
            QVERIFY(diag.isUsable);
            QCOMPARE(diag.currentExecInEntry, dummyBinary);
        }

        // 2. Manually authored with spec-compliant 4-backslash serialization
        {
            QString dfPath = tempDir.path() + QStringLiteral("/manual2.desktop");
            QFile file(dfPath);
            QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
            QTextStream out(&file);
            out << "[Desktop Entry]\n";
            out << "Type=Application\n";
            out << "Name=Manual2\n";
            // Raw text: Exec="/custom\\\\path\\\\binary" --flag
            out << "Exec=\"/custom\\\\\\\\path\\\\\\\\binary\" --flag\n";
            file.close();

            DesktopEntryDiagnosis diag =
                DesktopEntryHelper::diagnose(QStringLiteral("manual2.desktop"), {tempDir.path()});
            // Binary doesn't exist on disk, so PresentUnusable
            QCOMPARE(diag.status, DesktopEntryStatus::PresentUnusable);
            QCOMPARE(diag.currentExecInEntry, QString::fromUtf8(R"(/custom\path\binary)"));
        }

        // 3. Manually authored with spec-compliant \\$ and \\` and \\"
        {
            QString dfPath = tempDir.path() + QStringLiteral("/manual3.desktop");
            QFile file(dfPath);
            QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
            QTextStream out(&file);
            out << "[Desktop Entry]\n";
            out << "Type=Application\n";
            out << "Name=Manual3\n";
            out << "Exec=\"/custom/\\\\$HOME/\\\\`bin\\\\`/\\\\\"app\\\\\"\"\n";
            file.close();

            DesktopEntryDiagnosis diag =
                DesktopEntryHelper::diagnose(QStringLiteral("manual3.desktop"), {tempDir.path()});
            QCOMPARE(diag.status, DesktopEntryStatus::PresentUnusable);
            QCOMPARE(diag.currentExecInEntry, QString::fromUtf8(R"(/custom/$HOME/`bin`/"app")"));
        }

        // 4. Manually authored unquoted Exec with escaped space and percent
        {
            QString dfPath = tempDir.path() + QStringLiteral("/manual4.desktop");
            QFile file(dfPath);
            QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
            QTextStream out(&file);
            out << "[Desktop Entry]\n";
            out << "Type=Application\n";
            out << "Name=Manual4\n";
            out << "Exec=/usr/bin/app\\ with\\ spaces%%20 --option\n";
            file.close();

            DesktopEntryDiagnosis diag =
                DesktopEntryHelper::diagnose(QStringLiteral("manual4.desktop"), {tempDir.path()});
            QCOMPARE(diag.status, DesktopEntryStatus::PresentUnusable);
            QCOMPARE(diag.currentExecInEntry, QStringLiteral("/usr/bin/app with spaces%20"));
        }
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
