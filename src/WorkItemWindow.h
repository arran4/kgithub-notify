#ifndef WORKITEMWINDOW_H
#define WORKITEMWINDOW_H

#include <KXmlGuiWindow>
#include <QTableWidget>
#include <QPushButton>
#include <QNetworkReply>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QtGui/QAction>
#include <QLabel>
#include <QJsonArray>
#include "GitHubClient.h"

class WorkItemWindow : public KXmlGuiWindow {
    Q_OBJECT

public:
    enum EndpointType {
        EndpointIssues,
        EndpointRepositories
    };

    explicit WorkItemWindow(GitHubClient *client, const QString& windowTitle, EndpointType endpointType, const QString& baseQuery, QWidget *parent = nullptr);
    ~WorkItemWindow();

private slots:
    void onReplyFinished(QNetworkReply *reply);
    void exportToCsv();
    void exportToJson();
    void onCustomContextMenuRequested(const QPoint &pos);
    void onItemDoubleClicked(QTableWidgetItem *item);
    void openInBrowser();
    void copyLink();

private:
    GitHubClient *m_client;
    QString m_windowTitle;
    EndpointType m_endpointType;
    QString m_baseQuery;
    int m_currentPage;
    QJsonArray m_allData;
    QTableWidget *m_table;
    QLabel *m_statusLabel;
    QAction *m_openAction;
    QAction *m_copyAction;
    QNetworkAccessManager *m_manager;

    void setupUi();
    void loadData(int page = 1);
    void appendRow(const QJsonObject &item);
    QString getHtmlUrlForRow(int row) const;
    QString getCacheFilePath() const;
    void loadCache();
    void saveCache();
};

#endif // WORKITEMWINDOW_H