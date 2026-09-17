#include "SettingsDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QTextStream>
#include <QVBoxLayout>

#include "GitHubClient.h"
#include "RulesDialog.h"
#include "WalletManager.h"
#include "utils/DesktopEntryHelper.h"

SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent), testClient(nullptr) {
    setWindowTitle("Settings");

    QVBoxLayout* layout = new QVBoxLayout(this);

    QLabel* label = new QLabel("GitHub Personal Access Token:", this);
    layout->addWidget(label);

    QLabel* helpLabel =
        new QLabel("<small>" + GitHubClient::getPermissionGuidance().replace("\n", "<br>") + "</small>", this);
    helpLabel->setTextFormat(Qt::RichText);
    helpLabel->setStyleSheet("color: gray;");
    layout->addWidget(helpLabel);

    QHBoxLayout* tokenLayout = new QHBoxLayout();
    tokenEdit = new QLineEdit(this);
    tokenEdit->setEchoMode(QLineEdit::Password);

    // Load existing token asynchronously
    tokenEdit->setEnabled(false);
    tokenEdit->setPlaceholderText("Loading...");

    QFutureWatcher<WalletResult>* watcher = new QFutureWatcher<WalletResult>(this);
    connect(watcher, &QFutureWatcher<WalletResult>::finished, this, [this, watcher]() {
        WalletResult result = watcher->result();
        if (result.success) {
            tokenEdit->setText(result.token);
            tokenEdit->setPlaceholderText("");
        } else if (!result.errorMessage.isEmpty()) {
            statusLabel->setText(QString("<font color='red'>Warning: Failed to load token from KWallet: %1</font>")
                                     .arg(result.errorMessage.toHtmlEscaped()));
            statusLabel->show();
            tokenEdit->setPlaceholderText("Enter token...");
        } else {
            tokenEdit->setPlaceholderText("Enter token...");
        }
        tokenEdit->setEnabled(true);
        watcher->deleteLater();
    });
    watcher->setFuture(getTokenAsync());

    tokenLayout->addWidget(tokenEdit);

    testButton = new QPushButton("Test Key", this);
    connect(testButton, &QPushButton::clicked, this, &SettingsDialog::onTestClicked);
    saveWatcher = new QFutureWatcher<WalletResult>(this);
    connect(saveWatcher, &QFutureWatcher<WalletResult>::finished, this, &SettingsDialog::onSaveFinished);

    tokenLayout->addWidget(testButton);

    layout->addLayout(tokenLayout);

    statusLabel = new QLabel(this);
    statusLabel->hide();
    layout->addWidget(statusLabel);

    // Interval
    QLabel* intervalLabel = new QLabel("Refresh Interval (minutes):", this);
    layout->addWidget(intervalLabel);

    intervalCombo = new QComboBox(this);
    intervalCombo->addItems({"1", "5", "10", "15", "30", "60"});
    int currentInterval = getInterval();
    int index = intervalCombo->findText(QString::number(currentInterval));
    if (index >= 0) {
        intervalCombo->setCurrentIndex(index);
    } else {
        intervalCombo->setCurrentText("5");
    }
    layout->addWidget(intervalCombo);

    // Data Loading Strategy
    QLabel* dataLabel = new QLabel("Data Loading Strategy:", this);
    layout->addWidget(dataLabel);

    dataOptionCombo = new QComboBox(this);
    dataOptionCombo->addItem("Incrementally Manual", GetDataOption::Manual);
    dataOptionCombo->addItem("Incrementally Fill Screen (Then Manual)", GetDataOption::FillScreen);
    dataOptionCombo->addItem("Get All Data", GetDataOption::GetAll);
    dataOptionCombo->addItem("Infinite Scrolling", GetDataOption::Infinite);

    GetDataOption currentOption = getGetDataOption();
    index = dataOptionCombo->findData(currentOption);
    if (index >= 0) {
        dataOptionCombo->setCurrentIndex(index);
    }
    layout->addWidget(dataOptionCombo);

    // Notifications configuration
    QLabel* summaryThresholdLabel = new QLabel("Max notifications before summary:", this);
    layout->addWidget(summaryThresholdLabel);

    summaryThresholdCombo = new QComboBox(this);
    summaryThresholdCombo->addItems({"0", "1", "2", "3", "5", "10"});
    int currentThreshold = getSummaryThreshold();
    index = summaryThresholdCombo->findText(QString::number(currentThreshold));
    if (index >= 0) {
        summaryThresholdCombo->setCurrentIndex(index);
    } else {
        summaryThresholdCombo->setCurrentText("3");
    }
    layout->addWidget(summaryThresholdCombo);

    QLabel* notificationDelayLabel = new QLabel("Notification delay (ms):", this);
    layout->addWidget(notificationDelayLabel);

    notificationDelayCombo = new QComboBox(this);
    notificationDelayCombo->addItems({"0", "500", "1000", "1500", "2000", "5000"});
    int currentDelay = getNotificationDelayMs();
    index = notificationDelayCombo->findText(QString::number(currentDelay));
    if (index >= 0) {
        notificationDelayCombo->setCurrentIndex(index);
    } else {
        notificationDelayCombo->setCurrentText("1000");
    }
    layout->addWidget(notificationDelayCombo);

    QLabel* trayUnreadLimitLabel = new QLabel("Max unread notifications in tray:", this);
    layout->addWidget(trayUnreadLimitLabel);

    trayUnreadLimitCombo = new QComboBox(this);
    trayUnreadLimitCombo->addItems({"0", "3", "5", "10", "20"});
    int currentTrayLimit = getTrayUnreadLimit();
    index = trayUnreadLimitCombo->findText(QString::number(currentTrayLimit));
    if (index >= 0) {
        trayUnreadLimitCombo->setCurrentIndex(index);
    } else {
        trayUnreadLimitCombo->setCurrentText("5");
    }
    layout->addWidget(trayUnreadLimitCombo);

    // Startup
    autostartCheckBox = new QCheckBox("Run on startup", this);
    startMinimizedCheckBox = new QCheckBox("Start minimized (tray only)", this);

    layout->addWidget(autostartCheckBox);
    layout->addWidget(startMinimizedCheckBox);

    // Notify once
    notifyOnceCheckBox = new QCheckBox("Notify only once per notification (persists across restarts)", this);
    notifyOnceCheckBox->setChecked(getNotifyOnce());
    layout->addWidget(notifyOnceCheckBox);

    notifyReadCheckBox = new QCheckBox("Notify on read notifications", this);
    notifyReadCheckBox->setChecked(getNotifyRead());
    layout->addWidget(notifyReadCheckBox);
    QPushButton* rulesBtn = new QPushButton(tr("Manage Notification Rules..."), this);
    connect(rulesBtn, &QPushButton::clicked, this, [this]() {
        RulesDialog dialog(this);
        dialog.exec();
    });
    layout->addWidget(rulesBtn);

    // Notifications Service
    QLabel* serviceLabel = new QLabel("Notification Service:", this);
    layout->addWidget(serviceLabel);

    QPushButton* installServiceBtn = new QPushButton("Install kgithub-notify.notifyrc", this);
    connect(installServiceBtn, &QPushButton::clicked, this, &SettingsDialog::installNotifyRc);
    layout->addWidget(installServiceBtn);

    if (isAutostartEnabled()) {
        autostartCheckBox->setChecked(true);
        startMinimizedCheckBox->setEnabled(true);

        QString path =
            QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/autostart/kgithub-notify.desktop";
        QFile file(path);
        if (file.open(QIODevice::ReadOnly)) {
            QString content = file.readAll();
            if (content.contains("--background")) {
                startMinimizedCheckBox->setChecked(true);
            }
        }
    } else {
        autostartCheckBox->setChecked(false);
        startMinimizedCheckBox->setEnabled(false);
        startMinimizedCheckBox->setChecked(true);  // Default preference
    }

    connect(autostartCheckBox, &QCheckBox::toggled, startMinimizedCheckBox, &QCheckBox::setEnabled);

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* saveButton = new QPushButton("Save", this);
    saveButton->setObjectName("saveButton");
    QPushButton* cancelButton = new QPushButton("Cancel", this);

    buttonLayout->addWidget(saveButton);
    buttonLayout->addWidget(cancelButton);
    layout->addLayout(buttonLayout);

    connect(saveButton, &QPushButton::clicked, this, &SettingsDialog::onAccepted);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
}

