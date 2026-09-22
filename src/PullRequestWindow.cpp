#include "PullRequestWindow.h"

#include <KActionCollection>
#include <KStandardAction>
#include <QDateTime>
#include <QDesktopServices>
#include <QFontDatabase>
#include <QFrame>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QStatusBar>
#include <QTextEdit>
#include <QUrl>

#include "utils/UrlHelper.h"

class CommentWidget : public QWidget {
    Q_OBJECT
   public:
    explicit CommentWidget(const QString& author, const QString& body, const QString& formattedDate,
                           QWidget* parent = nullptr);
};

CommentWidget::CommentWidget(const QString& author, const QString& body, const QString& formattedDate, QWidget* parent)
    : QWidget(parent) {
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    // Header
    QWidget* headerWidget = new QWidget(this);
    QHBoxLayout* headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(0, 0, 0, 0);

    QPushButton* toggleBtn = new QPushButton(QStringLiteral("-"), headerWidget);
    toggleBtn->setFixedSize(20, 20);

    QLabel* headerLabel = new QLabel(tr("**%1** on %2").arg(author, formattedDate), headerWidget);
    headerLabel->setTextFormat(Qt::MarkdownText);

    QPushButton* detachBtn = new QPushButton(tr("Detach"), headerWidget);

    headerLayout->addWidget(toggleBtn);
    headerLayout->addWidget(headerLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(detachBtn);

    layout->addWidget(headerWidget);

    // Body
    QLabel* bodyLabel = new QLabel(body, this);
    bodyLabel->setTextFormat(Qt::MarkdownText);
    bodyLabel->setWordWrap(true);
    bodyLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    bodyLabel->setOpenExternalLinks(true);

    layout->addWidget(bodyLabel);

    // Divider
    QFrame* divider = new QFrame(this);
    divider->setFrameShape(QFrame::HLine);
    divider->setFrameShadow(QFrame::Sunken);
    layout->addWidget(divider);

    // Connections
    connect(toggleBtn, &QPushButton::clicked, this, [toggleBtn, bodyLabel]() {
        bool visible = !bodyLabel->isVisible();
        bodyLabel->setVisible(visible);
        toggleBtn->setText(visible ? QStringLiteral("-") : QStringLiteral("+"));
    });

    connect(detachBtn, &QPushButton::clicked, this, [author, body, this]() {
        QWidget* detachedWindow = new QWidget(this, Qt::Window);
        detachedWindow->setAttribute(Qt::WA_DeleteOnClose);
        detachedWindow->setObjectName(QStringLiteral("DetachedCommentWindow"));

        QLabel* label = new QLabel(body, detachedWindow);
        label->setTextFormat(Qt::MarkdownText);
        label->setWordWrap(true);
        label->setTextInteractionFlags(Qt::TextBrowserInteraction);
        label->setOpenExternalLinks(true);

        QScrollArea* scrollArea = new QScrollArea(detachedWindow);
        scrollArea->setWidgetResizable(true);
        scrollArea->setWidget(label);

        QVBoxLayout* winLayout = new QVBoxLayout(detachedWindow);
        winLayout->setContentsMargins(0, 0, 0, 0);
        winLayout->addWidget(scrollArea);
        detachedWindow->setWindowTitle(tr("Comment by %1").arg(author));
        detachedWindow->resize(600, 400);
        detachedWindow->show();
    });
}

#include "PullRequestWindow.moc"

PullRequestWindow::PullRequestWindow(const Notification& n, GitHubClient* client, QWidget* parent,
                                     QNetworkAccessManager* networkManager)
    : KXmlGuiWindow(parent, Qt::Window),
      m_notification(n),
      m_client(client),
      m_manager(networkManager ? networkManager : new QNetworkAccessManager(this)) {
    setWindowTitle(tr("Pull Request - %1").arg(n.title));
    resize(800, 600);

    setupUi();

    m_requestStatus = new QLabel(this);
    m_requestStatus->setTextFormat(Qt::PlainText);
    statusBar()->addWidget(m_requestStatus, 1);
    m_retryButton = new QPushButton(tr("Retry"), this);
    statusBar()->addPermanentWidget(m_retryButton);
    connect(m_retryButton, &QPushButton::clicked, this, &PullRequestWindow::fetchPrDetails);
    fetchPrDetails();
}

void PullRequestWindow::setupUi() {
    m_tabWidget = new QTabWidget(this);
    setObjectName("PullRequestWindow");
    setCentralWidget(m_tabWidget);
    setupMenus();
    setupGUI(Default, ":/kgithub-notifyui.rc");

    // 1. Conversation Tab
    m_conversationTab = new QWidget();
    m_conversationLayout = new QHBoxLayout(m_conversationTab);

    // LHS - Comments
    QWidget* leftConvWidget = new QWidget();
    QVBoxLayout* leftConvLayout = new QVBoxLayout(leftConvWidget);
    leftConvLayout->setContentsMargins(0, 0, 0, 0);

    m_conversationStatusLabel = new QLabel();
    m_conversationStatusLabel->hide();
    m_conversationRetryBtn = new QPushButton(tr("Retry"));
    m_conversationRetryBtn->hide();
    QHBoxLayout* convStatusLayout = new QHBoxLayout();
    convStatusLayout->addWidget(m_conversationStatusLabel);
    convStatusLayout->addWidget(m_conversationRetryBtn);
    convStatusLayout->addStretch();
    leftConvLayout->addLayout(convStatusLayout);

    connect(m_conversationRetryBtn, &QPushButton::clicked, this, [this]() {
        if (m_timelineState.isFailed) fetchTimeline();
        if (m_reviewState.isFailed) fetchReviewComments();
    });

    m_commentsScrollArea = new QScrollArea();
    m_commentsScrollArea->setWidgetResizable(true);
    m_commentsContainer = new QWidget();
    m_commentsContainerLayout = new QVBoxLayout(m_commentsContainer);
    m_commentsContainerLayout->setAlignment(Qt::AlignTop);
    m_commentsContainer->setLayout(m_commentsContainerLayout);
    m_commentsScrollArea->setWidget(m_commentsContainer);

    leftConvLayout->addWidget(m_commentsScrollArea);

    m_replyEdit = new QTextEdit();
    m_replyEdit->setPlaceholderText(tr("Leave a comment..."));
    m_replyEdit->setMaximumHeight(100);
    leftConvLayout->addWidget(m_replyEdit);

    m_commentButton = new QPushButton(tr("Comment"));
    connect(m_commentButton, &QPushButton::clicked, this, &PullRequestWindow::onCommentButtonClicked);
    leftConvLayout->addWidget(m_commentButton, 0, Qt::AlignRight);

    m_conversationLayout->addWidget(leftConvWidget, 1);

    // RHS - Metadata/Tool Window
    QWidget* rightConvWidget = new QWidget();
    rightConvWidget->setFixedWidth(250);
    QVBoxLayout* rhsLayout = new QVBoxLayout(rightConvWidget);
    rhsLayout->setContentsMargins(0, 0, 0, 0);

    m_openedByLabel = new QLabel(tr("<b>Opened by:</b> Loading..."));
    m_openedByLabel->setWordWrap(true);
    m_createdAtLabel = new QLabel(tr("<b>Created:</b> Loading..."));
    m_createdAtLabel->setWordWrap(true);
    m_updatedAtLabel = new QLabel(tr("<b>Updated:</b> Loading..."));
    m_updatedAtLabel->setWordWrap(true);
    m_convAssigneesLabel = new QLabel(tr("<b>Assignees:</b> Loading..."));
    m_convAssigneesLabel->setWordWrap(true);
    m_convLabelsLabel = new QLabel(tr("<b>Labels:</b> Loading..."));
    m_convLabelsLabel->setWordWrap(true);
    m_convMilestoneLabel = new QLabel(tr("<b>Milestone:</b> Loading..."));
    m_convMilestoneLabel->setWordWrap(true);

    rhsLayout->addWidget(m_openedByLabel);
    rhsLayout->addWidget(m_createdAtLabel);
    rhsLayout->addWidget(m_updatedAtLabel);

    QFrame* divider1 = new QFrame();
    divider1->setFrameShape(QFrame::HLine);
    divider1->setFrameShadow(QFrame::Sunken);
    rhsLayout->addWidget(divider1);

    rhsLayout->addWidget(m_convAssigneesLabel);
    rhsLayout->addWidget(m_convLabelsLabel);
    rhsLayout->addWidget(m_convMilestoneLabel);
    rhsLayout->addStretch();

    m_conversationLayout->addWidget(rightConvWidget);

    m_tabWidget->addTab(m_conversationTab, tr("Conversation"));

    // 2. Commits Tab
    m_commitsTab = new QWidget();
    m_commitsLayout = new QVBoxLayout(m_commitsTab);

    m_commitsStatusLabel = new QLabel();
    m_commitsStatusLabel->hide();
    m_commitsRetryBtn = new QPushButton(tr("Retry"));
    m_commitsRetryBtn->hide();
    QHBoxLayout* commitsStatusLayout = new QHBoxLayout();
    commitsStatusLayout->addWidget(m_commitsStatusLabel);
    commitsStatusLayout->addWidget(m_commitsRetryBtn);
    commitsStatusLayout->addStretch();
    m_commitsLayout->addLayout(commitsStatusLayout);

    connect(m_commitsRetryBtn, &QPushButton::clicked, this, [this]() {
        if (m_commitsState.isFailed) fetchCommits();
    });
    m_commitsTable = new QTableWidget();
    m_commitsTable->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_commitsTable->setColumnCount(4);
    m_commitsTable->setHorizontalHeaderLabels({tr("SHA"), tr("Author"), tr("Message"), tr("Date")});
    m_commitsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_commitsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_commitsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_commitsLayout->addWidget(m_commitsTable);
    m_tabWidget->addTab(m_commitsTab, tr("Commits"));

    // 3. Changed Files Tab
    m_filesTab = new QWidget();
    m_filesLayout = new QVBoxLayout(m_filesTab);

    m_filesStatusLabel = new QLabel();
    m_filesStatusLabel->hide();
    m_filesRetryBtn = new QPushButton(tr("Retry"));
    m_filesRetryBtn->hide();
    QHBoxLayout* filesStatusLayout = new QHBoxLayout();
    filesStatusLayout->addWidget(m_filesStatusLabel);
    filesStatusLayout->addWidget(m_filesRetryBtn);
    filesStatusLayout->addStretch();
    m_filesLayout->addLayout(filesStatusLayout);

    connect(m_filesRetryBtn, &QPushButton::clicked, this, [this]() {
        if (m_filesState.isFailed) fetchFiles();
    });
    m_filesTable = new QTableWidget();
    m_filesTable->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_filesTable->setColumnCount(4);
    m_filesTable->setHorizontalHeaderLabels({tr("Filename"), tr("Additions"), tr("Deletions"), tr("Changes")});
    m_filesTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_filesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_filesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_filesLayout->addWidget(m_filesTable);
    connect(m_filesTable, &QTableWidget::cellDoubleClicked, this, &PullRequestWindow::onFileDoubleClicked);
    m_tabWidget->addTab(m_filesTab, tr("Changed Files"));

    // 4. Metadata Tab
    m_metadataTab = new QWidget();
    m_metadataLayout = new QVBoxLayout(m_metadataTab);

    m_labelsLabel = new QLabel(tr("<b>Labels:</b> Loading..."));
    m_labelsLabel->setWordWrap(true);
    m_assigneesLabel = new QLabel(tr("<b>Assignees:</b> Loading..."));
    m_assigneesLabel->setWordWrap(true);
    m_milestoneLabel = new QLabel(tr("<b>Milestone:</b> Loading..."));
    m_milestoneLabel->setWordWrap(true);

    m_metadataLayout->addWidget(m_labelsLabel);
    m_metadataLayout->addWidget(m_assigneesLabel);
    m_metadataLayout->addWidget(m_milestoneLabel);
    m_metadataLayout->addStretch();

    m_tabWidget->addTab(m_metadataTab, tr("Metadata"));
}

void PullRequestWindow::fetchPrDetails() {
    m_detailsGeneration = QUuid::createUuid();
    m_requestStatus->setText(tr("Loading PR details..."));
    m_retryButton->setEnabled(false);
    QUrl url(m_notification.url);
    QNetworkRequest request = m_client->createAuthenticatedRequest(url);
    QNetworkReply* reply = m_manager->get(request);
    reply->setProperty("generation", m_detailsGeneration);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { onPrDetailsReply(reply); });
}

