#include "PopupNotification.h"

#include <QApplication>
#include <QStyle>

PopupNotification::PopupNotification(QWidget *parent) : QWidget(parent) {
    // Set window flags for a tooltip-like, topmost, frameless window
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_DeleteOnClose, false);  // We manage showing/hiding

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(5);

    // Message Label
    messageLabel = new QLabel(this);
    messageLabel->setWordWrap(true);
    messageLabel->setStyleSheet("color: white; font-weight: bold;");
    mainLayout->addWidget(messageLabel);

    // Buttons Layout
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(10);
    buttonLayout->addStretch();

    settingsButton = new QPushButton("Settings", this);
    settingsButton->setStyleSheet(
        "background-color: #4CAF50; color: white; border: none; padding: 5px 10px; border-radius: 3px;");
    connect(settingsButton, &QPushButton::clicked, this, &PopupNotification::onSettingsClicked);
    buttonLayout->addWidget(settingsButton);

    dismissButton = new QPushButton("Dismiss", this);
    dismissButton->setStyleSheet(
        "background-color: #f44336; color: white; border: none; padding: 5px 10px; border-radius: 3px;");
    connect(dismissButton, &QPushButton::clicked, this, &PopupNotification::onDismissClicked);
    buttonLayout->addWidget(dismissButton);

    mainLayout->addLayout(buttonLayout);

    // General styling
    setStyleSheet("PopupNotification { background-color: #333; border: 1px solid #555; border-radius: 5px; }");

    // Set fixed width to avoid being too wide or narrow
    setFixedWidth(300);
}

void PopupNotification::setMessage(const QString &message) {
    messageLabel->setText(message);
    adjustSize();
}

void PopupNotification::setSettingsVisible(bool visible) {
    settingsButton->setVisible(visible);
}

void PopupNotification::onSettingsClicked() {
    emit settingsClicked();
    close();
}

void PopupNotification::onDismissClicked() {
    emit dismissed();
    close();
}
