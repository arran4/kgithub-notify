#ifndef REPOLISTWINDOW_H
#define REPOLISTWINDOW_H

#include <QMainWindow>
#include <QTableWidget>
#include <QToolBar>
#include <QStatusBar>
#include <QLabel>
#include <QTimer>
#include <QDateTime>
#include <QJsonArray>
#include "GitHubClient.h"

class RepoListWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit RepoListWindow(GitHubClient *client, QWidget *parent = nullptr);

private slots:
    void onRefreshClicked();
    void onExportClicked();
    void onReposReceived(const QJsonArray &repos, const QString &nextPageUrl);
    void updateTimerLabel();
    void onCustomContextMenuRequested(const QPoint &pos);
    void onError(const QString &error);

private:
    void setupUI();
    void loadCache();
    void saveCache();
    void addReposToTable(const QJsonArray &repos);

    GitHubClient *m_client;
    QTableWidget *m_table;
    QToolBar *m_toolbar;
    QStatusBar *m_statusBar;
    QLabel *m_timerLabel;
    QTimer *m_updateTimer;
    QDateTime m_lastRefresh;
    QJsonArray m_allRepos;
};

#endif // REPOLISTWINDOW_H