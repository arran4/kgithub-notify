#include "PullRequestWindow.h"

#include <KActionCollection>
#include <KStandardAction>
#include <QDateTime>
#include <QFrame>
#include <QDesktopServices>
#include <QFontDatabase>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QUrl>
#include <QTextEdit>

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

PullRequestWindow::PullRequestWindow(const Notification& n, GitHubClient* client, QWidget* parent)
    : KXmlGuiWindow(parent, Qt::Window),
      m_notification(n),
      m_client(client),
      m_manager(new QNetworkAccessManager(this)) {
    setWindowTitle(tr("Pull Request - %1").arg(n.title));
    resize(800, 600);

    setupUi();

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
    m_conversationLayout = new QVBoxLayout(m_conversationTab);

    m_commentsScrollArea = new QScrollArea();
    m_commentsScrollArea->setWidgetResizable(true);
    m_commentsContainer = new QWidget();
    m_commentsContainerLayout = new QVBoxLayout(m_commentsContainer);
    m_commentsContainerLayout->setAlignment(Qt::AlignTop);
    m_commentsContainer->setLayout(m_commentsContainerLayout);
    m_commentsScrollArea->setWidget(m_commentsContainer);

    m_conversationLayout->addWidget(m_commentsScrollArea);

    m_replyEdit = new QTextEdit();
    m_replyEdit->setPlaceholderText(tr("Leave a comment..."));
    m_replyEdit->setMaximumHeight(100);
    m_conversationLayout->addWidget(m_replyEdit);

    m_commentButton = new QPushButton(tr("Comment"));
    connect(m_commentButton, &QPushButton::clicked, this, &PullRequestWindow::onCommentButtonClicked);
    m_conversationLayout->addWidget(m_commentButton, 0, Qt::AlignRight);

    m_tabWidget->addTab(m_conversationTab, tr("Conversation"));

    // 2. Commits Tab
    m_commitsTab = new QWidget();
    m_commitsLayout = new QVBoxLayout(m_commitsTab);
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
    QUrl url(m_notification.url);
    QNetworkRequest request = m_client->createAuthenticatedRequest(url);
    QNetworkReply* reply = m_manager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { onPrDetailsReply(reply); });
}

void PullRequestWindow::onPrDetailsReply(QNetworkReply* reply) {
    if (reply->error() == QNetworkReply::NoError) {
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

        // Add the PR body as the first comment
        QString author = obj["user"].toObject()["login"].toString();
        QString body = obj["body"].toString();
        QString createdAt = obj["created_at"].toString();
        addCommentToUI(author, body, createdAt);

        // Update Metadata
        QStringList labels;
        QJsonArray labelsArray = obj["labels"].toArray();
        for (const QJsonValue& val : labelsArray) {
            labels << val.toObject()["name"].toString();
        }
        m_labelsLabel->setText(tr("<b>Labels:</b> %1").arg(labels.isEmpty() ? "None" : labels.join(", ")));

        QStringList assignees;
        QJsonArray assigneesArray = obj["assignees"].toArray();
        for (const QJsonValue& val : assigneesArray) {
            assignees << val.toObject()["login"].toString();
        }
        m_assigneesLabel->setText(tr("<b>Assignees:</b> %1").arg(assignees.isEmpty() ? "None" : assignees.join(", ")));

        QJsonObject milestoneObj = obj["milestone"].toObject();
        if (!milestoneObj.isEmpty()) {
            m_milestoneLabel->setText(tr("<b>Milestone:</b> %1").arg(milestoneObj["title"].toString()));
        } else {
            m_milestoneLabel->setText(tr("<b>Milestone:</b> None"));
        }

        fetchTimeline();
        fetchReviewComments();
        fetchCommits();
        fetchFiles();
    } else {
        QMessageBox::warning(this, tr("Error"), tr("Failed to fetch PR details: %1").arg(reply->errorString()));
    }
    reply->deleteLater();
}

void PullRequestWindow::fetchTimeline() {
    if (m_timelineUrl.isEmpty()) return;
    QUrl url(m_timelineUrl);
    QNetworkRequest request = m_client->createAuthenticatedRequest(url);
    QNetworkReply* reply = m_manager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { onTimelineReply(reply); });
}

