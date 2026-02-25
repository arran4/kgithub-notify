#include "NotificationItemWidget.h"

#include <QDateTime>
#include <QFont>
#include <QLocale>
#include <QStyle>
#include <QPixmap>
#include <QPainter>
#include <QIcon>
#include <QColor>
#include <QApplication>

static QIcon getThemedIcon(const QStringList &names, QStyle *style, QStyle::StandardPixmap fallback) {
    for (const QString &name : names) {
        if (QIcon::hasThemeIcon(name)) {
            return QIcon::fromTheme(name);
        }
    }
    return style->standardIcon(fallback);
}

NotificationItemWidget::NotificationItemWidget(const Notification &n,
                                               QWidget *parent)
    : QWidget(parent), m_isLoading(false) {
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(5, 5, 5, 5);

    setMinimumHeight(60);

    // Unread Indicator
    unreadIndicator = new QLabel(this);
    unreadIndicator->setFixedSize(10, 10);
    QPixmap dot(10, 10);
    dot.fill(Qt::transparent);
    {
        QPainter p(&dot);
        p.setRenderHint(QPainter::Antialiasing);
        p.setBrush(QColor(0, 122, 255)); // Blue
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

    QVBoxLayout *contentLayout = new QVBoxLayout();

    // Title Layout (Title + Status Icons)
    QHBoxLayout *titleLayout = new QHBoxLayout();
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

    // Saved Indicator
    savedIndicator = new QLabel(this);
    QIcon savedIcon = QIcon::fromTheme("bookmark-new");
    if (savedIcon.isNull()) savedIcon = style()->standardIcon(QStyle::SP_MessageBoxQuestion);
    savedIndicator->setPixmap(savedIcon.pixmap(16, 16));
    savedIndicator->setToolTip(tr("Saved"));
    savedIndicator->hide();
    titleLayout->addWidget(savedIndicator);

    // Done Indicator
    doneIndicator = new QLabel(this);
    QIcon doneIcon = QIcon::fromTheme("task-complete");
    if (doneIcon.isNull()) doneIcon = style()->standardIcon(QStyle::SP_DialogApplyButton);
    doneIndicator->setPixmap(doneIcon.pixmap(16, 16));
    doneIndicator->setToolTip(tr("Done"));
    doneIndicator->hide();
    titleLayout->addWidget(doneIndicator);

    contentLayout->addLayout(titleLayout);

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

    // Loading Label
    loadingLabel = new QLabel(tr("Processing..."), this);
    loadingLabel->setStyleSheet("color: gray; font-style: italic;");
    loadingLabel->hide();
    contentLayout->addWidget(loadingLabel);

    mainLayout->addLayout(contentLayout);

    // Action Buttons
    QVBoxLayout *actionLayout = new QVBoxLayout();
    actionLayout->setContentsMargins(0, 0, 0, 0);
    actionLayout->setAlignment(Qt::AlignTop);

    openButton = new QToolButton(this);
    openButton->setAutoRaise(true);
    openButton->setIcon(getThemedIcon({QStringLiteral("internet-web-browser"), QStringLiteral("document-open-remote"), QStringLiteral("text-html")}, style(), QStyle::SP_DirOpenIcon));
    openButton->setIconSize(QSize(24, 24));
    openButton->setToolTip(tr("Open in Browser"));
    connect(openButton, &QToolButton::clicked, this, &NotificationItemWidget::openClicked);
    actionLayout->addWidget(openButton);

    saveButton = new QToolButton(this);
    saveButton->setAutoRaise(true);
    saveButton->setIcon(getThemedIcon({QStringLiteral("bookmark-new"), QStringLiteral("document-save")}, style(), QStyle::SP_DialogSaveButton));
    saveButton->setIconSize(QSize(24, 24));
    saveButton->setToolTip(tr("Save"));
    connect(saveButton, &QToolButton::clicked, this, &NotificationItemWidget::saveClicked);
    actionLayout->addWidget(saveButton);

    doneButton = new QToolButton(this);
    doneButton->setAutoRaise(true);
    doneButton->setIcon(getThemedIcon({QStringLiteral("task-complete"), QStringLiteral("object-select"), QStringLiteral("dialog-ok")}, style(), QStyle::SP_DialogApplyButton));
    doneButton->setIconSize(QSize(24, 24));
    doneButton->setToolTip(tr("Mark as Done"));
    connect(doneButton, &QToolButton::clicked, this, &NotificationItemWidget::doneClicked);
    actionLayout->addWidget(doneButton);

    actionLayout->addStretch();
    mainLayout->addLayout(actionLayout);

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

void NotificationItemWidget::setRead(bool read) {
    unreadIndicator->setVisible(!read);
    QFont f = titleLabel->font();
    f.setBold(!read);
    titleLabel->setFont(f);
}

void NotificationItemWidget::setSaved(bool saved) {
    savedIndicator->setVisible(saved);
    if (saved) {
        saveButton->setToolTip(tr("Unsave"));
    } else {
        saveButton->setToolTip(tr("Save"));
    }
}

void NotificationItemWidget::setDone(bool done) {
    doneIndicator->setVisible(done);
}

void NotificationItemWidget::setLoading(bool loading) {
    m_isLoading = loading;
    saveButton->setEnabled(!loading);
    doneButton->setEnabled(!loading);

    if (loading) {
        loadingLabel->show();
        errorLabel->hide();
    } else {
        loadingLabel->hide();
    }
}
