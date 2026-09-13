#include <QFutureInterface>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include "../src/SettingsDialog.h"
#include "../src/WalletManager.h"

namespace {
QFuture<WalletResult> completedFuture(const WalletResult& result) {
    QFutureInterface<WalletResult> interface;
    interface.reportStarted();
    interface.reportResult(result);
    interface.reportFinished();
    return interface.future();
}

class MockWalletBackend : public WalletBackend {
   public:
    QFuture<WalletResult> loadTokenAsync() override {
        ++loadCalls;
        return completedFuture(loadResult);
    }

    QFuture<WalletResult> saveTokenAsync(const QString& token) override {
        ++saveCalls;
        savedToken = token;
        return completedFuture(saveResult);
    }

    QFuture<WalletResult> clearTokenAsync() override {
        ++clearCalls;
        return completedFuture(clearResult);
    }

    WalletResult loadResult{true, "", ""};
    WalletResult saveResult{true, "", ""};
    WalletResult clearResult{true, "", ""};
    QString savedToken;
    int loadCalls = 0;
    int saveCalls = 0;
    int clearCalls = 0;
};

QLineEdit* tokenEdit(SettingsDialog& dialog) {
    const auto edits = dialog.findChildren<QLineEdit*>();
    for (QLineEdit* edit : edits) {
        if (edit->echoMode() == QLineEdit::Password) return edit;
    }
    return nullptr;
}

QLabel* labelContaining(SettingsDialog& dialog, const QString& text) {
    const auto labels = dialog.findChildren<QLabel*>();
    for (QLabel* label : labels) {
        if (label->text().contains(text)) return label;
    }
    return nullptr;
}

bool tokenIsAbsentFromQSettings(const QString& token) {
    QSettings settings;
    if (settings.contains("token")) return false;
    for (const QString& key : settings.allKeys()) {
        if (settings.value(key).toString() == token) return false;
    }
    return true;
}
}  // namespace

class TestCredentialPersistence : public QObject {
    Q_OBJECT

   private slots:
    void initTestCase() {
        QVERIFY(storage.isValid());
        QStandardPaths::setTestModeEnabled(true);
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, storage.path());
        QCoreApplication::setOrganizationName("credential-persistence-tests");
        QCoreApplication::setApplicationName("credential-persistence-tests");
    }

    void init() { QSettings().clear(); }
    void cleanup() { WalletManager::setBackend(nullptr); }

    void loadFailureIsVisibleAndEditable() {
        MockWalletBackend backend;
        backend.loadResult = {false, "", "wallet locked"};
        WalletManager::setBackend(&backend);

        SettingsDialog dialog;
        QLineEdit* edit = tokenEdit(dialog);
        QVERIFY(edit);
        QTRY_VERIFY(edit->isEnabled());
        QCOMPARE(edit->text(), QString());
        QCOMPARE(edit->placeholderText(), QString("Enter token..."));
        QTRY_VERIFY(labelContaining(dialog, "Failed to load token from KWallet") != nullptr);
        QCOMPARE(backend.loadCalls, 1);
    }

    void saveFailurePreservesTokenAndRetryAccepts() {
        MockWalletBackend backend;
        backend.saveResult = {false, "", "write denied"};
        WalletManager::setBackend(&backend);

        SettingsDialog dialog;
        QLineEdit* edit = tokenEdit(dialog);
        QPushButton* save = dialog.findChild<QPushButton*>("saveButton");
        QVERIFY(edit);
        QVERIFY(save);
        QTRY_VERIFY(edit->isEnabled());

        edit->setText("new_token");
        save->click();
        QTRY_COMPARE(backend.saveCalls, 1);
        QTRY_VERIFY(save->isEnabled());
        QVERIFY(dialog.result() != QDialog::Accepted);
        QCOMPARE(edit->text(), QString("new_token"));
        QTRY_VERIFY(labelContaining(dialog, "Failed to save token to KWallet") != nullptr);
        QVERIFY(tokenIsAbsentFromQSettings("new_token"));

        backend.saveResult = {true, "", ""};
        save->click();
        QTRY_COMPARE(backend.saveCalls, 2);
        QTRY_COMPARE(dialog.result(), int(QDialog::Accepted));
        QCOMPARE(backend.savedToken, QString("new_token"));
        QVERIFY(tokenIsAbsentFromQSettings("new_token"));
    }

    void clearFailureKeepsDialogOpenAndRetryAccepts() {
        MockWalletBackend backend;
        backend.loadResult = {true, "old_token", ""};
        backend.clearResult = {false, "", "remove denied"};
        WalletManager::setBackend(&backend);

        SettingsDialog dialog;
        QLineEdit* edit = tokenEdit(dialog);
        QPushButton* save = dialog.findChild<QPushButton*>("saveButton");
        QVERIFY(edit);
        QVERIFY(save);
        QTRY_COMPARE(edit->text(), QString("old_token"));

        edit->clear();
        save->click();
        QTRY_COMPARE(backend.clearCalls, 1);
        QTRY_VERIFY(save->isEnabled());
        QVERIFY(dialog.result() != QDialog::Accepted);
        QCOMPARE(edit->text(), QString());
        QTRY_VERIFY(labelContaining(dialog, "Failed to save token to KWallet") != nullptr);

        backend.clearResult = {true, "", ""};
        save->click();
        QTRY_COMPARE(backend.clearCalls, 2);
        QTRY_COMPARE(dialog.result(), int(QDialog::Accepted));
    }

   private:
    QTemporaryDir storage;
};

QTEST_MAIN(TestCredentialPersistence)
#include "TestCredentialPersistence.moc"