void PullRequestWindow::onPrDetailsReply(QNetworkReply* reply) {
    if (reply->property("generation").toUuid() != m_detailsGeneration) {
        reply->deleteLater();
        return;
    }
    m_retryButton->setEnabled(true);
    if (reply->error() == QNetworkReply::NoError) {
        m_requestStatus->setText(tr("PR details loaded."));
        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        m_rawJsonStr = QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
        if (QAction* action = actionCollection()->action(QStringLiteral("view_raw_json"))) {
            action->setEnabled(true);
        }
        QJsonObject obj = doc.object();

        m_commentsUrl = obj["comments_url"].toString();
        m_reviewCommentsUrl = obj["review_comments_url"].toString();
        m_commitsUrl = obj["commits_url"].toString();

        // Convert issues url to get issue comments url for PR
        QString issueUrl = obj["issue_url"].toString();
        m_issueCommentsUrl = issueUrl + "/comments";
        m_timelineUrl = issueUrl + "/timeline";

        m_timelineState = CollectionState();
        m_timelineState.generation = QUuid::createUuid();
        m_timelineState.nextUrl = m_timelineUrl + "?per_page=100";

        m_reviewState = CollectionState();
        m_reviewState.generation = QUuid::createUuid();
        m_reviewState.nextUrl = m_reviewCommentsUrl + "?per_page=100";

        m_commitsState = CollectionState();
        m_commitsState.generation = QUuid::createUuid();
        m_commitsState.nextUrl = m_commitsUrl + "?per_page=100";

        m_filesState = CollectionState();
        m_filesState.generation = QUuid::createUuid();
        m_filesState.nextUrl = m_notification.url + "/files?per_page=100";

        m_events.clear();

        while (QLayoutItem* item = m_commentsContainerLayout->takeAt(0)) {
            delete item->widget();
            delete item;
        }

        m_commentsContainerLayout->addStretch();

        QString author = obj["user"].toObject()["login"].toString();
        QString body = obj["body"].toString();
        QString createdAt = obj["created_at"].toString();
        QDateTime createdDt = QDateTime::fromString(createdAt, Qt::ISODate);

        PREvent ev;
        ev.id = QString::number(obj["id"].toVariant().toLongLong());
        ev.type = PREvent::Body;
        ev.timestamp = createdDt;
        ev.author = author;
        ev.body = body;
        m_events.append(ev);

        // Update RHS conversation metadata
        m_openedByLabel->setText(tr("<b>Opened by:</b> %1").arg(author));

        QString createdStr =
            createdDt.isValid() ? QLocale().toString(createdDt.toLocalTime(), QLocale::ShortFormat) : tr("N/A");
        m_createdAtLabel->setText(tr("<b>Created:</b> %1").arg(createdStr));

        QString updatedAt = obj["updated_at"].toString();
        QDateTime updatedDt = QDateTime::fromString(updatedAt, Qt::ISODate);
        QString updatedStr =
            updatedDt.isValid() ? QLocale().toString(updatedDt.toLocalTime(), QLocale::ShortFormat) : tr("N/A");
        m_updatedAtLabel->setText(tr("<b>Updated:</b> %1").arg(updatedStr));

        // Update Metadata
        QStringList labels;
        QJsonArray labelsArray = obj["labels"].toArray();
        for (const QJsonValue& val : labelsArray) {
            labels << val.toObject()["name"].toString();
        }
        QString labelsText = labels.isEmpty() ? tr("None") : labels.join(QStringLiteral(", "));
        m_labelsLabel->setText(tr("<b>Labels:</b> %1").arg(labelsText));
        m_convLabelsLabel->setText(tr("<b>Labels:</b> %1").arg(labelsText));

        QStringList assignees;
        QJsonArray assigneesArray = obj["assignees"].toArray();
        for (const QJsonValue& val : assigneesArray) {
            assignees << val.toObject()["login"].toString();
        }
        QString assigneesText = assignees.isEmpty() ? tr("None") : assignees.join(QStringLiteral(", "));
        m_assigneesLabel->setText(tr("<b>Assignees:</b> %1").arg(assigneesText));
        m_convAssigneesLabel->setText(tr("<b>Assignees:</b> %1").arg(assigneesText));

        QJsonObject milestoneObj = obj["milestone"].toObject();
        QString milestoneText = tr("None");
        if (!milestoneObj.isEmpty()) {
            milestoneText = milestoneObj["title"].toString();
        }
        m_milestoneLabel->setText(tr("<b>Milestone:</b> %1").arg(milestoneText));
        m_convMilestoneLabel->setText(tr("<b>Milestone:</b> %1").arg(milestoneText));

        fetchTimeline();
        fetchReviewComments();
        fetchCommits();
        fetchFiles();
    } else {
        m_requestStatus->setText(tr("Failed to fetch PR details: %1. Retry to try again.").arg(reply->errorString()));
    }
    reply->deleteLater();
}

