#include "NotificationWindow.h"
#include "GitHubClient.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QDesktopServices>
#include <QApplication>
#include <QClipboard>
#include <QWidget>
#include <QJsonDocument>
#include <QDialog>
#include <QTextEdit>
#include "PullRequestWindow.h"
#include "ActionWindow.h"

NotificationWindow::NotificationWindow(const Notification &n, GitHubClient *client, QWidget *parent)
    : KXmlGuiWindow(parent, Qt::Window), m_notification(n), m_client(client) {
    setWindowTitle(tr("Notification Details - %1").arg(n.repository));
    resize(500, 400);

    // Menu Bar
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    QAction *closeAction = new QAction(QIcon::fromTheme("window-close"), tr("Close"), this);
    closeAction->setShortcut(QKeySequence::Close);
    connect(closeAction, &QAction::triggered, this, &NotificationWindow::close);
    fileMenu->addAction(closeAction);

    QMenu *editMenu = menuBar()->addMenu(tr("&Edit"));

    QAction *copyLinkAction = new QAction(QIcon::fromTheme("edit-copy"), tr("Copy Link"), this);
    copyLinkAction->setShortcut(QKeySequence::Copy);
    connect(copyLinkAction, &QAction::triggered, this, &NotificationWindow::onCopyLink);
    editMenu->addAction(copyLinkAction);

    QMenu *actionsMenu = menuBar()->addMenu(tr("&Actions"));

    QAction *openUrlAction = new QAction(QIcon::fromTheme("internet-web-browser"), tr("Open URL"), this);
    connect(openUrlAction, &QAction::triggered, this, &NotificationWindow::onOpenUrl);
    actionsMenu->addAction(openUrlAction);


    QAction *markAsReadAction = new QAction(QIcon::fromTheme("mail-mark-read"), tr("Mark as Read"), this);
    connect(markAsReadAction, &QAction::triggered, this, &NotificationWindow::onMarkAsRead);
    actionsMenu->addAction(markAsReadAction);

    QAction *markAsDoneAction = new QAction(QIcon::fromTheme("task-complete"), tr("Mark as Done"), this);
    connect(markAsDoneAction, &QAction::triggered, this, &NotificationWindow::onMarkAsDone);
    actionsMenu->addAction(markAsDoneAction);

    actionsMenu->addSeparator();

    QAction *viewRawAction = new QAction(QIcon::fromTheme("text-x-generic"), tr("View Raw JSON"), this);
    connect(viewRawAction, &QAction::triggered, this, &NotificationWindow::onViewRawJson);
    actionsMenu->addAction(viewRawAction);

    QAction *viewPullRequestAction = nullptr;
    QAction *viewActionJobAction = nullptr;

    if (m_notification.type == "PullRequest") {
        viewPullRequestAction = new QAction(QIcon::fromTheme("vcs-merge-request"), tr("View Pull Request"), this);
        connect(viewPullRequestAction, &QAction::triggered, this, &NotificationWindow::onViewPullRequest);
        actionsMenu->addAction(viewPullRequestAction);
    } else if (m_notification.type == "CheckSuite" || m_notification.type == "WorkflowRun") {
        viewActionJobAction = new QAction(QIcon::fromTheme("system-run"), tr("View Action Job"), this);
        connect(viewActionJobAction, &QAction::triggered, this, &NotificationWindow::onViewActionJob);
        actionsMenu->addAction(viewActionJobAction);
    }

    // Tool Bar
    QToolBar *toolBar = addToolBar(tr("Actions"));
    toolBar->addAction(openUrlAction);
    toolBar->addAction(copyLinkAction);
    toolBar->addSeparator();
    toolBar->addAction(markAsReadAction);
    toolBar->addAction(markAsDoneAction);
    toolBar->addSeparator();
    toolBar->addAction(viewRawAction);

    if (viewPullRequestAction) {
        toolBar->addSeparator();
        toolBar->addAction(viewPullRequestAction);
    }
    if (viewActionJobAction) {
        toolBar->addSeparator();
        toolBar->addAction(viewActionJobAction);
    }

    // Central Widget
    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(centralWidget);

    {
        QLabel *lbl = new QLabel(tr("<b>ID:</b> %1").arg(n.id.toHtmlEscaped()));
        lbl->setTextInteractionFlags(Qt::TextBrowserInteraction);
        layout->addWidget(lbl);
    }
    {
        QLabel *lbl = new QLabel(tr("<b>Title:</b> %1").arg(n.title.toHtmlEscaped()));
        lbl->setTextInteractionFlags(Qt::TextBrowserInteraction);
        layout->addWidget(lbl);
    }
    {
        QLabel *lbl = new QLabel(tr("<b>Repository:</b> %1").arg(n.repository.toHtmlEscaped()));
        lbl->setTextInteractionFlags(Qt::TextBrowserInteraction);
        layout->addWidget(lbl);
    }
    {
        QLabel *lbl = new QLabel(tr("<b>Type:</b> %1").arg(n.type.toHtmlEscaped()));
        lbl->setTextInteractionFlags(Qt::TextBrowserInteraction);
        layout->addWidget(lbl);
    }
    {
        QLabel *lbl = new QLabel(tr("<b>Updated At:</b> %1").arg(n.updatedAt.toHtmlEscaped()));
        lbl->setTextInteractionFlags(Qt::TextBrowserInteraction);
        layout->addWidget(lbl);
    }

    if (!n.lastReadAt.isEmpty()) {
        {
        QLabel *lbl = new QLabel(tr("<b>Last Read At:</b> %1").arg(n.lastReadAt.toHtmlEscaped()));
        lbl->setTextInteractionFlags(Qt::TextBrowserInteraction);
        layout->addWidget(lbl);
    }
    }

    QString apiUrl = n.url;
    QString htmlUrl = GitHubClient::apiToHtmlUrl(apiUrl, n.id);

    QLabel *urlLabel = new QLabel(tr("<b>API URL:</b> <a href=\"%1\">%1</a>").arg(apiUrl.toHtmlEscaped()));
    urlLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    connect(urlLabel, &QLabel::linkActivated, this, [this](const QString &link) {
        emit debugApiRequested(link);
    });
    layout->addWidget(urlLabel);

    if (!htmlUrl.isEmpty() && htmlUrl != apiUrl) {
        QLabel *htmlUrlLabel = new QLabel(tr("<b>HTML URL:</b> <a href=\"%1\">%1</a>").arg(htmlUrl.toHtmlEscaped()));
        htmlUrlLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
        htmlUrlLabel->setOpenExternalLinks(true);
        layout->addWidget(htmlUrlLabel);
    }

    layout->addStretch();

    // Bottom Actions
    QHBoxLayout *bottomLayout = new QHBoxLayout();
    bottomLayout->addStretch();

    QPushButton *closeBtn = new QPushButton(tr("Close"));
    connect(closeBtn, &QPushButton::clicked, this, &KXmlGuiWindow::close);
    bottomLayout->addWidget(closeBtn);

    QPushButton *markDoneBtn = new QPushButton(QIcon::fromTheme("task-complete"), tr("Mark as Done"));
    connect(markDoneBtn, &QPushButton::clicked, this, &NotificationWindow::onMarkAsDone);
    bottomLayout->addWidget(markDoneBtn);

    QPushButton *closeAndReadBtn = new QPushButton(QIcon::fromTheme("mail-mark-read"), tr("Close and mark as read"));
    connect(closeAndReadBtn, &QPushButton::clicked, this, &NotificationWindow::onCloseAndMarkAsRead);
    bottomLayout->addWidget(closeAndReadBtn);

    if (viewPullRequestAction) {
        QPushButton *viewPrBtn = new QPushButton(QIcon::fromTheme("vcs-merge-request"), tr("View Pull Request"));
        connect(viewPrBtn, &QPushButton::clicked, this, &NotificationWindow::onViewPullRequest);
        bottomLayout->addWidget(viewPrBtn);
    }
    if (viewActionJobAction) {
        QPushButton *viewActionBtn = new QPushButton(QIcon::fromTheme("system-run"), tr("View Action Job"));
        connect(viewActionBtn, &QPushButton::clicked, this, &NotificationWindow::onViewActionJob);
        bottomLayout->addWidget(viewActionBtn);
    }

    layout->addLayout(bottomLayout);

    setupGUI(Default, "kgithub-notifyui.rc");
    setCentralWidget(centralWidget);

    // Status Bar
    QString statusText = tr("Status: %1 | Inbox: %2")
        .arg(n.unread ? tr("Unread") : tr("Read"))
        .arg(n.inInbox ? tr("Yes") : tr("No"));
    statusBar()->showMessage(statusText);
}

