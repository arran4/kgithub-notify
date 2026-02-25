#include "NotificationItemWidget.h"

#include <QDateTime>
#include <QFont>
#include <QLocale>
#include <QStyle>
#include <QPixmap>

NotificationItemWidget::NotificationItemWidget(const Notification &n,
                                               QWidget *parent)
    : QWidget(parent) {
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(5, 5, 5, 5);

    avatarLabel = new QLabel(this);
    avatarLabel->setFixedSize(40, 40);
    // Placeholder
    QPixmap placeholder(40, 40);
    placeholder.fill(Qt::lightGray);
    avatarLabel->setPixmap(placeholder);
    mainLayout->addWidget(avatarLabel);

    checkBox = new QCheckBox(this);
    mainLayout->addWidget(checkBox);

    QVBoxLayout *contentLayout = new QVBoxLayout();

    // Title
    titleLabel = new QLabel(n.title, this);
    titleLabel->setTextFormat(Qt::PlainText);
    QFont titleFont = titleLabel->font();
    if (n.unread) {
        titleFont.setBold(true);
    }
    titleLabel->setFont(titleFont);
    titleLabel->setWordWrap(true);
    contentLayout->addWidget(titleLabel);

    // Repo, Author and Type
    QHBoxLayout *repoTypeLayout = new QHBoxLayout();
    repoLabel = new QLabel(
        QString("Repo: <b>%1</b>").arg(n.repository.toHtmlEscaped()), this);
    repoLabel->setTextFormat(Qt::RichText);

    authorLabel = new QLabel("Author: ...", this);

    typeLabel = new QLabel(QString("Type: %1").arg(n.type), this);
    typeLabel->setTextFormat(Qt::PlainText);

    repoTypeLayout->addWidget(repoLabel);
    repoTypeLayout->addSpacing(10);
    repoTypeLayout->addWidget(authorLabel);
    repoTypeLayout->addSpacing(10);
    repoTypeLayout->addWidget(typeLabel);
    repoTypeLayout->addStretch();

    contentLayout->addLayout(repoTypeLayout);

    // Date
    // Parse date
    QDateTime dt = QDateTime::fromString(n.updatedAt, Qt::ISODate);
    QString dateStr = dt.isValid()
                          ? QLocale::system().toString(dt, QLocale::ShortFormat)
                          : n.updatedAt;

    dateLabel = new QLabel("Date: " + dateStr, this);
    contentLayout->addWidget(dateLabel);

    // URL
    QString htmlUrl = GitHubClient::apiToHtmlUrl(n.url);
    urlLabel = new QLabel(QString("<a href=\"%1\">Open on GitHub</a>").arg(htmlUrl.toHtmlEscaped()), this);
    urlLabel->setTextFormat(Qt::RichText);
    urlLabel->setOpenExternalLinks(true);
    urlLabel->setTextInteractionFlags(
        Qt::TextSelectableByMouse);  // Allow selection
    contentLayout->addWidget(urlLabel);

    // Error Label
    errorLabel = new QLabel(this);
    errorLabel->setStyleSheet("color: red;");
    errorLabel->setWordWrap(true);
    errorLabel->hide();
    contentLayout->addWidget(errorLabel);

    mainLayout->addLayout(contentLayout);

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
}

void NotificationItemWidget::setAuthor(const QString &name, const QPixmap &avatar) {
    authorLabel->setText("Author: " + name);
    if (!avatar.isNull()) {
        avatarLabel->setPixmap(avatar.scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}

void NotificationItemWidget::setHtmlUrl(const QString &url) {
     urlLabel->setText(QString("<a href=\"%1\">Open on GitHub</a>").arg(url.toHtmlEscaped()));
}

void NotificationItemWidget::setError(const QString &error) {
    errorLabel->setText(tr("Error: %1").arg(error));
    errorLabel->show();
}
