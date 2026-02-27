#include "SettingsDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QTextStream>
#include <QVBoxLayout>
#include <QFutureWatcher>

#include "GitHubClient.h"
#include "WalletManager.h"

SettingsDialog::SettingsDialog(QWidget *parent) : QDialog(parent), testClient(nullptr) {
    setWindowTitle("Settings");

    QVBoxLayout *layout = new QVBoxLayout(this);

    QLabel *label = new QLabel("GitHub Personal Access Token:", this);
    layout->addWidget(label);

    QHBoxLayout *tokenLayout = new QHBoxLayout();
    tokenEdit = new QLineEdit(this);
    tokenEdit->setEchoMode(QLineEdit::Password);

    // Load existing token asynchronously
    tokenEdit->setEnabled(false);
    tokenEdit->setPlaceholderText("Loading...");

    QFutureWatcher<QString> *watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher]() {
        tokenEdit->setText(watcher->result());
        tokenEdit->setEnabled(true);
        tokenEdit->setPlaceholderText("");
        watcher->deleteLater();
    });
    watcher->setFuture(getTokenAsync());

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

    // Data Loading Strategy
    QLabel *dataLabel = new QLabel("Data Loading Strategy:", this);
    layout->addWidget(dataLabel);

    dataOptionCombo = new QComboBox(this);
    dataOptionCombo->addItem("Incrementally Manual", GetDataOption::Manual);
    dataOptionCombo->addItem("Incrementally Fill Screen", GetDataOption::FillScreen);
    dataOptionCombo->addItem("Get All Data", GetDataOption::GetAll);
    dataOptionCombo->addItem("Infinite Scrolling", GetDataOption::Infinite);

    GetDataOption currentOption = getGetDataOption();
    index = dataOptionCombo->findData(currentOption);
    if (index >= 0) {
        dataOptionCombo->setCurrentIndex(index);
    }
    layout->addWidget(dataOptionCombo);

    // Startup
    autostartCheckBox = new QCheckBox("Run on startup", this);
    startMinimizedCheckBox = new QCheckBox("Start minimized (tray only)", this);

    layout->addWidget(autostartCheckBox);
    layout->addWidget(startMinimizedCheckBox);

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
    settings.setValue("dataOption", dataOptionCombo->currentData().toInt());

    updateAutostartEntry();

    accept();
}

void SettingsDialog::updateAutostartEntry() {
    QString configPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    QDir dir(configPath);
    if (!dir.exists("autostart")) {
        dir.mkdir("autostart");
    }
    QString path = configPath + "/autostart/kgithub-notify.desktop";

    if (autostartCheckBox->isChecked()) {
        QFile file(path);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << "[Desktop Entry]\n";
            out << "Type=Application\n";
            out << "Name=KGitHub Notify\n";
            out << "Comment=GitHub Notification System Tray\n";
            QString exec = QCoreApplication::applicationFilePath();
            if (startMinimizedCheckBox->isChecked()) {
                exec += " --background";
            }
            out << "Exec=" << exec << "\n";
            out << "Icon=kgithub-notify\n";
            out << "Categories=Development;Utility;Qt;KDE;\n";
            out << "StartupWMClass=Kgithub-notify\n";
            out << "Terminal=false\n";
            out << "X-KDE-autostart-after=panel\n";
        }
    } else {
        QFile::remove(path);
    }
}

bool SettingsDialog::isAutostartEnabled() {
    QString path =
        QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/autostart/kgithub-notify.desktop";
    return QFile::exists(path);
}

QString SettingsDialog::getToken() { return WalletManager::loadToken(); }

QFuture<QString> SettingsDialog::getTokenAsync() { return WalletManager::loadTokenAsync(); }

int SettingsDialog::getInterval() {
    QSettings settings;
    return settings.value("interval", 5).toInt();
}

SettingsDialog::GetDataOption SettingsDialog::getGetDataOption() {
    QSettings settings;
    return static_cast<GetDataOption>(settings.value("dataOption", GetDataOption::Manual).toInt());
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
