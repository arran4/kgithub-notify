#ifndef TRENDINGWINDOW_H
#define TRENDINGWINDOW_H

#include <KXmlGuiWindow>
#include <QComboBox>
#include <QHBoxLayout>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QPushButton>
#include <QSet>
#include <QUuid>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>

class GitHubClient;

class TrendingWindow : public KXmlGuiWindow {
    Q_OBJECT

   public:
    explicit TrendingWindow(GitHubClient* client, QWidget* parent = nullptr);

   private slots:
    void onRefreshClicked();
    void onItemActivated(QTableWidgetItem* item);
    void onModeChanged(int index);
    void onRawDataReceived(const QUuid& reqId, const QByteArray& data);
    void onErrorOccurred(const QUuid& reqId, const QString& error);
    void onRepoStarredCheckFinished(QNetworkReply* reply);
    void onItemSelectionChanged();

   private:
    QComboBox* modeComboBox;
    QComboBox* timeframeComboBox;
    QComboBox* langComboBox;
    QComboBox* spokenLangComboBox;
    QPushButton* refreshButton;
    QTableWidget* tableWidget;
    GitHubClient* m_client;
    QUuid m_currentReqId;

    // Store the last requested URL to ignore responses from other raw requests
    QString lastRequestedUrl;
    QSet<QString> m_selectedUrls;
    QNetworkAccessManager* m_netManager;
};

#endif  // TRENDINGWINDOW_H
