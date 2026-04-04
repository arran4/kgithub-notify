#include "NotificationItemWidget.h"

#include <QApplication>
#include <QColor>
#include <QDateTime>
#include <QFont>
#include <QIcon>
#include <QLocale>
#include <QPainter>
#include <QPixmap>
#include <QStyle>

static QIcon getThemedIcon(const QStringList& names, QStyle* style, QStyle::StandardPixmap fallback) {
    for (const QString& name : names) {
        if (QIcon::hasThemeIcon(name)) {
            return QIcon::fromTheme(name);
        }
    }
    return style->standardIcon(fallback);
}

NotificationItemWidget::NotificationItemWidget(const Notification& n, QWidget* parent)
    : QWidget(parent), m_isLoading(false) {
    QVBoxLayout* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    QWidget* topWidget = new QWidget(this);
    QHBoxLayout* mainLayout = new QHBoxLayout(topWidget);
    mainLayout->setContentsMargins(5, 5, 5, 5);
    outerLayout->addWidget(topWidget);

    setMinimumHeight(60);

    // Unread Indicator
    unreadIndicator = new QLabel(this);
    unreadIndicator->setFixedSize(10, 10);
    QPixmap dot(10, 10);
    dot.fill(Qt::transparent);
    {
        QPainter p(&dot);
        p.setRenderHint(QPainter::Antialiasing);
        p.setBrush(QColor(0, 122, 255));  // Blue
        p.setPen(Qt::NoPen);
        p.drawEllipse(0, 0, 10, 10);
    }
    unreadIndicator->setPixmap(dot);
    unreadIndicator->setVisible(n.unread);
    mainLayout->addWidget(unreadIndicator);

    avatarLabel = new QLabel(this);
    avatarLabel->setFixedSize(40, 40);
    // Placeholder
    QPixmap placeholder(40, 40);
    placeholder.fill(Qt::lightGray);
    avatarLabel->setPixmap(placeholder);
    mainLayout->addWidget(avatarLabel);

    QVBoxLayout* contentLayout = new QVBoxLayout();

    // Title Layout (Title + Status Icons)
    QHBoxLayout* titleLayout = new QHBoxLayout();
    titleLayout->setContentsMargins(0, 0, 0, 0);

    titleLabel = new QLabel(n.title, this);
    titleLabel->setTextFormat(Qt::PlainText);
    QFont titleFont = titleLabel->font();
    if (n.unread) {
        titleFont.setBold(true);
    }
    titleLabel->setFont(titleFont);
    titleLabel->setWordWrap(true);
    titleLayout->addWidget(titleLabel, 1);

    contentLayout->addLayout(titleLayout);

    // Repo, Author and Type
    QHBoxLayout* repoTypeLayout = new QHBoxLayout();
    repoLabel = new QLabel(QString("Repo: <b>%1</b>").arg(n.repository.toHtmlEscaped()), this);
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
    QString dateStr = dt.isValid() ? QLocale::system().toString(dt, QLocale::ShortFormat) : n.updatedAt;

    dateLabel = new QLabel("Date: " + dateStr, this);
    contentLayout->addWidget(dateLabel);

    // URL
    QString htmlUrl = GitHubClient::apiToHtmlUrl(n.url);
    urlLabel = new QLabel(QString("<a href=\"%1\">Open on GitHub</a>").arg(htmlUrl.toHtmlEscaped()), this);
    urlLabel->setTextFormat(Qt::RichText);
    urlLabel->setOpenExternalLinks(true);
    urlLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);  // Allow selection
    contentLayout->addWidget(urlLabel);

    // Error Label
    errorLabel = new QLabel(this);
    errorLabel->setStyleSheet("color: red;");
    errorLabel->setWordWrap(true);
    errorLabel->hide();
    contentLayout->addWidget(errorLabel);

    // Loading Label
    loadingLabel = new QLabel(tr("Processing..."), this);
    loadingLabel->setStyleSheet("color: gray; font-style: italic;");
    loadingLabel->hide();
    contentLayout->addWidget(loadingLabel);

    mainLayout->addLayout(contentLayout);

    // Action Buttons
    QVBoxLayout* actionLayout = new QVBoxLayout();
    actionLayout->setContentsMargins(0, 0, 0, 0);
    actionLayout->setAlignment(Qt::AlignTop);

    expandButton = new QToolButton(this);
    expandButton->setAutoRaise(true);
    expandButton->setIcon(QIcon::fromTheme("go-down", style()->standardIcon(QStyle::SP_ArrowDown)));
    expandButton->setIconSize(QSize(24, 24));
    expandButton->setToolTip(tr("Expand Grouped Notifications"));
    expandButton->setVisible(!n.groupedNotifications.isEmpty());
    actionLayout->addWidget(expandButton);

    openButton = new QToolButton(this);
    openButton->setAutoRaise(true);
    openButton->setIcon(getThemedIcon(
        {QStringLiteral("internet-web-browser"), QStringLiteral("document-open-remote"), QStringLiteral("text-html")},
        style(), QStyle::SP_DirOpenIcon));
    openButton->setIconSize(QSize(24, 24));
    openButton->setToolTip(tr("Open in Browser"));
    connect(openButton, &QToolButton::clicked, this, &NotificationItemWidget::openClicked);
    actionLayout->addWidget(openButton);

    doneButton = new QToolButton(this);
    doneButton->setAutoRaise(true);
    doneButton->setIcon(
        getThemedIcon({QStringLiteral("task-complete"), QStringLiteral("object-select"), QStringLiteral("dialog-ok")},
                      style(), QStyle::SP_DialogApplyButton));
    doneButton->setIconSize(QSize(24, 24));
    doneButton->setToolTip(tr("Mark as Done"));
    connect(doneButton, &QToolButton::clicked, this, &NotificationItemWidget::doneClicked);
    actionLayout->addWidget(doneButton);

    actionLayout->addStretch();
    mainLayout->addLayout(actionLayout);

    childrenContainer = new QWidget(this);
    QVBoxLayout* childrenLayout = new QVBoxLayout(childrenContainer);
    childrenLayout->setContentsMargins(40, 0, 5, 5);  // Indent children
    childrenLayout->setSpacing(2);

    if (!n.groupedNotifications.isEmpty()) {
        for (const auto& child : n.groupedNotifications) {
            QString htmlUrl = GitHubClient::apiToHtmlUrl(child.url);
            if (!child.htmlUrl.isEmpty()) {
                htmlUrl = child.htmlUrl;
            }

            QWidget* childWidget = new QWidget(this);
            QHBoxLayout* childLayout = new QHBoxLayout(childWidget);
            childLayout->setContentsMargins(0, 0, 0, 0);

            QLabel* childUnread = new QLabel(this);
            childUnread->setFixedSize(6, 6);
            if (child.unread) {
                QPixmap dot(6, 6);
                dot.fill(Qt::transparent);
                QPainter p(&dot);
                p.setRenderHint(QPainter::Antialiasing);
                p.setBrush(QColor(0, 122, 255));
                p.setPen(Qt::NoPen);
                p.drawEllipse(0, 0, 6, 6);
                childUnread->setPixmap(dot);
            } else {
                childUnread->setVisible(false);
            }
            childLayout->addWidget(childUnread);

            QLabel* childLabel = new QLabel(QString("↳ <a href=\"%1\"><b>%2</b>: %3</a>")
                                                .arg(htmlUrl.toHtmlEscaped(), child.type, child.title.toHtmlEscaped()),
                                            this);
            childLabel->setTextFormat(Qt::RichText);
            childLabel->setWordWrap(true);
            childLabel->setOpenExternalLinks(false);
            connect(childLabel, &QLabel::linkActivated, this,
                    [this](const QString& link) { emit childOpenClicked(link); });
            childLayout->addWidget(childLabel, 1);

            QToolButton* copyBtn = new QToolButton(this);
            copyBtn->setIcon(QIcon::fromTheme("edit-copy"));
            copyBtn->setToolTip(tr("Copy Link"));
            copyBtn->setIconSize(QSize(16, 16));
            copyBtn->setAutoRaise(true);
            connect(copyBtn, &QToolButton::clicked, this, [this, htmlUrl]() { emit childCopyClicked(htmlUrl); });
            childLayout->addWidget(copyBtn);

            QToolButton* readBtn = new QToolButton(this);
            readBtn->setIcon(QIcon::fromTheme("mail-mark-read"));
            readBtn->setToolTip(tr("Mark as Read"));
            readBtn->setIconSize(QSize(16, 16));
            readBtn->setAutoRaise(true);
            readBtn->setVisible(child.unread);
            connect(readBtn, &QToolButton::clicked, this, [this, child, childUnread, readBtn]() {
                emit childMarkAsReadClicked(child.id);
                childUnread->setVisible(false);
                readBtn->setVisible(false);
            });
            childLayout->addWidget(readBtn);

            QToolButton* doneBtn = new QToolButton(this);
            doneBtn->setIcon(QIcon::fromTheme("task-complete"));
            doneBtn->setToolTip(tr("Mark as Done"));
            doneBtn->setIconSize(QSize(16, 16));
            doneBtn->setAutoRaise(true);
            connect(doneBtn, &QToolButton::clicked, this, [this, child, childWidget]() {
                emit childMarkAsDoneClicked(child.id);
                childWidget->setVisible(false);
                emit heightChanged();
            });
            childLayout->addWidget(doneBtn);

            childrenLayout->addWidget(childWidget);
        }
    }

    childrenContainer->setVisible(false);
    outerLayout->addWidget(childrenContainer);

    connect(expandButton, &QToolButton::clicked, this, [this]() {
        bool isVisible = childrenContainer->isVisible();
        childrenContainer->setVisible(!isVisible);
        expandButton->setIcon(
            QIcon::fromTheme(isVisible ? "go-down" : "go-up",
                             style()->standardIcon(isVisible ? QStyle::SP_ArrowDown : QStyle::SP_ArrowUp)));
        emit heightChanged();
    });

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
}

