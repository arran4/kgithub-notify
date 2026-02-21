#include "SettingsDialog.h"
#include "GitHubClient.h"
#include "WalletManager.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QCoreApplication>
#include <QComboBox>
#include <QSettings>

SettingsDialog::SettingsDialog(QWidget *parent) : QDialog(parent), testClient(nullptr) {
    setWindowTitle("Settings");

    QVBoxLayout *layout = new QVBoxLayout(this);

    QLabel *label = new QLabel("GitHub Personal Access Token:", this);
    layout->addWidget(label);

    QHBoxLayout *tokenLayout = new QHBoxLayout();
    tokenEdit = new QLineEdit(this);
    tokenEdit->setEchoMode(QLineEdit::Password);
    // Load existing token
    tokenEdit->setText(getToken());
    tokenLayout->addWidget(tokenEdit);

    testButton = new QPushButton("Test Key", this);
    connect(testButton, &QPushButton::clicked, this, &SettingsDialog::onTestClicked);
    tokenLayout->addWidget(testButton);

    layout->addLayout(tokenLayout);

    statusLabel = new QLabel(this);
    statusLabel->hide();
    layout->addWidget(statusLabel);

    // Interval
    QLabel *intervalLabel = new QLabel("Refresh Interval (minutes):", this);
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

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    QPushButton *saveButton = new QPushButton("Save", this);
    QPushButton *cancelButton = new QPushButton("Cancel", this);

    buttonLayout->addWidget(saveButton);
    buttonLayout->addWidget(cancelButton);
    layout->addLayout(buttonLayout);

    connect(saveButton, &QPushButton::clicked, this, &SettingsDialog::saveSettings);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
}

void SettingsDialog::saveSettings() {
    WalletManager::saveToken(tokenEdit->text());

    QSettings settings;
    settings.setValue("interval", intervalCombo->currentText().toInt());

    accept();
}

QString SettingsDialog::getToken() {
    return WalletManager::loadToken();
}

int SettingsDialog::getInterval() {
    QSettings settings;
    return settings.value("interval", 5).toInt();
}

void SettingsDialog::onTestClicked() {
    if (tokenEdit->text().isEmpty()) {
        statusLabel->setText("Please enter a token first.");
        statusLabel->setStyleSheet("color: red;");
        statusLabel->show();
        return;
    }

    if (!testClient) {
        testClient = new GitHubClient(this);
        connect(testClient, &GitHubClient::tokenVerified, this, &SettingsDialog::onVerificationResult);
    }

    testClient->setToken(tokenEdit->text());
    statusLabel->setText("Testing...");
    statusLabel->setStyleSheet("color: black;");
    statusLabel->show();
    testButton->setEnabled(false);
    testClient->verifyToken();
}

void SettingsDialog::onVerificationResult(bool valid, const QString &message) {
    testButton->setEnabled(true);
    statusLabel->setText(message);
    if (valid) {
        statusLabel->setStyleSheet("color: green;");
    } else {
        statusLabel->setStyleSheet("color: red;");
    }
}
