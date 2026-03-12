#include "MockNetworkAccessManager.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QDateTime>
#include <QTimer>
#include <QDebug>

MockReply::MockReply(QObject *parent, const QByteArray &data, const QNetworkRequest &req)
    : QNetworkReply(parent), m_data(data), m_offset(0) {
    setRequest(req);
    setUrl(req.url());
    setOperation(QNetworkAccessManager::GetOperation);
    open(QIODevice::ReadOnly | QIODevice::Unbuffered);

    QTimer::singleShot(100, this, [this]() {
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 200);
        emit readyRead();
        emit finished();
    });
}

MockReply::~MockReply() {}

qint64 MockReply::bytesAvailable() const {
    return m_data.size() - m_offset + QIODevice::bytesAvailable();
}

qint64 MockReply::readData(char *data, qint64 maxlen) {
    if (m_offset >= m_data.size()) return 0;
    qint64 number = qMin(maxlen, m_data.size() - m_offset);
    memcpy(data, m_data.constData() + m_offset, number);
    m_offset += number;
    return number;
}

MockNetworkAccessManager::MockNetworkAccessManager(QObject *parent) : QNetworkAccessManager(parent) {}

QNetworkReply *MockNetworkAccessManager::createRequest(Operation op, const QNetworkRequest &req, QIODevice *outgoingData) {
    QString urlStr = req.url().toString();
    QByteArray responseData;

    qDebug() << "Mock request intercepted:" << urlStr;

    if (urlStr.contains("/search/issues") || urlStr.contains("/search/repositories")) {
        // Handled by requestRaw in our earlier mock but just in case
        QJsonObject root;
        root["total_count"] = 1;
        QJsonArray items;
        QJsonObject item;
        item["title"] = "Mock Item from intercepted request";
        item["html_url"] = "https://github.com/mock/mock/issues/1";
        item["created_at"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        item["state"] = "open";
        QJsonObject user;
        user["login"] = "intercept_mock";
        item["user"] = user;
        items.append(item);
        root["items"] = items;
        responseData = QJsonDocument(root).toJson();
    }
    else if (urlStr.endsWith("/pulls/10") || urlStr.contains("/pulls/10?")) {
        QJsonObject pr;
        pr["title"] = "Fix crash on startup";
        pr["body"] = "This PR addresses the critical crash on startup. Please review.";
        pr["html_url"] = "https://github.com/arran4/Kgithub-notify/pull/10";
        pr["state"] = "open";
        QJsonObject user;
        user["login"] = "jules_dev";
        user["avatar_url"] = "https://github.com/identicons/jules.png";
        pr["user"] = user;
        QJsonObject head; head["ref"] = "fix/crash";
        QJsonObject base; base["ref"] = "main";
        pr["head"] = head;
        pr["base"] = base;
        pr["mergeable"] = true;
        pr["additions"] = 15;
        pr["deletions"] = 2;
        responseData = QJsonDocument(pr).toJson();
    }
    else if (urlStr.endsWith("/comments") && urlStr.contains("issues/10")) {
        QJsonArray comments;
        QJsonObject comment;
        comment["body"] = "Looks good to me!";
        comment["created_at"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        QJsonObject user; user["login"] = "reviewer1";
        comment["user"] = user;
        comments.append(comment);
        responseData = QJsonDocument(comments).toJson();
    }
    else if (urlStr.endsWith("/comments") && urlStr.contains("pulls/10")) {
        QJsonArray comments;
        QJsonObject comment;
        comment["body"] = "Could you add a test for this edge case?";
        comment["created_at"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        QJsonObject user; user["login"] = "lead_dev";
        comment["user"] = user;
        comments.append(comment);
        responseData = QJsonDocument(comments).toJson();
    }
    else if (urlStr.endsWith("/commits")) {
        QJsonArray commits;
        QJsonObject commitWrapper;
        QJsonObject commit;
        commit["message"] = "Fix null pointer exception during init";
        QJsonObject author; author["name"] = "Jules";
        commit["author"] = author;
        commitWrapper["commit"] = commit;
        commitWrapper["html_url"] = "https://github.com/mock/commit/123";
        commits.append(commitWrapper);
        responseData = QJsonDocument(commits).toJson();
    }
    else if (urlStr.endsWith("/files")) {
        QJsonArray files;
        QJsonObject file;
        file["filename"] = "src/main.cpp";
        file["status"] = "modified";
        file["additions"] = 15;
        file["deletions"] = 2;
        files.append(file);
        responseData = QJsonDocument(files).toJson();
    }
    else if (urlStr.contains("/actions/runs/123/jobs")) {
        QJsonObject root;
        QJsonArray jobs;
        QJsonObject job;
        job["name"] = "build-and-test (ubuntu-latest)";
        job["status"] = "completed";
        job["conclusion"] = "failure";
        job["html_url"] = "https://github.com/mock/runs/123/job/1";
        QJsonArray steps;
        QJsonObject step1; step1["name"] = "Checkout"; step1["status"] = "completed"; step1["conclusion"] = "success";
        QJsonObject step2; step2["name"] = "Run Tests"; step2["status"] = "completed"; step2["conclusion"] = "failure";
        steps.append(step1); steps.append(step2);
        job["steps"] = steps;
        jobs.append(job);
        root["jobs"] = jobs;
        responseData = QJsonDocument(root).toJson();
    }
    else if (urlStr.contains("/actions/runs/123")) {
        QJsonObject run;
        run["name"] = "CI Pipeline";
        run["status"] = "completed";
        run["conclusion"] = "failure";
        run["head_branch"] = "main";
        run["event"] = "push";
        run["html_url"] = "https://github.com/arran4/Kgithub-notify/actions/runs/123";
        QJsonObject repo; repo["full_name"] = "arran4/Kgithub-notify";
        run["repository"] = repo;
        QJsonObject headCommit; headCommit["message"] = "Merge pull request #9";
        run["head_commit"] = headCommit;
        responseData = QJsonDocument(run).toJson();
    }
    else {
        // Default empty array/object
        responseData = "[]";
    }

    return new MockReply(this, responseData, req);
}

// Need to update MockGitHubClient::createAuthenticatedRequest to retain url
// In .jules/mock/MockGitHubClient.cpp it was: request.setUrl(QUrl("file:///dev/null"));
// We need it to keep the original URL so MockNetworkAccessManager can see it.
