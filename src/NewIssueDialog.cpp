#include "NewIssueDialog.h"

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QUrl>
#include <QVBoxLayout>

NewIssueDialog::NewIssueDialog(GitHubClient *client, QWidget *parent)
    : QDialog(parent), m_client(client), m_verifyTimer(new QTimer(this)), m_isFetchingRepos(false) {
    setupUI();

    m_verifyTimer->setSingleShot(true);
    connect(m_verifyTimer, &QTimer::timeout, this, &NewIssueDialog::verifyRepo);

    connect(m_client, &GitHubClient::repoVerified, this, &NewIssueDialog::onRepoVerified);
    connect(m_client, &GitHubClient::userReposReceived, this, &NewIssueDialog::onReposReceived);
    connect(m_client, &GitHubClient::errorOccurred, this, &NewIssueDialog::onErrorOccurred);
    connect(m_client, &GitHubClient::issueCreated, this, &NewIssueDialog::onIssueCreated);

    loadCache();
}

void NewIssueDialog::setupUI() {
    setWindowTitle(tr("New Issue"));
    resize(500, 450);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    QLabel *instructionLabel = new QLabel(tr("Repository:"), this);
    mainLayout->addWidget(instructionLabel);

    QHBoxLayout *repoLayout = new QHBoxLayout();
    m_repoComboBox = new QComboBox(this);
    m_repoComboBox->setEditable(true);
    m_repoComboBox->setInsertPolicy(QComboBox::NoInsert);

    QCompleter *completer = m_repoComboBox->completer();
    if (completer) {
        completer->setCompletionMode(QCompleter::PopupCompletion);
        completer->setFilterMode(Qt::MatchContains);
    }

    m_repoComboBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_repoComboBox->setCurrentIndex(-1);  // Start empty
    connect(m_repoComboBox, &QComboBox::currentTextChanged, this, &NewIssueDialog::onRepoTextChanged);
    repoLayout->addWidget(m_repoComboBox);

    m_refreshButton = new QPushButton(tr("Refresh"), this);
    connect(m_refreshButton, &QPushButton::clicked, this, &NewIssueDialog::onRefreshClicked);
    repoLayout->addWidget(m_refreshButton);
    mainLayout->addLayout(repoLayout);

    QLabel *titleLabel = new QLabel(tr("Title:"), this);
    mainLayout->addWidget(titleLabel);

    m_titleEdit = new QLineEdit(this);
    mainLayout->addWidget(m_titleEdit);

    QLabel *assigneeLabel = new QLabel(tr("Assignee (username, optional):"), this);
    mainLayout->addWidget(assigneeLabel);

    m_assigneeEdit = new QLineEdit(this);
    mainLayout->addWidget(m_assigneeEdit);

    QLabel *bodyLabel = new QLabel(tr("Body:"), this);
    mainLayout->addWidget(bodyLabel);

    m_bodyEdit = new QTextEdit(this);
    mainLayout->addWidget(m_bodyEdit);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet("color: gray;");
    m_statusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_statusLabel->setWordWrap(true);
    mainLayout->addWidget(m_statusLabel);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    QPushButton *cancelButton = new QPushButton(tr("Cancel"), this);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    buttonLayout->addWidget(cancelButton);

    m_createButton = new QPushButton(tr("Create Issue"), this);
    m_createButton->setEnabled(false);  // Disabled until verified
    connect(m_createButton, &QPushButton::clicked, this, &NewIssueDialog::onCreateClicked);
    buttonLayout->addWidget(m_createButton);

    mainLayout->addLayout(buttonLayout);
}

void NewIssueDialog::loadCache() {
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    QString cachePath = dir.filePath("repos_cache.json");
    QFile file(cachePath);

    if (file.open(QIODevice::ReadOnly)) {
        QByteArray data = file.readAll();
        file.close();

        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            if (obj.contains("repos") && obj["repos"].isArray()) {
                m_allRepos = obj["repos"].toArray();

                m_repoComboBox->clear();
                QStringList repoNames;
                for (int i = 0; i < m_allRepos.size(); ++i) {
                    QJsonObject repo = m_allRepos[i].toObject();
                    QString fullName = repo["owner"].toObject()["login"].toString() + "/" + repo["name"].toString();
                    repoNames << fullName;
                }
                repoNames.sort();
                m_repoComboBox->addItems(repoNames);
                m_repoComboBox->setCurrentIndex(-1);  // Empty by default
            }
        }
    }
}

void NewIssueDialog::saveCache() {
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    dir.mkpath(".");

    QString cachePath = dir.filePath("repos_cache.json");

    QJsonObject obj;
    // Keep existing last_refresh if any
    QFile existingFile(cachePath);
    if (existingFile.open(QIODevice::ReadOnly)) {
        QJsonDocument existingDoc = QJsonDocument::fromJson(existingFile.readAll());
        if (existingDoc.isObject() && existingDoc.object().contains("last_refresh")) {
            obj["last_refresh"] = existingDoc.object()["last_refresh"];
        }
        existingFile.close();
    }

    QFile file(cachePath);
    if (file.open(QIODevice::WriteOnly)) {
        obj["repos"] = m_allRepos;

        QJsonDocument doc(obj);
        file.write(doc.toJson());
        file.close();
    }
}