QString PullRequestWindow::parseNextLink(QNetworkReply* reply) {
    if (reply->hasRawHeader("Link")) {
        QString linkHeader = reply->rawHeader("Link");
        QRegularExpression re("<([^>]+)>;\\s*rel=\"next\"");
        QRegularExpressionMatch match = re.match(linkHeader);
        if (match.hasMatch()) {
            return match.captured(1);
        }
    }
    return QString();
}

void PullRequestWindow::updateCollectionStatusUi() {
    if (m_timelineState.isFailed || m_reviewState.isFailed) {
        m_conversationStatusLabel->setText(tr("Error loading conversation."));
        m_conversationStatusLabel->show();
        m_conversationRetryBtn->show();
    } else if (m_timelineState.isLoading || m_reviewState.isLoading) {
        m_conversationStatusLabel->setText(tr("Loading conversation..."));
        m_conversationStatusLabel->show();
        m_conversationRetryBtn->hide();
    } else {
        m_conversationStatusLabel->hide();
        m_conversationRetryBtn->hide();
    }

    if (m_commitsState.isFailed) {
        m_commitsStatusLabel->setText(tr("Error loading commits: %1").arg(m_commitsState.errorString));
        m_commitsStatusLabel->show();
        m_commitsRetryBtn->show();
    } else if (m_commitsState.isLoading) {
        m_commitsStatusLabel->setText(tr("Loading commits..."));
        m_commitsStatusLabel->show();
        m_commitsRetryBtn->hide();
    } else {
        if (m_commitsTable->rowCount() == 0) {
            m_commitsStatusLabel->setText(tr("No commits."));
            m_commitsStatusLabel->show();
        } else {
            m_commitsStatusLabel->hide();
        }
        m_commitsRetryBtn->hide();
    }

    if (m_filesState.isFailed) {
        m_filesStatusLabel->setText(tr("Error loading changed files: %1").arg(m_filesState.errorString));
        m_filesStatusLabel->show();
        m_filesRetryBtn->show();
    } else if (m_filesState.isLoading) {
        m_filesStatusLabel->setText(tr("Loading changed files..."));
        m_filesStatusLabel->show();
        m_filesRetryBtn->hide();
    } else {
        if (m_filesTable->rowCount() == 0) {
            m_filesStatusLabel->setText(tr("No changed files."));
            m_filesStatusLabel->show();
        } else {
            m_filesStatusLabel->hide();
        }
        m_filesRetryBtn->hide();
    }
}

