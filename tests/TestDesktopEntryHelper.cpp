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
};

QTEST_MAIN(TestDesktopEntryHelper)
#include "TestDesktopEntryHelper.moc"
