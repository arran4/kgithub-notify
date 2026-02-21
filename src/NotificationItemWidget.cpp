#include "NotificationItemWidget.h"
#include <QFont>
#include <QDateTime>
#include <QStyle>
#include <QLocale>

NotificationItemWidget::NotificationItemWidget(const Notification &n, QWidget *parent) : QWidget(parent) {
    m_notificationId = n.id;
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(5, 5, 5, 5);

    checkBox = new QCheckBox(this);
    mainLayout->addWidget(checkBox);

    // Avatar
    avatarLabel = new QLabel(this);
    avatarLabel->setFixedSize(40, 40);
    // Placeholder style
    avatarLabel->setStyleSheet("background-color: #f0f0f0; border-radius: 4px; border: 1px solid #e1e4e8;");
    avatarLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(avatarLabel);

    QVBoxLayout *contentLayout = new QVBoxLayout();

    // Title
    titleLabel = new QLabel(n.title, this);
    QFont titleFont = titleLabel->font();
    if (n.unread) {
        titleFont.setBold(true);
    }
    titleLabel->setFont(titleFont);
    titleLabel->setWordWrap(true);
    contentLayout->addWidget(titleLabel);

    // Repo, Type, Author
    QHBoxLayout *metaLayout = new QHBoxLayout();
    repoLabel = new QLabel(QString("<b>%1</b>").arg(n.repository), this);
    repoLabel->setTextFormat(Qt::RichText);

    typeLabel = new QLabel(n.type, this);
    typeLabel->setStyleSheet("color: #586069;"); // GitHub gray

    authorLabel = new QLabel(this); // Empty initially
    authorLabel->setStyleSheet("color: #586069; font-style: italic;");

    metaLayout->addWidget(repoLabel);
    metaLayout->addSpacing(10);
    metaLayout->addWidget(typeLabel);
    metaLayout->addSpacing(10);
    metaLayout->addWidget(authorLabel);
    metaLayout->addStretch();

    contentLayout->addLayout(metaLayout);

    // Date and Link
    QHBoxLayout *bottomLayout = new QHBoxLayout();

    // Parse date
    QDateTime dt = QDateTime::fromString(n.updatedAt, Qt::ISODate);
    QString dateStr = dt.isValid() ? QLocale::system().toString(dt, QLocale::ShortFormat) : n.updatedAt;
    dateLabel = new QLabel(dateStr, this);
    dateLabel->setStyleSheet("color: #586069; font-size: 11px;");

    // Rich Link
    QString htmlUrl = GitHubClient::apiToHtmlUrl(n.url);
    QString linkText = "View";
    // Try to extract #number
    // url format: .../issues/123 or .../pulls/123
    int lastSlash = n.url.lastIndexOf('/');
    if (lastSlash != -1) {
        QString idStr = n.url.mid(lastSlash + 1);
        bool ok;
        idStr.toInt(&ok);
        if (ok) {
            linkText = "#" + idStr;
        }
    }

    urlLabel = new QLabel(QString("<a href=\"%1\" style=\"text-decoration:none; color:#0366d6;\">%2</a>").arg(htmlUrl, linkText), this);
    urlLabel->setTextFormat(Qt::RichText);
    urlLabel->setOpenExternalLinks(true);
    urlLabel->setCursor(Qt::PointingHandCursor);

    bottomLayout->addWidget(dateLabel);
    bottomLayout->addStretch();
    bottomLayout->addWidget(urlLabel);

    contentLayout->addLayout(bottomLayout);

    mainLayout->addLayout(contentLayout);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
}

void NotificationItemWidget::setAuthor(const QString &name, const QPixmap &avatar) {
    if (!name.isEmpty()) {
        authorLabel->setText("by " + name);
    }
    if (!avatar.isNull()) {
        avatarLabel->setPixmap(avatar.scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        avatarLabel->setStyleSheet(""); // Remove placeholder
    }
}

void NotificationItemWidget::updateFromNotification(const Notification &n) {
    if (m_notificationId != n.id) return;

    titleLabel->setText(n.title);
    QFont f = titleLabel->font();
    f.setBold(n.unread);
    titleLabel->setFont(f);

    QDateTime dt = QDateTime::fromString(n.updatedAt, Qt::ISODate);
    QString dateStr = dt.isValid() ? QLocale::system().toString(dt, QLocale::ShortFormat) : n.updatedAt;
    dateLabel->setText(dateStr);
}
