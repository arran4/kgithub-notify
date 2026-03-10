#include "PullRequestWindow.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QHeaderView>
#include <QMessageBox>
#include <QDateTime>

PullRequestWindow::PullRequestWindow(const Notification &n, GitHubClient *client, QWidget *parent)
    : KXmlGuiWindow(parent, Qt::Window), m_notification(n), m_client(client), m_manager(new QNetworkAccessManager(this))
{
    setWindowTitle(tr("Pull Request - %1").arg(n.title));
    resize(800, 600);

    setupUi();

    fetchPrDetails();
}

void PullRequestWindow::setupUi()
{
    m_tabWidget = new QTabWidget(this);
    setupGUI(Default, "kgithub-notifyui.rc");
    setCentralWidget(m_tabWidget);

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
    m_filesTable->setColumnCount(4);
    m_filesTable->setHorizontalHeaderLabels({tr("Filename"), tr("Additions"), tr("Deletions"), tr("Changes")});
    m_filesTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_filesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_filesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_filesLayout->addWidget(m_filesTable);
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

void PullRequestWindow::fetchPrDetails()
{
    QUrl url(m_notification.url);
    QNetworkRequest request = m_client->createAuthenticatedRequest(url);
    QNetworkReply *reply = m_manager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { onPrDetailsReply(reply); });
}

void PullRequestWindow::onPrDetailsReply(QNetworkReply *reply)
{
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonObject obj = doc.object();

        m_commentsUrl = obj["comments_url"].toString();
        m_reviewCommentsUrl = obj["review_comments_url"].toString();
        m_commitsUrl = obj["commits_url"].toString();

        // Convert issues url to get issue comments url for PR
        m_issueCommentsUrl = obj["issue_url"].toString() + "/comments";

        // Add the PR body as the first comment
        QString author = obj["user"].toObject()["login"].toString();
        QString body = obj["body"].toString();
        QString createdAt = obj["created_at"].toString();
        addCommentToUI(author, body, createdAt);

        // Update Metadata
        QStringList labels;
        QJsonArray labelsArray = obj["labels"].toArray();
        for (const QJsonValue &val : labelsArray) {
            labels << val.toObject()["name"].toString();
        }
        m_labelsLabel->setText(tr("<b>Labels:</b> %1").arg(labels.isEmpty() ? "None" : labels.join(", ")));

        QStringList assignees;
        QJsonArray assigneesArray = obj["assignees"].toArray();
        for (const QJsonValue &val : assigneesArray) {
            assignees << val.toObject()["login"].toString();
        }
        m_assigneesLabel->setText(tr("<b>Assignees:</b> %1").arg(assignees.isEmpty() ? "None" : assignees.join(", ")));

        QJsonObject milestoneObj = obj["milestone"].toObject();
        if (!milestoneObj.isEmpty()) {
            m_milestoneLabel->setText(tr("<b>Milestone:</b> %1").arg(milestoneObj["title"].toString()));
        } else {
            m_milestoneLabel->setText(tr("<b>Milestone:</b> None"));
        }

        fetchComments();
        fetchReviewComments();
        fetchCommits();
        fetchFiles();
    } else {
        QMessageBox::warning(this, tr("Error"), tr("Failed to fetch PR details: %1").arg(reply->errorString()));
    }
    reply->deleteLater();
}

void PullRequestWindow::fetchComments()
{
    if (m_issueCommentsUrl.isEmpty()) return;
    QUrl url(m_issueCommentsUrl);
    QNetworkRequest request = m_client->createAuthenticatedRequest(url);
    QNetworkReply *reply = m_manager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { onCommentsReply(reply); });
}

void PullRequestWindow::onCommentsReply(QNetworkReply *reply)
{
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonArray array = doc.array();

        for (const QJsonValue &val : array) {
            QJsonObject obj = val.toObject();
            QString author = obj["user"].toObject()["login"].toString();
            QString body = obj["body"].toString();
            QString createdAt = obj["created_at"].toString();
            addCommentToUI(author, body, createdAt);
        }
    }
    reply->deleteLater();
}

void PullRequestWindow::fetchReviewComments()
{
    if (m_reviewCommentsUrl.isEmpty()) return;
    QUrl url(m_reviewCommentsUrl);
    QNetworkRequest request = m_client->createAuthenticatedRequest(url);
    QNetworkReply *reply = m_manager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { onReviewCommentsReply(reply); });
}

void PullRequestWindow::onReviewCommentsReply(QNetworkReply *reply)
{
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonArray array = doc.array();

        for (const QJsonValue &val : array) {
            QJsonObject obj = val.toObject();
            QString author = obj["user"].toObject()["login"].toString();
            QString body = obj["body"].toString();
            QString createdAt = obj["created_at"].toString();
            QString path = obj["path"].toString();
            QString diffHunk = obj["diff_hunk"].toString();

            QString fullBody = tr("<b>Review comment on %1:</b><br><pre>%2</pre><br>%3").arg(path, diffHunk.toHtmlEscaped(), body);
            addCommentToUI(author, fullBody, createdAt);
        }
    }
    reply->deleteLater();
}

void PullRequestWindow::fetchCommits()
{
    if (m_commitsUrl.isEmpty()) return;
    QUrl url(m_commitsUrl);
    QNetworkRequest request = m_client->createAuthenticatedRequest(url);
    QNetworkReply *reply = m_manager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { onCommitsReply(reply); });
}

void PullRequestWindow::onCommitsReply(QNetworkReply *reply)
{
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonArray array = doc.array();

        m_commitsTable->setRowCount(0);
        for (int i = 0; i < array.size(); ++i) {
            QJsonObject obj = array[i].toObject();
            QString sha = obj["sha"].toString().left(7);
            QJsonObject commitObj = obj["commit"].toObject();
            QString message = commitObj["message"].toString().section('\n', 0, 0); // First line only
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

void PullRequestWindow::fetchFiles()
{
    QUrl url(m_notification.url + "/files");
    QNetworkRequest request = m_client->createAuthenticatedRequest(url);
    QNetworkReply *reply = m_manager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { onFilesReply(reply); });
}

void PullRequestWindow::onFilesReply(QNetworkReply *reply)
{
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

            m_filesTable->insertRow(i);
            m_filesTable->setItem(i, 0, new QTableWidgetItem(filename));

            QTableWidgetItem *addItem = new QTableWidgetItem(QString::number(additions));
            addItem->setForeground(QBrush(Qt::darkGreen));
            m_filesTable->setItem(i, 1, addItem);

            QTableWidgetItem *delItem = new QTableWidgetItem(QString::number(deletions));
            delItem->setForeground(QBrush(Qt::darkRed));
            m_filesTable->setItem(i, 2, delItem);

            m_filesTable->setItem(i, 3, new QTableWidgetItem(QString::number(changes)));
        }
    }
    reply->deleteLater();
}

void PullRequestWindow::addCommentToUI(const QString &author, const QString &body, const QString &createdAt)
{
    QDateTime dt = QDateTime::fromString(createdAt, Qt::ISODate);
    QString formattedDate = QLocale().toString(dt, QLocale::ShortFormat);

    QLabel *label = new QLabel(tr("<b>%1</b> on %2<br>%3<hr>").arg(author, formattedDate, body));
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::TextBrowserInteraction);
    label->setOpenExternalLinks(true);
    m_commentsContainerLayout->addWidget(label);
}

void PullRequestWindow::onCommentButtonClicked()
{
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

    QNetworkReply *reply = m_manager->post(request, data);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { onPostCommentReply(reply); });
}

void PullRequestWindow::onPostCommentReply(QNetworkReply *reply)
{
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