void SettingsDialog::saveSettings() {
    QSettings settings;
    settings.setValue("interval", intervalCombo->currentText().toInt());
    settings.setValue("dataOption", dataOptionCombo->currentData().toInt());
    settings.setValue("summaryThreshold", summaryThresholdCombo->currentText().toInt());
    settings.setValue("notificationDelayMs", notificationDelayCombo->currentText().toInt());
    settings.setValue("trayUnreadLimit", trayUnreadLimitCombo->currentText().toInt());
    setNotifyOnce(notifyOnceCheckBox->isChecked());
    setNotifyRead(notifyReadCheckBox->isChecked());

    updateAutostartEntry();

    QDialog::accept();
}

void SettingsDialog::updateAutostartEntry() {
    QString error;
    bool ok = DesktopEntryHelper::writeAutostartEntry(autostartCheckBox->isChecked(),
                                                      startMinimizedCheckBox->isChecked(), QString(), &error);
    if (!ok && !error.isEmpty()) {
        statusLabel->setText(error);
        statusLabel->setStyleSheet(QStringLiteral("color: red;"));
        statusLabel->show();
    }
}

bool SettingsDialog::isAutostartEnabled() { return DesktopEntryHelper::isAutostartEnabled(); }

QFuture<WalletResult> SettingsDialog::getTokenAsync() { return WalletManager::loadTokenAsync(); }

