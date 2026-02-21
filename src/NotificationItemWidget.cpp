#include "NotificationItemWidget.h"
#include <QFont>
#include <QDateTime>
#include <QStyle>
#include <QLocale>

NotificationItemWidget::NotificationItemWidget(const Notification &n, QWidget *parent) : QWidget(parent) {
    m_notificationId = n.id;
    m_apiUrl = n.url;

    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(5, 5, 5, 5);

    checkBox = new QCheckBox(this);
    mainLayout->addWidget(checkBox);

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

    // Repo and Type
    QHBoxLayout *repoTypeLayout = new QHBoxLayout();
    repoLabel = new QLabel(QString("Repo: <b>%1</b>").arg(n.repository), this);
    repoLabel->setTextFormat(Qt::RichText);
    typeLabel = new QLabel(QString("Type: %1").arg(n.type), this);

    repoTypeLayout->addWidget(repoLabel);
    repoTypeLayout->addSpacing(10);
    repoTypeLayout->addWidget(typeLabel);
    repoTypeLayout->addStretch();

    contentLayout->addLayout(repoTypeLayout);

    // Author/Details
    authorLabel = new QLabel("Loading details...", this);
    authorLabel->setStyleSheet("color: gray; font-style: italic;");
    contentLayout->addWidget(authorLabel);

    // Date and URL
    QHBoxLayout *dateUrlLayout = new QHBoxLayout();

    // Parse date
    QDateTime dt = QDateTime::fromString(n.updatedAt, Qt::ISODate);
    QString dateStr = dt.isValid() ? QLocale::system().toString(dt, QLocale::ShortFormat) : n.updatedAt;

    dateLabel = new QLabel("Date: " + dateStr, this);

    QString htmlUrl = GitHubClient::apiToHtmlUrl(n.url, n.id);
    QString linkText = QString("<a href=\"%1\">%1</a>").arg(htmlUrl);
    urlLabel = new QLabel(linkText, this);
    urlLabel->setWordWrap(true);
    urlLabel->setTextFormat(Qt::RichText);
    urlLabel->setOpenExternalLinks(false); // We handle link clicks manually via signal

    // Forward link activation
    connect(urlLabel, &QLabel::linkActivated, [this](const QString &link) {
        emit linkActivated(m_notificationId, m_apiUrl, link);
    });

    dateUrlLayout->addWidget(dateLabel);
    dateUrlLayout->addSpacing(10);
    dateUrlLayout->addWidget(urlLabel);

    contentLayout->addLayout(dateUrlLayout);

    mainLayout->addLayout(contentLayout);

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
}

void NotificationItemWidget::setDetails(const QString &details) {
    authorLabel->setText(details);
    // Use normal style for content
    authorLabel->setStyleSheet("");
}
