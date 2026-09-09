#ifndef ACTIONWINDOW_H
#define ACTIONWINDOW_H

#include <KXmlGuiWindow>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPushButton>
#include <QTableWidget>
#include <QTextEdit>
#include <QVBoxLayout>

#include "GitHubClient.h"
#include "Notification.h"

class ActionWindow : public KXmlGuiWindow {
    Q_OBJECT
    friend class TestRequestConsumers;

   public:
    explicit ActionWindow(const Notification& n, GitHubClient* client, QWidget* parent = nullptr,
                          QNetworkAccessManager* networkManager = nullptr);

   private slots:
    void fetchRunDetails();
    void onRunDetailsReply(QNetworkReply* reply);

    void fetchJobs();
    void onJobsReply(QNetworkReply* reply);

   private:
    Notification m_notification;
    GitHubClient* m_client;
    QNetworkAccessManager* m_manager;
    QUuid m_detailsGeneration;
    QLabel* m_requestStatus;
    QPushButton* m_retryButton;

    QLabel* m_statusLabel;
    QTableWidget* m_jobsTable;
    QString m_jobsUrl;

    void setupUi();
};

#endif  // ACTIONWINDOW_H
