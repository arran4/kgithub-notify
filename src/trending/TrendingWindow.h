#ifndef TRENDINGWINDOW_H
#define TRENDINGWINDOW_H

#include <QWidget>
#include <QComboBox>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPointer>
#include <QNetworkReply>

class GitHubClient;

class TrendingWindow : public QWidget {
    Q_OBJECT

public:
    explicit TrendingWindow(GitHubClient *client, QWidget *parent = nullptr);

private slots:
    void onRefreshClicked();
    void onItemActivated(QListWidgetItem *item);
    void onModeChanged(int index);
    void onRawDataReceived(const QByteArray &data);

private:
    QComboBox *modeComboBox;
    QComboBox *timeframeComboBox;
    QPushButton *refreshButton;
    QListWidget *listWidget;
    GitHubClient *m_client;

    // Store the last requested URL to ignore responses from other raw requests
    QString lastRequestedUrl;
};

#endif // TRENDINGWINDOW_H
