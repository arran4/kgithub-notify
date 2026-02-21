#include "NotificationItemWidget.h"
#include "NotificationTextFormatter.h"

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
    TextWithFormat titleData = NotificationTextFormatter::formatTitle(n);
    titleLabel = new QLabel(titleData.text, this);
    titleLabel->setTextFormat(titleData.format);

    QFont titleFont = titleLabel->font();
    if (n.unread) {
        titleFont.setBold(true);
    }
    titleLabel->setFont(titleFont);
    titleLabel->setWordWrap(true);
    contentLayout->addWidget(titleLabel);

    // Repo, Author and Type
    QHBoxLayout *repoTypeLayout = new QHBoxLayout();

    TextWithFormat repoData = NotificationTextFormatter::formatRepo(n);
    repoLabel = new QLabel(repoData.text, this);
    repoLabel->setTextFormat(repoData.format);

    authorLabel = new QLabel("Author: ...", this);

    TextWithFormat typeData = NotificationTextFormatter::formatType(n);
    typeLabel = new QLabel(typeData.text, this);
    typeLabel->setTextFormat(typeData.format);

    repoTypeLayout->addWidget(repoLabel);
    repoTypeLayout->addSpacing(10);
    repoTypeLayout->addWidget(authorLabel);
    repoTypeLayout->addSpacing(10);
    repoTypeLayout->addWidget(typeLabel);
    repoTypeLayout->addStretch();

    contentLayout->addLayout(repoTypeLayout);

    // Date and URL
    QHBoxLayout *dateUrlLayout = new QHBoxLayout();

    // Date
    TextWithFormat dateData = NotificationTextFormatter::formatDate(n);
    dateLabel = new QLabel(dateData.text, this);
    dateLabel->setTextFormat(dateData.format);

    // URL
    TextWithFormat urlData = NotificationTextFormatter::formatUrl(n);
    urlLabel = new QLabel(urlData.text, this);
    urlLabel->setTextFormat(urlData.format);
    urlLabel->setOpenExternalLinks(true);
    urlLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);  // Allow selection

    dateUrlLayout->addWidget(dateLabel);
    dateUrlLayout->addSpacing(10);
    dateUrlLayout->addWidget(urlLabel);

    contentLayout->addLayout(dateUrlLayout);

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
    TextWithFormat urlData = NotificationTextFormatter::formatUrlStr(url);
    urlLabel->setText(urlData.text);
    urlLabel->setTextFormat(urlData.format);
}