void PullRequestWindow::updateConversationUi() {
    if ((!m_timelineState.isComplete && !m_timelineState.isFailed) ||
        (!m_reviewState.isComplete && !m_reviewState.isFailed)) {
        return;
    }

    while (QLayoutItem* item = m_commentsContainerLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }

    m_commentsContainerLayout->addStretch();

    std::sort(m_events.begin(), m_events.end());
    auto last = std::unique(m_events.begin(), m_events.end());
    m_events.erase(last, m_events.end());

    for (const PREvent& ev : m_events) {
        QString formattedDate =
            ev.timestamp.isValid() ? QLocale().toString(ev.timestamp.toLocalTime(), QLocale::ShortFormat) : "";
        if (ev.type == PREvent::TimelineEvent) {
            QString text = ev.actionText;
            if (!formattedDate.isEmpty()) {
                text += tr(" on %1").arg(formattedDate);
            }
            QLabel* label = new QLabel(text);
            label->setTextFormat(Qt::RichText);
            label->setWordWrap(true);
            label->setStyleSheet("color: gray;");
            m_commentsContainerLayout->insertWidget(m_commentsContainerLayout->count() - 1, label);
        } else if (ev.type == PREvent::ReviewComment) {
            QString fullBody =
                tr("**Review comment on %1:**\n\n```diff\n%2\n```\n\n%3").arg(ev.path, ev.diffHunk, ev.body);
            addCommentToUI(ev.author, fullBody, ev.timestamp.toString(Qt::ISODate));
        } else {
            addCommentToUI(ev.author, ev.body, ev.timestamp.toString(Qt::ISODate));
        }
    }
}

