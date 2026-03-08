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
#include <QMainWindow>

class GitHubClient;

class TrendingWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit TrendingWindow(GitHubClient *client, QWidget *parent = nullptr);

private slots:
    void onRefreshClicked();
    void onItemActivated(QTableWidgetItem *item);
    void onModeChanged(int index);
    void onRawDataReceived(const QByteArray &data);

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
};

#endif // TRENDINGWINDOW_H