void PullRequestWindow::onTimelineReply(QNetworkReply* reply) {
    if (reply->error() == QNetworkReply::NoError) {
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
                addCommentToUI(author, body, createdAt);
            } else {
                QString createdAt = obj["created_at"].toString();
                QString text;

                if (event == "committed") {
                    QString sha = obj["sha"].toString().left(7);
                    QString message = obj["message"].toString().section('\n', 0, 0);
                    QString author = obj["author"].toObject()["name"].toString();
                    text = tr("<i>%1 committed %2: %3</i>").arg(author, sha, message);
                } else if (event == "assigned") {
                    QString actor = obj["actor"].toObject()["login"].toString();
                    QString assignee = obj["assignee"].toObject()["login"].toString();
                    text = tr("<i>%1 assigned %2</i>").arg(actor, assignee);
                } else if (event == "unassigned") {
                    QString actor = obj["actor"].toObject()["login"].toString();
                    QString assignee = obj["assignee"].toObject()["login"].toString();
                    text = tr("<i>%1 unassigned %2</i>").arg(actor, assignee);
                } else if (event == "labeled") {
                    QString actor = obj["actor"].toObject()["login"].toString();
                    QString label = obj["label"].toObject()["name"].toString();
                    text = tr("<i>%1 added the %2 label</i>").arg(actor, label);
                } else if (event == "unlabeled") {
                    QString actor = obj["actor"].toObject()["login"].toString();
                    QString label = obj["label"].toObject()["name"].toString();
                    text = tr("<i>%1 removed the %2 label</i>").arg(actor, label);
                } else if (event == "closed") {
                    QString actor = obj["actor"].toObject()["login"].toString();
                    text = tr("<i>%1 closed this</i>").arg(actor);
                } else if (event == "reopened") {
                    QString actor = obj["actor"].toObject()["login"].toString();
                    text = tr("<i>%1 reopened this</i>").arg(actor);
                } else if (event == "merged") {
                    QString actor = obj["actor"].toObject()["login"].toString();
                    QString commitId = obj["commit_id"].toString().left(7);
                    text = tr("<i>%1 merged commit %2</i>").arg(actor, commitId);
                } else if (event == "review_requested") {
                    QString actor = obj["actor"].toObject()["login"].toString();
                    QString requested = obj["requested_reviewer"].toObject()["login"].toString();
                    if (requested.isEmpty()) {
                        requested = obj["requested_team"].toObject()["name"].toString();
                    }
                    text = tr("<i>%1 requested a review from %2</i>").arg(actor, requested);
                } else if (event == "review_request_removed") {
                    QString actor = obj["actor"].toObject()["login"].toString();
                    QString requested = obj["requested_reviewer"].toObject()["login"].toString();
                    if (requested.isEmpty()) {
                        requested = obj["requested_team"].toObject()["name"].toString();
                    }
                    text = tr("<i>%1 removed the request for review from %2</i>").arg(actor, requested);
                } else if (event == "reviewed") {
                    QString actor = obj["user"].toObject()["login"].toString();
                    QString state = obj["state"].toString();
                    text = tr("<i>%1 reviewed this (%2)</i>").arg(actor, state);
                } else if (event == "head_ref_force_pushed") {
                    QString actor = obj["actor"].toObject()["login"].toString();
                    text = tr("<i>%1 force-pushed the branch</i>").arg(actor);
                } else {
                    QString actor = obj["actor"].toObject()["login"].toString();
                    if (actor.isEmpty()) {
                        actor = obj["user"].toObject()["login"].toString();
                    }
                    if (actor.isEmpty()) {
                        text = tr("<i>Event: %1</i>").arg(event);
                    } else {
                        text = tr("<i>%1: %2</i>").arg(actor, event);
                    }
                }

                if (!text.isEmpty()) {
                    if (!createdAt.isEmpty()) {
                        QDateTime dt = QDateTime::fromString(createdAt, Qt::ISODate);
                        QString formattedDate = QLocale().toString(dt, QLocale::ShortFormat);
                        text += tr(" on %1").arg(formattedDate);
                    }
                    QLabel* label = new QLabel(text);
                    label->setTextFormat(Qt::RichText);
                    label->setWordWrap(true);
                    label->setStyleSheet("color: gray;");
                    m_commentsContainerLayout->addWidget(label);
                }
            }
        }
    } else {
        qWarning() << "Failed to fetch timeline:" << reply->errorString();
    }
    reply->deleteLater();
}

void PullRequestWindow::fetchReviewComments() {
    if (m_reviewCommentsUrl.isEmpty()) return;
    QUrl url(m_reviewCommentsUrl);
    QNetworkRequest request = m_client->createAuthenticatedRequest(url);
    QNetworkReply* reply = m_manager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { onReviewCommentsReply(reply); });
}