int SettingsDialog::getInterval() {
    QSettings settings;
    return settings.value("interval", 5).toInt();
}

SettingsDialog::GetDataOption SettingsDialog::getGetDataOption() {
    QSettings settings;
    return static_cast<GetDataOption>(settings.value("dataOption", GetDataOption::Manual).toInt());
}

int SettingsDialog::getSummaryThreshold() {
    QSettings settings;
    return settings.value("summaryThreshold", 3).toInt();
}

int SettingsDialog::getNotificationDelayMs() {
    QSettings settings;
    return settings.value("notificationDelayMs", 1000).toInt();
}

int SettingsDialog::getTrayUnreadLimit() {
    QSettings settings;
    return settings.value("trayUnreadLimit", 5).toInt();
}

bool SettingsDialog::getNotifyOnce() {
    QSettings settings;
    return settings.value("notifyOnce", true).toBool();
}

void SettingsDialog::setNotifyOnce(bool notify) {
    QSettings settings;
    settings.setValue("notifyOnce", notify);
}

bool SettingsDialog::getNotifyRead() {
    QSettings settings;
    return settings.value("notifyRead", false).toBool();
}

void SettingsDialog::setNotifyRead(bool notify) {
    QSettings settings;
    settings.setValue("notifyRead", notify);
}

void SettingsDialog::onTestClicked() {
    m_verificationRequestId = QUuid();
    testButton->setEnabled(true);
    if (tokenEdit->text().isEmpty()) {
        statusLabel->setText("Please enter a token first.");
        statusLabel->setStyleSheet("color: red;");
        statusLabel->show();
        return;
    }

    if (!testClient) {
        testClient = new GitHubClient(this);
        connect(testClient, &GitHubClient::tokenVerified, this,
                [this](const QUuid& reqId, bool valid, const TokenCapabilities& caps, const QString& message) {
                    this->onVerificationResult(reqId, valid, caps, message);
                });
    }

    testClient->setToken(tokenEdit->text());
    statusLabel->setText("Testing...");
    statusLabel->setStyleSheet("color: black;");
    statusLabel->show();
    testButton->setEnabled(false);
    m_verificationRequestId = QUuid::createUuid();
    testClient->verifyToken(m_verificationRequestId);
}

void SettingsDialog::installNotifyRc() {
    QString targetDir =
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/knotifications6");
    QDir dir;
    if (!dir.mkpath(targetDir)) {
        statusLabel->setText(tr("Failed to create knotifications6 directory: %1").arg(targetDir));
        statusLabel->setStyleSheet(QStringLiteral("color: red;"));
        statusLabel->show();
        return;
    }

    QString resPath = QStringLiteral(":/knotifications6/kgithub-notify.notifyrc");
    if (!QFile::exists(resPath)) {
        resPath = QStringLiteral(":/kgithub-notify.notifyrc");
    }
    QFile sourceFile(resPath);
    if (!sourceFile.open(QIODevice::ReadOnly)) {
        statusLabel->setText(tr("Failed to read notifyrc resource."));
        statusLabel->setStyleSheet(QStringLiteral("color: red;"));
        statusLabel->show();
        return;
    }
    QByteArray content = sourceFile.readAll();
    sourceFile.close();

    QString targetPath = targetDir + QStringLiteral("/kgithub-notify.notifyrc");
    QString tempPath = targetPath + QStringLiteral(".tmp.%1").arg(QCoreApplication::applicationPid());
    QFile tempFile(tempPath);
    if (!tempFile.open(QIODevice::WriteOnly)) {
        statusLabel->setText(tr("Failed to create temporary notifyrc file."));
        statusLabel->setStyleSheet(QStringLiteral("color: red;"));
        statusLabel->show();
        return;
    }

    if (tempFile.write(content) != content.size()) {
        tempFile.close();
        QFile::remove(tempPath);
        statusLabel->setText(tr("Failed to write complete notifyrc file."));
        statusLabel->setStyleSheet(QStringLiteral("color: red;"));
        statusLabel->show();
        return;
    }
    tempFile.close();

    if (QFile::exists(targetPath) && !QFile::remove(targetPath)) {
        QFile::remove(tempPath);
        statusLabel->setText(tr("Failed to replace existing notifyrc file."));
        statusLabel->setStyleSheet(QStringLiteral("color: red;"));
        statusLabel->show();
        return;
    }

    if (!QFile::rename(tempPath, targetPath)) {
        QFile::remove(tempPath);
        statusLabel->setText(tr("Failed to install notifyrc file to %1").arg(targetPath));
        statusLabel->setStyleSheet(QStringLiteral("color: red;"));
        statusLabel->show();
        return;
    }

    statusLabel->setText(tr("Successfully installed kgithub-notify.notifyrc"));
    statusLabel->setStyleSheet(QStringLiteral("color: green;"));
    statusLabel->show();
}

