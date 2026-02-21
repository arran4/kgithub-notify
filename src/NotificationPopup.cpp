#include "NotificationPopup.h"
#include <QApplication>
#include <QStyle>
#include <QDesktopServices>
#include <QUrl>

NotificationPopup::NotificationPopup(QWidget *parent) : QWidget(parent) {
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_DeleteOnClose, false);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(5);

    // Title Label
    titleLabel = new QLabel(this);
    titleLabel->setStyleSheet("color: white; font-weight: bold; font-size: 14px;");
    mainLayout->addWidget(titleLabel);

    // Message Label
    messageLabel = new QLabel(this);
    messageLabel->setWordWrap(true);
    messageLabel->setStyleSheet("color: white;");
    mainLayout->addWidget(messageLabel);

    // Buttons Layout
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(10);
    buttonLayout->addStretch();

    openButton = new QPushButton("Open", this);
    openButton->setStyleSheet(
        "background-color: #2196F3; color: white; border: none; padding: 5px 10px; border-radius: 3px;");
    connect(openButton, &QPushButton::clicked, this, &NotificationPopup::onOpenClicked);
    buttonLayout->addWidget(openButton);

    clientButton = new QPushButton("Open Client", this);
    clientButton->setStyleSheet(
        "background-color: #4CAF50; color: white; border: none; padding: 5px 10px; border-radius: 3px;");
    connect(clientButton, &QPushButton::clicked, this, &NotificationPopup::onClientClicked);
    buttonLayout->addWidget(clientButton);

    dismissButton = new QPushButton("Dismiss", this);
    dismissButton->setStyleSheet(
        "background-color: #f44336; color: white; border: none; padding: 5px 10px; border-radius: 3px;");
    connect(dismissButton, &QPushButton::clicked, this, &NotificationPopup::onDismissClicked);
    buttonLayout->addWidget(dismissButton);

    mainLayout->addLayout(buttonLayout);

    setStyleSheet("NotificationPopup { background-color: #333; border: 1px solid #555; border-radius: 5px; }");
    setFixedWidth(350);

    // Auto-close timer
    autoCloseTimer = new QTimer(this);
    autoCloseTimer->setSingleShot(true);
    connect(autoCloseTimer, &QTimer::timeout, this, &NotificationPopup::onDismissClicked);
}

void NotificationPopup::setTitle(const QString &title) {
    titleLabel->setText(title);
}

void NotificationPopup::setMessage(const QString &message) {
    messageLabel->setText(message);
    adjustSize();
    // Restart timer when content changes
    autoCloseTimer->start(10000); // 10 seconds
}

void NotificationPopup::setOpenUrl(const QString &url) {
    currentUrl = url;
}

void NotificationPopup::showOpenButton(bool show) {
    openButton->setVisible(show);
}

void NotificationPopup::onOpenClicked() {
    autoCloseTimer->stop();
    if (!currentUrl.isEmpty()) {
        emit openUrlClicked(currentUrl);
    }
    close();
}

void NotificationPopup::onClientClicked() {
    autoCloseTimer->stop();
    emit openClientClicked();
    close();
}

void NotificationPopup::onDismissClicked() {
    autoCloseTimer->stop();
    emit dismissed();
    close();
}
