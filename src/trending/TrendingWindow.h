#ifndef TRENDINGWINDOW_H
#define TRENDINGWINDOW_H

#include <QWidget>
#include <QComboBox>
#include <QTableWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPointer>
#include <QNetworkReply>
#include <QSet>
#include <QNetworkAccessManager>
#include <KXmlGuiWindow>

class GitHubClient;

class TrendingWindow : public KXmlGuiWindow {
    Q_OBJECT

public:
    explicit TrendingWindow(GitHubClient *client, QWidget *parent = nullptr);

private slots:
    void onRefreshClicked();
    void onItemActivated(QTableWidgetItem *item);
    void onModeChanged(int index);
    void onRawDataReceived(const QByteArray &data);
    void onRepoStarredCheckFinished(QNetworkReply *reply);
    void onItemSelectionChanged();

private:
    QComboBox *modeComboBox;
    QComboBox *timeframeComboBox;
    QComboBox *langComboBox;
    QComboBox *spokenLangComboBox;
    QPushButton *refreshButton;
    QTableWidget *tableWidget;
    GitHubClient *m_client;

    // Store the last requested URL to ignore responses from other raw requests
    QString lastRequestedUrl;
    QSet<QString> m_selectedUrls;
    QNetworkAccessManager *m_netManager;
};

#endif // TRENDINGWINDOW_H