void PullRequestWindow::fetchTimeline(const QString& urlStr) {
    QString targetUrl = urlStr.isEmpty() ? m_timelineState.nextUrl : urlStr;
    if (targetUrl.isEmpty()) return;
    m_timelineState.isLoading = true;
    updateCollectionStatusUi();
    m_timelineState.isFailed = false;
    m_timelineState.errorString.clear();
    QUrl url(targetUrl);
    QNetworkRequest request = m_client->createAuthenticatedRequest(url);
    QNetworkReply* reply = m_manager->get(request);
    reply->setProperty("generation", m_detailsGeneration);
    reply->setProperty("collectionGeneration", m_timelineState.generation);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { onTimelineReply(reply); });
}

void PullRequestWindow::onTimelineReply(QNetworkReply* reply) {
    if (reply->property("generation").toUuid() != m_detailsGeneration ||
        reply->property("collectionGeneration").toUuid() != m_timelineState.generation) {
        reply->deleteLater();
        return;
    }
    m_timelineState.isLoading = false;
    updateCollectionStatusUi();
    if (reply->error() == QNetworkReply::NoError) {
        m_timelineState.nextUrl = parseNextLink(reply);
        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonArray array = doc.array();

        for (const QJsonValue& val : array) {
            QJsonObject obj = val.toObject();
            QString event = obj["event"].toString();

            if (event == "commented") {
                QString author = obj["user"].toObject()["login"].toString();
                QString body = obj["body"].toString();
                QString createdAt = obj["created_at"].toString();

                PREvent ev;
                ev.id = QString::number(obj["id"].toVariant().toLongLong());
                ev.type = PREvent::IssueComment;
                ev.timestamp = QDateTime::fromString(createdAt, Qt::ISODate);
                ev.author = author;
                ev.body = body;
                m_events.append(ev);
            } else {
                QString createdAt = obj["created_at"].toString();
                QString text;

                if (event == "committed") {
                    QString sha = obj["sha"].toString().left(7);
                    QString author = obj["author"].toObject()["name"].toString();
                    text = tr("<b>%1</b> added commit <code>%2</code>").arg(author, sha);
                    createdAt = obj["author"].toObject()["date"].toString();
                } else if (event == "merged") {
                    QString actor = obj["actor"].toObject()["login"].toString();
                    QString commitId = obj["commit_id"].toString().left(7);
                    text = tr("<b>%1</b> merged commit <code>%2</code>").arg(actor, commitId);
                } else if (event == "closed") {
                    QString actor = obj["actor"].toObject()["login"].toString();
                    text = tr("<b>%1</b> closed this").arg(actor);
                } else if (event == "reopened") {
                    QString actor = obj["actor"].toObject()["login"].toString();
                    text = tr("<b>%1</b> reopened this").arg(actor);
                } else if (event == "labeled") {
                    QString actor = obj["actor"].toObject()["login"].toString();
                    QString labelName = obj["label"].toObject()["name"].toString();
                    text = tr("<b>%1</b> added label <b>%2</b>").arg(actor, labelName);
                } else if (event == "unlabeled") {
                    QString actor = obj["actor"].toObject()["login"].toString();
                    QString labelName = obj["label"].toObject()["name"].toString();
                    text = tr("<b>%1</b> removed label <b>%2</b>").arg(actor, labelName);
                } else if (event == "assigned") {
                    QString actor = obj["actor"].toObject()["login"].toString();
                    QString assignee = obj["assignee"].toObject()["login"].toString();
                    text = tr("<b>%1</b> assigned <b>%2</b>").arg(actor, assignee);
                } else if (event == "unassigned") {
                    QString actor = obj["actor"].toObject()["login"].toString();
                    QString assignee = obj["assignee"].toObject()["login"].toString();
                    text = tr("<b>%1</b> unassigned <b>%2</b>").arg(actor, assignee);
                }

                if (!text.isEmpty()) {
                    PREvent ev;
                    ev.id = QString::number(obj["id"].toVariant().toLongLong());
                    ev.type = PREvent::TimelineEvent;
                    ev.timestamp = QDateTime::fromString(createdAt, Qt::ISODate);
                    ev.actionText = text;
                    m_events.append(ev);
                }
            }
        }

        if (!m_timelineState.nextUrl.isEmpty()) {
            fetchTimeline();
        } else {
            m_timelineState.isComplete = true;
        }
        updateConversationUi();
        updateCollectionStatusUi();
    } else {
        m_timelineState.isFailed = true;
        m_timelineState.errorString = reply->errorString();
        updateCollectionStatusUi();
    }
    reply->deleteLater();
}

