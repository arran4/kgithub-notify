#include "SettingsDialog.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QCoreApplication>
#include <KWallet>

SettingsDialog::SettingsDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle("Settings");

    QVBoxLayout *layout = new QVBoxLayout(this);

    QLabel *label = new QLabel("GitHub Personal Access Token:", this);
    layout->addWidget(label);

    tokenEdit = new QLineEdit(this);
    tokenEdit->setEchoMode(QLineEdit::Password);

    // Load existing token
    tokenEdit->setText(getToken());

    layout->addWidget(tokenEdit);

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
    KWallet::Wallet *wallet = KWallet::Wallet::openWallet(KWallet::Wallet::LocalWallet(), 0, KWallet::Wallet::Synchronous);
    if (wallet) {
        if (!wallet->hasFolder("Kgithub-notify")) {
             wallet->createFolder("Kgithub-notify");
        }
        wallet->setFolder("Kgithub-notify");
        wallet->writePassword("token", tokenEdit->text());
        delete wallet;
    }
    accept();
}

QString SettingsDialog::getToken() {
    KWallet::Wallet *wallet = KWallet::Wallet::openWallet(KWallet::Wallet::LocalWallet(), 0, KWallet::Wallet::Synchronous);
    if (wallet) {
        if (!wallet->hasFolder("Kgithub-notify")) {
             wallet->createFolder("Kgithub-notify");
        }
        wallet->setFolder("Kgithub-notify");
        QString token;
        wallet->readPassword("token", token);
        delete wallet;
        return token;
    }
    return QString();
}
