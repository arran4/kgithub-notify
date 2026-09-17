#ifndef WORKITEMWINDOW_H
#define WORKITEMWINDOW_H

#include <KXmlGuiWindow>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QMap>
#include <QNetworkReply>
#include <QPushButton>
#include <QSet>
#include <QTableWidget>
#include <QUuid>
#include <QVBoxLayout>
#include <QtGui/QAction>

#include "GitHubClient.h"

class QCloseEvent;

class WorkItemWindow : public KXmlGuiWindow {
    Q_OBJECT
    friend class TestRequestConsumers;

   public:
    enum EndpointType { EndpointIssues, EndpointRepositories };

    explicit WorkItemWindow(GitHubClient* client, const QString& windowTitle, EndpointType endpointType,
                            const QString& baseQuery, QWidget* parent = nullptr,
                            QNetworkAccessManager* manager = nullptr);
    ~WorkItemWindow() override;

    void startRefresh();

   protected:
    void closeEvent(QCloseEvent* event) override;

   private slots:
    void onReplyFinished(QNetworkReply* reply);
    void exportToCsv();
    void exportToJson();
    void onCustomContextMenuRequested(const QPoint& pos);
    void onItemDoubleClicked(QTableWidgetItem* item);
    void openInBrowser();
    void copyLink();

   private:
    GitHubClient* m_client;
    QString m_windowTitle;
    EndpointType m_endpointType;
    QString m_baseQuery;
    QJsonArray m_allData;
    QTableWidget* m_table;
    QLabel* m_statusLabel;
    QAction* m_openAction;
    QAction* m_copyAction;
    QNetworkAccessManager* m_manager;
    bool m_ownsManager = false;

    QUuid m_currentGenerationId;
    QMap<int, QJsonArray> m_stagedPages;
    QSet<int> m_inFlightPages;
    QSet<QNetworkReply*> m_inFlightReplies;
    int m_totalCount = 0;

    void setupUi();
    void loadData(int page = 1);
    void fetchPage(const QUuid& generationId, int page);
    void commitStagedData(int totalCount, int maxPages);
    void appendRow(const QJsonObject& item);
    QString getHtmlUrlForRow(int row) const;
    QString getCacheFilePath() const;
    void loadCache();
    void saveCache();
};

#endif  // WORKITEMWINDOW_H