void PullRequestWindow::onReviewCommentsReply(QNetworkReply* reply) {
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonArray array = doc.array();

        for (const QJsonValue& val : array) {
            QJsonObject obj = val.toObject();
            QString author = obj["user"].toObject()["login"].toString();
            QString body = obj["body"].toString();
            QString createdAt = obj["created_at"].toString();
            QString path = obj["path"].toString();
            QString diffHunk = obj["diff_hunk"].toString();

            QString fullBody = tr("**Review comment on %1:**\n\n```diff\n%2\n```\n\n%3").arg(path, diffHunk, body);
            addCommentToUI(author, fullBody, createdAt);
        }
    }
    reply->deleteLater();
}

void PullRequestWindow::fetchCommits() {
    if (m_commitsUrl.isEmpty()) return;
    QUrl url(m_commitsUrl);
    QNetworkRequest request = m_client->createAuthenticatedRequest(url);
    QNetworkReply* reply = m_manager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { onCommitsReply(reply); });
}

void PullRequestWindow::onCommitsReply(QNetworkReply* reply) {
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonArray array = doc.array();

        m_commitsTable->setRowCount(0);
        for (int i = 0; i < array.size(); ++i) {
            QJsonObject obj = array[i].toObject();
            QString sha = obj["sha"].toString().left(7);
            QJsonObject commitObj = obj["commit"].toObject();
            QString message = commitObj["message"].toString().section('\n', 0, 0);  // First line only
            QString author = commitObj["author"].toObject()["name"].toString();
            QString date = commitObj["author"].toObject()["date"].toString();

            m_commitsTable->insertRow(i);
            m_commitsTable->setItem(i, 0, new QTableWidgetItem(sha));
            m_commitsTable->setItem(i, 1, new QTableWidgetItem(author));
            m_commitsTable->setItem(i, 2, new QTableWidgetItem(message));
            m_commitsTable->setItem(i, 3, new QTableWidgetItem(date));
        }
    }
    reply->deleteLater();
}

void PullRequestWindow::fetchFiles() {
    QUrl url(m_notification.url + "/files");
    QNetworkRequest request = m_client->createAuthenticatedRequest(url);
    QNetworkReply* reply = m_manager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { onFilesReply(reply); });
}

void PullRequestWindow::onFilesReply(QNetworkReply* reply) {
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonArray array = doc.array();

        m_filesTable->setRowCount(0);
        for (int i = 0; i < array.size(); ++i) {
            QJsonObject obj = array[i].toObject();
            QString filename = obj["filename"].toString();
            int additions = obj["additions"].toInt();
            int deletions = obj["deletions"].toInt();
            int changes = obj["changes"].toInt();
            QString blobUrl = obj["blob_url"].toString();

            m_filesTable->insertRow(i);
            QTableWidgetItem* fileItem = new QTableWidgetItem(filename);
            fileItem->setData(Qt::UserRole, blobUrl);
            m_filesTable->setItem(i, 0, fileItem);

            QTableWidgetItem* addItem = new QTableWidgetItem(QString::number(additions));
            addItem->setForeground(QBrush(Qt::darkGreen));
            m_filesTable->setItem(i, 1, addItem);

            QTableWidgetItem* delItem = new QTableWidgetItem(QString::number(deletions));
            delItem->setForeground(QBrush(Qt::darkRed));
            m_filesTable->setItem(i, 2, delItem);

            m_filesTable->setItem(i, 3, new QTableWidgetItem(QString::number(changes)));
        }
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
        if (url.isValid() && (url.scheme() == "http" || url.scheme() == "https")) {
            if (!QDesktopServices::openUrl(url)) {
                QMessageBox::warning(this, tr("Error"), tr("Failed to open the URL in your web browser."));
            }
        } else {
            QMessageBox::warning(this, tr("Security Warning"), tr("Blocked attempt to open an unsafe or invalid URL."));
        }
    }
}

void PullRequestWindow::addCommentToUI(const QString& author, const QString& body, const QString& createdAt) {
    QDateTime dt = QDateTime::fromString(createdAt, Qt::ISODate);
    QString formattedDate = QLocale().toString(dt, QLocale::ShortFormat);

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

    QNetworkReply* reply = m_manager->post(request, data);
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
    connect(openUrlAction, &QAction::triggered, this,
            [this]() { QDesktopServices::openUrl(QUrl(GitHubClient::apiToHtmlUrl(m_notification.url, m_notification.id))); });
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