void PullRequestWindow::fetchReviewComments(const QString& urlStr) {
    QString targetUrl = urlStr.isEmpty() ? m_reviewState.nextUrl : urlStr;
    if (targetUrl.isEmpty()) return;
    m_reviewState.isLoading = true;
    updateCollectionStatusUi();
    m_reviewState.isFailed = false;
    m_reviewState.errorString.clear();
    QUrl url(targetUrl);
    QNetworkRequest request = m_client->createAuthenticatedRequest(url);
    QNetworkReply* reply = m_manager->get(request);
    reply->setProperty("generation", m_detailsGeneration);
    reply->setProperty("collectionGeneration", m_reviewState.generation);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { onReviewCommentsReply(reply); });
}

void PullRequestWindow::onReviewCommentsReply(QNetworkReply* reply) {
    if (reply->property("generation").toUuid() != m_detailsGeneration ||
        reply->property("collectionGeneration").toUuid() != m_reviewState.generation) {
        reply->deleteLater();
        return;
    }
    m_reviewState.isLoading = false;
    updateCollectionStatusUi();
    if (reply->error() == QNetworkReply::NoError) {
        m_reviewState.nextUrl = parseNextLink(reply);
        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonArray array = doc.array();

        for (const QJsonValue& val : array) {
            QJsonObject obj = val.toObject();
            PREvent ev;
            ev.id = QString::number(obj["id"].toVariant().toLongLong());
            ev.type = PREvent::ReviewComment;
            ev.timestamp = QDateTime::fromString(obj["created_at"].toString(), Qt::ISODate);
            ev.author = obj["user"].toObject()["login"].toString();
            ev.body = obj["body"].toString();
            ev.diffHunk = obj["diff_hunk"].toString();
            ev.path = obj["path"].toString();
            m_events.append(ev);
        }
        if (!m_reviewState.nextUrl.isEmpty()) {
            fetchReviewComments();
        } else {
            m_reviewState.isComplete = true;
        }
        updateConversationUi();
        updateCollectionStatusUi();
    } else {
        m_reviewState.isFailed = true;
        m_reviewState.errorString = reply->errorString();
        updateCollectionStatusUi();
    }
    reply->deleteLater();
}