void NotificationItemWidget::setAuthor(const QString& name, const QPixmap& avatar) {
    authorLabel->setText("Author: " + name);
    if (!avatar.isNull()) {
        avatarLabel->setPixmap(avatar.scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}

void NotificationItemWidget::setHtmlUrl(const QString& url) {
    urlLabel->setText(QString("<a href=\"%1\">Open on GitHub</a>").arg(url.toHtmlEscaped()));
}

void NotificationItemWidget::setError(const QString& error) {
    errorLabel->setText(tr("Error: %1").arg(error));
    errorLabel->show();
}

void NotificationItemWidget::setRead(bool read) {
    unreadIndicator->setVisible(!read);
    QFont f = titleLabel->font();
    f.setBold(!read);
    titleLabel->setFont(f);
}

void NotificationItemWidget::setLoading(bool loading) {
    m_isLoading = loading;
    doneButton->setEnabled(!loading);

    if (loading) {
        loadingLabel->show();
        errorLabel->hide();
    } else {
        loadingLabel->hide();
    }
}

void NotificationItemWidget::updateNotification(const Notification& n) {
    if (titleLabel->text() != n.title) {
        titleLabel->setText(n.title);
    }

    QString repoText = QString("Repo: <b>%1</b>").arg(n.repository.toHtmlEscaped());
    if (repoLabel->text() != repoText) {
        repoLabel->setText(repoText);
    }

    QString typeText = QString("Type: %1").arg(n.type);
    if (typeLabel->text() != typeText) {
        typeLabel->setText(typeText);
    }

    QDateTime dt = QDateTime::fromString(n.updatedAt, Qt::ISODate);
    QString dateStr = dt.isValid() ? QLocale::system().toString(dt, QLocale::ShortFormat) : n.updatedAt;
    QString dateLabelText = "Date: " + dateStr;
    if (dateLabel->text() != dateLabelText) {
        dateLabel->setText(dateLabelText);
    }

    // Update read status
    setRead(!n.unread);

    // Note: Author and Avatar are updated separately via updateDetails/updateImage signals
    // Note: URL is often updated via details, but we can set the base one here
    QString htmlUrl = GitHubClient::apiToHtmlUrl(n.url);
    if (!n.htmlUrl.isEmpty()) {
        htmlUrl = n.htmlUrl;
    }
    QString urlText = QString("<a href=\"%1\">Open on GitHub</a>").arg(htmlUrl.toHtmlEscaped());
    if (urlLabel->text() != urlText) {
        urlLabel->setText(urlText);
    }
}
