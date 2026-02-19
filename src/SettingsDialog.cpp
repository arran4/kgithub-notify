#include "SettingsDialog.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QHBoxLayout>

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
    QSettings settings("Kgithub-notify", "Kgithub-notify");
    settings.setValue("token", tokenEdit->text());
    accept();
}

QString SettingsDialog::getToken() {
    QSettings settings("Kgithub-notify", "Kgithub-notify");
    return settings.value("token").toString();
}