void SettingsDialog::onVerificationResult(const QUuid& reqId, bool isValid, const TokenCapabilities& capabilities,
                                          const QString& error) {
    if (reqId != m_verificationRequestId) return;

    testButton->setEnabled(true);
    tokenEdit->setEnabled(true);
    if (auto* bb = findChild<QDialogButtonBox*>()) bb->button(QDialogButtonBox::Ok)->setEnabled(true);

    if (isValid) {
        QString capabilityText =
            QString("<font color='green'>Authentication Successful%1</font><br/><br/><b>Capabilities:</b><ul>")
                .arg(capabilities.login.isEmpty() ? "" : " for " + capabilities.login.toHtmlEscaped());

        auto statusToStr = [](CapabilityStatus status) -> QString {
            switch (status) {
                case CapabilityStatus::Available:
                    return "<font color='green'>Yes</font>";
                case CapabilityStatus::Limited:
                    return "<font color='orange'>Limited</font>";
                case CapabilityStatus::Unavailable:
                    return "<font color='red'>No</font>";
                case CapabilityStatus::Unknown:
                default:
                    return "<font color='gray'>Unknown</font>";
            }
        };

        capabilityText += QString("<li>Notifications: %1</li>").arg(statusToStr(capabilities.hasNotifications));
        capabilityText += QString("<li>Private Repos: %1</li>").arg(statusToStr(capabilities.hasPrivateRepos));
        capabilityText += QString("<li>Repo Metadata: %1</li>").arg(statusToStr(capabilities.hasRepoMetadata));
        capabilityText += QString("<li>Create Issues: %1</li>").arg(statusToStr(capabilities.hasCreateIssues));
        capabilityText += QString("<li>PR Comments: %1</li>").arg(statusToStr(capabilities.hasPrComments));
        capabilityText += "</ul>";

        if (capabilities.hasNotifications == CapabilityStatus::Unavailable ||
            capabilities.hasRepoMetadata == CapabilityStatus::Unavailable) {
            capabilityText +=
                "<br/><i>Note: Token is valid but lacks recommended capabilities. Features may be limited.</i>";
        }
        statusLabel->setText(capabilityText);
        statusLabel->setStyleSheet("");
    } else {
        statusLabel->setText(QString("<font color='red'>Verification failed: %1</font>").arg(error.toHtmlEscaped()));
        statusLabel->setStyleSheet("");
    }
}

void SettingsDialog::onAccepted() {
    QString token = tokenEdit->text().trimmed();

    testButton->setEnabled(false);
    tokenEdit->setEnabled(false);
    if (auto* sb = findChild<QPushButton*>("saveButton")) sb->setEnabled(false);

    if (token.isEmpty()) {
        saveWatcher->setFuture(WalletManager::clearTokenAsync());
    } else {
        saveWatcher->setFuture(WalletManager::saveTokenAsync(token));
    }
}

void SettingsDialog::onSaveFinished() {
    WalletResult result = saveWatcher->result();
    if (result.success) {
        saveSettings();
    } else {
        testButton->setEnabled(true);
        tokenEdit->setEnabled(true);
        if (auto* sb = findChild<QPushButton*>("saveButton")) sb->setEnabled(true);
        statusLabel->setText(QString("<font color='red'>Failed to save token to KWallet: %1</font><br/>"
                                     "Please try again or check your KWallet configuration.")
                                 .arg(result.errorMessage.toHtmlEscaped()));
        statusLabel->show();
    }
}