void PullRequestWindow::fetchCommits(const QString& urlStr) {
    QString targetUrl = urlStr.isEmpty() ? m_commitsState.nextUrl : urlStr;
    if (targetUrl.isEmpty()) return;
    m_commitsState.isLoading = true;
    updateCollectionStatusUi();
    m_commitsState.isFailed = false;
    m_commitsState.errorString.clear();
    QUrl url(targetUrl);
    QNetworkRequest request = m_client->createAuthenticatedRequest(url);
    QNetworkReply* reply = m_manager->get(request);
    reply->setProperty("generation", m_detailsGeneration);
    reply->setProperty("collectionGeneration", m_commitsState.generation);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { onCommitsReply(reply); });
}

void PullRequestWindow::onCommitsReply(QNetworkReply* reply) {
    if (reply->property("generation").toUuid() != m_detailsGeneration ||
        reply->property("collectionGeneration").toUuid() != m_commitsState.generation) {
        reply->deleteLater();
        return;
    }
    m_commitsState.isLoading = false;
    updateCollectionStatusUi();
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonArray array = doc.array();

        for (int i = 0; i < array.size(); ++i) {
            QJsonObject obj = array[i].toObject();
            QString sha = obj["sha"].toString().left(7);
            QJsonObject commitObj = obj["commit"].toObject();
            QString message = commitObj["message"].toString().section('\n', 0, 0);
            QString author = commitObj["author"].toObject()["name"].toString();
            QString date = commitObj["author"].toObject()["date"].toString();

            int row = m_commitsTable->rowCount();
            m_commitsTable->insertRow(row);
            m_commitsTable->setItem(row, 0, new QTableWidgetItem(sha));
            m_commitsTable->setItem(row, 1, new QTableWidgetItem(message));
            m_commitsTable->setItem(row, 2, new QTableWidgetItem(author));
            m_commitsTable->setItem(row, 3,
                                    new QTableWidgetItem(QLocale().toString(QDateTime::fromString(date, Qt::ISODate),
                                                                            QLocale::ShortFormat)));
        }

        m_commitsState.nextUrl.clear();
        if (reply->hasRawHeader("Link")) {
            m_commitsState.nextUrl = parseNextLink(reply);
            if (!m_commitsState.nextUrl.isEmpty()) {
                fetchCommits();
            }
        }

        if (m_commitsState.nextUrl.isEmpty()) {
            m_commitsState.isComplete = true;
        }
        updateCollectionStatusUi();
    } else {
        m_commitsState.isFailed = true;
        m_commitsState.errorString = reply->errorString();
        updateCollectionStatusUi();
    }
    reply->deleteLater();
}

void PullRequestWindow::fetchFiles(const QString& urlStr) {
    QString targetUrl = urlStr.isEmpty() ? m_filesState.nextUrl : urlStr;
    if (targetUrl.isEmpty()) return;
    m_filesState.isLoading = true;
    updateCollectionStatusUi();
    m_filesState.isFailed = false;
    m_filesState.errorString.clear();
    QUrl url(targetUrl);
    QNetworkRequest request = m_client->createAuthenticatedRequest(url);
    QNetworkReply* reply = m_manager->get(request);
    reply->setProperty("generation", m_detailsGeneration);
    reply->setProperty("collectionGeneration", m_filesState.generation);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { onFilesReply(reply); });
}

void PullRequestWindow::onFilesReply(QNetworkReply* reply) {
    if (reply->property("generation").toUuid() != m_detailsGeneration ||
        reply->property("collectionGeneration").toUuid() != m_filesState.generation) {
        reply->deleteLater();
        return;
    }
    m_filesState.isLoading = false;
    updateCollectionStatusUi();
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonArray array = doc.array();

        for (int i = 0; i < array.size(); ++i) {
            QJsonObject obj = array[i].toObject();
            QString filename = obj["filename"].toString();
            QString status = obj["status"].toString();
            int additions = obj["additions"].toInt();
            int deletions = obj["deletions"].toInt();

            int row = m_filesTable->rowCount();
            m_filesTable->insertRow(row);
            m_filesTable->setItem(row, 0, new QTableWidgetItem(filename));
            m_filesTable->setItem(row, 1, new QTableWidgetItem(status));
            m_filesTable->setItem(row, 2, new QTableWidgetItem(QString::number(additions)));
            m_filesTable->setItem(row, 3, new QTableWidgetItem(QString::number(deletions)));
        }

        m_filesState.nextUrl.clear();
        if (reply->hasRawHeader("Link")) {
            m_filesState.nextUrl = parseNextLink(reply);
            if (!m_filesState.nextUrl.isEmpty()) {
                fetchFiles();
            }
        }

        if (m_filesState.nextUrl.isEmpty()) {
            m_filesState.isComplete = true;
        }
        updateCollectionStatusUi();
    } else {
        m_filesState.isFailed = true;
        m_filesState.errorString = reply->errorString();
        updateCollectionStatusUi();
    }
    reply->deleteLater();
}