void NotificationWindow::onOpenUrl() {
    QString url = GitHubClient::apiToHtmlUrl(m_notification.url, m_notification.id);
    QDesktopServices::openUrl(QUrl(url));
}

void NotificationWindow::onCopyLink() {
    QString url = GitHubClient::apiToHtmlUrl(m_notification.url, m_notification.id);
    QApplication::clipboard()->setText(url);
}

void NotificationWindow::onMarkAsRead() {
    emit actionRequested("markAsRead", m_notification.id, m_notification.url);
}

void NotificationWindow::onMarkAsDone() {
    emit actionRequested("markAsDone", m_notification.id, m_notification.url);
    close();
}

void NotificationWindow::onCloseAndMarkAsRead() {
    emit actionRequested("markAsRead", m_notification.id, m_notification.url);
    close();
}

void NotificationWindow::onViewRawJson() {
    QJsonObject json = m_notification.toJson();
    QJsonDocument doc(json);
    QString rawJson = QString::fromUtf8(doc.toJson(QJsonDocument::Indented));

    QDialog *dialog = new QDialog(this);
    dialog->setWindowTitle(tr("Raw JSON"));
    dialog->resize(600, 400);

    QVBoxLayout *layout = new QVBoxLayout(dialog);
    QTextEdit *textEdit = new QTextEdit(dialog);
    textEdit->setReadOnly(true);
    textEdit->setPlainText(rawJson);
    layout->addWidget(textEdit);

    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void NotificationWindow::onViewPullRequest() {
    PullRequestWindow *win = new PullRequestWindow(m_notification, m_client, this);
    win->setAttribute(Qt::WA_DeleteOnClose);
    win->show();
}

void NotificationWindow::onViewActionJob() {
    ActionWindow *win = new ActionWindow(m_notification, m_client, this);
    win->setAttribute(Qt::WA_DeleteOnClose);
    win->show();
}