void NewIssueDialog::onRepoTextChanged(const QString &text) {
    m_createButton->setEnabled(false);

    // Basic format check
    if (text.isEmpty() || !text.contains("/")) {
        m_statusLabel->setText("");
        return;
    }

    m_statusLabel->setText(tr("Verifying repository..."));
    m_statusLabel->setStyleSheet("color: gray;");

    m_currentVerifyRepo = text.trimmed();
    m_verifyTimer->start(1000);  // 1 second debounce
}

void NewIssueDialog::verifyRepo() {
    if (m_currentVerifyRepo.isEmpty()) return;

    // First check if it's in our cache
    for (int i = 0; i < m_allRepos.size(); ++i) {
        QJsonObject repo = m_allRepos[i].toObject();
        QString fullName = repo["owner"].toObject()["login"].toString() + "/" + repo["name"].toString();
        if (fullName.compare(m_currentVerifyRepo, Qt::CaseInsensitive) == 0) {
            m_statusLabel->setText(tr("Repository found in cache."));
            m_statusLabel->setStyleSheet("color: green;");
            m_createButton->setEnabled(true);
            return;
        }
    }

    // Not in cache, verify via API
    m_client->verifyRepo(m_currentVerifyRepo);
}

void NewIssueDialog::onRepoVerified(const QString &repoFullName, bool exists) {
    if (repoFullName.compare(m_currentVerifyRepo, Qt::CaseInsensitive) != 0) return;

    if (exists) {
        m_statusLabel->setText(tr("Repository verified successfully."));
        m_statusLabel->setStyleSheet("color: green;");
        m_createButton->setEnabled(true);
    } else {
        m_statusLabel->setText(tr("Repository not found or not accessible."));
        m_statusLabel->setStyleSheet("color: red;");
        m_createButton->setEnabled(false);
    }
}

void NewIssueDialog::onCreateClicked() {
    QString repoName = m_repoComboBox->currentText().trimmed();
    QString title = m_titleEdit->text().trimmed();
    QString body = m_bodyEdit->toPlainText();
    QString assignee = m_assigneeEdit->text().trimmed();

    if (repoName.isEmpty() || title.isEmpty()) {
        m_statusLabel->setText(tr("Title and Repository are required."));
        m_statusLabel->setStyleSheet("color: red;");
        return;
    }

    m_createButton->setEnabled(false);
    m_statusLabel->setText(tr("Creating issue..."));
    m_statusLabel->setStyleSheet("color: gray;");

    m_client->createIssue(repoName, title, body, assignee);
}

void NewIssueDialog::onIssueCreated(const QByteArray &data) {
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isObject() && doc.object().contains("html_url")) {
        QString url = doc.object()["html_url"].toString();
        m_statusLabel->setText(tr("Issue created successfully."));
        m_statusLabel->setStyleSheet("color: green;");
        QDesktopServices::openUrl(QUrl(url));
        accept();
    } else {
        QString errMsg = tr("Failed to create issue.");
        if (doc.isObject() && doc.object().contains("message")) {
            errMsg += " " + doc.object()["message"].toString();
        }

        if (errMsg.contains("Not Found") || errMsg.contains("404")) {
            errMsg +=
                "\n\n(Hint: Ensure the repository exists, has Issues enabled, and your Personal Access Token has the "
                "'repo', 'public_repo', or 'repo:status' scope depending on repository visibility.)";
        }
        m_statusLabel->setText(errMsg);
        m_statusLabel->setStyleSheet("color: red;");
        m_createButton->setEnabled(true);
    }
}

void NewIssueDialog::onRefreshClicked() {
    m_isFetchingRepos = true;
    m_refreshButton->setEnabled(false);
    m_statusLabel->setText(tr("Refreshing cache..."));
    m_statusLabel->setStyleSheet("color: gray;");
    m_allRepos = QJsonArray();
    m_client->fetchUserRepos();
}

void NewIssueDialog::onReposReceived(const QJsonArray &repos, const QString &nextPageUrl) {
    if (!m_isFetchingRepos) return;
    for (int i = 0; i < repos.size(); ++i) {
        m_allRepos.append(repos[i]);
    }

    if (!nextPageUrl.isEmpty()) {
        m_client->fetchUserRepos(nextPageUrl);
    } else {
        saveCache();
        loadCache();

        m_refreshButton->setEnabled(true);
        m_statusLabel->setText(tr("Cache refreshed."));
        m_statusLabel->setStyleSheet("color: green;");

        m_isFetchingRepos = false;
        // Re-verify current text if any
        if (!m_repoComboBox->currentText().isEmpty()) {
            onRepoTextChanged(m_repoComboBox->currentText());
        }
    }
}

void NewIssueDialog::onErrorOccurred(const QString &error) {
    m_refreshButton->setEnabled(true);
    m_createButton->setEnabled(true);
    QString errMsg = tr("Error: %1").arg(error);
    if (error.contains("404") || error.contains("Not Found")) {
        errMsg +=
            "\n\n(Hint: Ensure the repository exists, has Issues enabled, and your Personal Access Token has the "
            "'repo', 'public_repo', or 'repo:status' scope depending on repository visibility.)";
    }
    m_statusLabel->setText(errMsg);
    m_statusLabel->setStyleSheet("color: red;");
}

void NewIssueDialog::setInitialRepo(const QString &repoFullName) {
    int index = m_repoComboBox->findText(repoFullName, Qt::MatchExactly);
    if (index >= 0) {
        m_repoComboBox->setCurrentIndex(index);
    } else {
        m_repoComboBox->setCurrentText(repoFullName);
    }
}
