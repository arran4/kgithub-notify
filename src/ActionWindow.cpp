#include "ActionWindow.h"

#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>

ActionWindow::ActionWindow(const Notification &n, GitHubClient *client, QWidget *parent)
    : KXmlGuiWindow(parent, Qt::Window),
      m_notification(n),
      m_client(client),
      m_manager(new QNetworkAccessManager(this)) {
    setWindowTitle(tr("Action Run - %1").arg(n.title));
    resize(700, 500);

    setupUi();

    fetchRunDetails();
}

void ActionWindow::setupUi() {
    QWidget *centralWidget = new QWidget(this);
    setupGUI(Default, ":/kgithub-notifyui.rc");
    setCentralWidget(centralWidget);

    QVBoxLayout *layout = new QVBoxLayout(centralWidget);

    m_statusLabel = new QLabel(tr("<b>Status:</b> Loading..."));
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);

    m_jobsTable = new QTableWidget();
    m_jobsTable->setColumnCount(4);
    m_jobsTable->setHorizontalHeaderLabels({tr("Job Name"), tr("Status"), tr("Conclusion"), tr("Started At")});
    m_jobsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_jobsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_jobsTable->setSelectionBehavior(QAbstractItemView::SelectRows);

    layout->addWidget(m_jobsTable);
}

void ActionWindow::fetchRunDetails() {
    QUrl url(m_notification.url);
    QNetworkRequest request = m_client->createAuthenticatedRequest(url);
    QNetworkReply *reply = m_manager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { onRunDetailsReply(reply); });
}

void ActionWindow::onRunDetailsReply(QNetworkReply *reply) {
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonObject obj = doc.object();

        QString status = obj["status"].toString();
        QString conclusion = obj["conclusion"].toString();
        QString name = obj["name"].toString();

        m_jobsUrl = obj["jobs_url"].toString();

        m_statusLabel->setText(
            tr("<b>Run Name:</b> %1<br><b>Status:</b> %2<br><b>Conclusion:</b> %3").arg(name, status, conclusion));

        fetchJobs();
    } else {
        QMessageBox::warning(this, tr("Error"), tr("Failed to fetch action details: %1").arg(reply->errorString()));
    }
    reply->deleteLater();
}

void ActionWindow::fetchJobs() {
    if (m_jobsUrl.isEmpty()) return;

    QUrl url(m_jobsUrl);
    QNetworkRequest request = m_client->createAuthenticatedRequest(url);
    QNetworkReply *reply = m_manager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { onJobsReply(reply); });
}

void ActionWindow::onJobsReply(QNetworkReply *reply) {
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonObject obj = doc.object();
        QJsonArray jobs = obj["jobs"].toArray();

        m_jobsTable->setRowCount(0);
        for (int i = 0; i < jobs.size(); ++i) {
            QJsonObject job = jobs[i].toObject();

            QString name = job["name"].toString();
            QString status = job["status"].toString();
            QString conclusion = job["conclusion"].toString();
            QString startedAt = job["started_at"].toString();

            m_jobsTable->insertRow(i);
            m_jobsTable->setItem(i, 0, new QTableWidgetItem(name));
            m_jobsTable->setItem(i, 1, new QTableWidgetItem(status));
            m_jobsTable->setItem(i, 2, new QTableWidgetItem(conclusion));
            m_jobsTable->setItem(i, 3, new QTableWidgetItem(startedAt));
        }
    } else {
        QMessageBox::warning(this, tr("Error"), tr("Failed to fetch jobs: %1").arg(reply->errorString()));
    }
    reply->deleteLater();
}
