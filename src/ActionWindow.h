#ifndef ACTIONWINDOW_H
#define ACTIONWINDOW_H

#include <KXmlGuiWindow>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QVBoxLayout>
#include <QLabel>
#include <QTableWidget>
#include <QTextEdit>

#include "Notification.h"
#include "GitHubClient.h"

class ActionWindow : public KXmlGuiWindow {
    Q_OBJECT

public:
    explicit ActionWindow(const Notification &n, GitHubClient *client, QWidget *parent = nullptr);

private slots:
    void fetchRunDetails();
    void onRunDetailsReply(QNetworkReply *reply);

    void fetchJobs();
    void onJobsReply(QNetworkReply *reply);

private:
    Notification m_notification;
    GitHubClient *m_client;
    QNetworkAccessManager *m_manager;

    QLabel *m_statusLabel;
    QTableWidget *m_jobsTable;
    QString m_jobsUrl;

    void setupUi();
};

#endif // ACTIONWINDOW_H
