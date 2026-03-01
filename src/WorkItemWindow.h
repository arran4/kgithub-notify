#ifndef WORKITEMWINDOW_H
#define WORKITEMWINDOW_H

#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
#include <QNetworkReply>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QAction>
#include "GitHubClient.h"

class WorkItemWindow : public QDialog {
    Q_OBJECT

public:
    enum Type {
        Issues,
        PullRequests
    };

    explicit WorkItemWindow(GitHubClient *client, Type type, QWidget *parent = nullptr);
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
    Type m_type;
    QTableWidget *m_table;
    QPushButton *m_exportCsvBtn;
    QPushButton *m_exportJsonBtn;
    QAction *m_openAction;
    QAction *m_copyAction;
    QNetworkAccessManager *m_manager;

    void setupUi();
    void loadData();
    void populateTable(const QByteArray &data);
    QString getHtmlUrlForRow(int row) const;
};

#endif // WORKITEMWINDOW_H