void PullRequestWindow::onFileDoubleClicked(int row, int column) {
    Q_UNUSED(column);
    QTableWidgetItem* item = m_filesTable->item(row, 0);
    if (!item) return;

    QString blobUrl = item->data(Qt::UserRole).toString();
    if (!blobUrl.isEmpty()) {
        QUrl url(blobUrl);
        if (UrlHelper::isSafeWebUrl(url)) {
            if (!UrlHelper::openUrl(url)) {
                QMessageBox::warning(this, tr("Error"), tr("Failed to open the URL in your web browser."));
            }
        } else {
            QMessageBox::warning(this, tr("Security Warning"), tr("Blocked attempt to open an unsafe or invalid URL."));
        }
    }
}

void PullRequestWindow::addCommentToUI(const QString& author, const QString& body, const QString& createdAt) {
    QDateTime dt = QDateTime::fromString(createdAt, Qt::ISODate);
    QString formattedDate = dt.isValid() ? QLocale().toString(dt.toLocalTime(), QLocale::ShortFormat) : createdAt;

    CommentWidget* widget = new CommentWidget(author, body, formattedDate);
    m_commentsContainerLayout->addWidget(widget);
}

void PullRequestWindow::onCommentButtonClicked() {
    QString commentText = m_replyEdit->toPlainText().trimmed();
    if (commentText.isEmpty() || m_issueCommentsUrl.isEmpty()) return;

    m_commentButton->setEnabled(false);

    QJsonObject json;
    json["body"] = commentText;
    QJsonDocument doc(json);
    QByteArray data = doc.toJson();

    QUrl url(m_issueCommentsUrl);
    QNetworkRequest request = m_client->createAuthenticatedRequest(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    m_commentRequestId = QUuid::createUuid();
    QNetworkReply* reply = m_manager->post(request, data);
    reply->setProperty("reqId", m_commentRequestId);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { onPostCommentReply(reply); });
}

void PullRequestWindow::setupMenus() {
    KStandardAction::close(this, &PullRequestWindow::close, actionCollection());
    QAction* viewRawJsonAction = new QAction(QIcon::fromTheme("text-x-generic"), tr("View Raw JSON"), this);
    viewRawJsonAction->setEnabled(false);
    connect(viewRawJsonAction, &QAction::triggered, this, &PullRequestWindow::onViewRawJson);
    actionCollection()->addAction(QStringLiteral("view_raw_json"), viewRawJsonAction);

    // Open in browser mapping for the tools menu since it shares rc file
    QAction* openUrlAction = new QAction(QIcon::fromTheme("internet-web-browser"), tr("Open PR in Browser"), this);
    connect(openUrlAction, &QAction::triggered, this, [this]() {
        const QUrl url(GitHubClient::apiToHtmlUrl(m_notification.url, m_notification.id));
        if (UrlHelper::isSafeWebUrl(url)) {
            if (!UrlHelper::openUrl(url)) {
                QMessageBox::warning(this, tr("Error"), tr("Failed to open the URL in your web browser."));
            }
        } else {
            QMessageBox::warning(this, tr("Security Warning"), tr("Blocked attempt to open an unsafe or invalid URL."));
        }
    });
    actionCollection()->addAction(QStringLiteral("open_browser"), openUrlAction);
}

void PullRequestWindow::onViewRawJson() {
    QDialog* dialog = new QDialog(this);
    dialog->setWindowTitle(tr("Raw JSON"));
    dialog->resize(600, 400);

    QVBoxLayout* layout = new QVBoxLayout(dialog);
    QTextEdit* textEdit = new QTextEdit(dialog);
    textEdit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    textEdit->setReadOnly(true);
    textEdit->setPlainText(m_rawJsonStr);
    layout->addWidget(textEdit);

    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void PullRequestWindow::onPostCommentReply(QNetworkReply* reply) {
    if (reply->property("reqId").toUuid() != m_commentRequestId) {
        reply->deleteLater();
        return;
    }
    m_commentRequestId = QUuid();
    m_commentButton->setEnabled(true);
    if (reply->error() == QNetworkReply::NoError) {
        m_replyEdit->clear();

        // Add the new comment to the UI instantly
        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonObject obj = doc.object();
        QString author = obj["user"].toObject()["login"].toString();
        QString body = obj["body"].toString();
        QString createdAt = obj["created_at"].toString();
        addCommentToUI(author, body, createdAt);
    } else {
        QMessageBox::warning(this, tr("Error"), tr("Failed to post comment: %1").arg(reply->errorString()));
    }
    reply->deleteLater();
}
