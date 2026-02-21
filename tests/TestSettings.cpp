#include <QTest>
#include <QSettings>
#include "../src/SettingsDialog.h"

class TestSettings : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        QCoreApplication::setOrganizationName("Kgithub-notify-test");
        QCoreApplication::setApplicationName("Kgithub-notify-test");
    }

    void init() {
        QSettings settings;
        settings.clear();
    }

    void testDefaultInterval() {
        QCOMPARE(SettingsDialog::getInterval(), 5);
    }

    void testCustomInterval() {
        QSettings settings;
        settings.setValue("interval", 15);
        QCOMPARE(SettingsDialog::getInterval(), 15);
    }
};

QTEST_MAIN(TestSettings)
#include "TestSettings.moc"
