#ifndef NEWISSUEDIALOG_H
#define NEWISSUEDIALOG_H

#include <QComboBox>
#include <QCompleter>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QTimer>

#include "GitHubClient.h"

class NewIssueDialog : public QDialog {
    Q_OBJECT

   public:
    explicit NewIssueDialog(GitHubClient* client, QWidget* parent = nullptr);
    void setInitialRepo(const QString& repoFullName);

   private slots:
    void onRepoTextChanged(const QString& text);
    void verifyRepo();
    void onRepoVerified(const QString& repoFullName, bool exists);
    void onCreateClicked();
    void onRefreshClicked();
    void onReposReceived(const QJsonArray& repos, const QString& nextPageUrl);
    void onIssueCreated(const QByteArray& data);
    void onErrorOccurred(const QString& error);

   private:
    void setupUI();
    void loadCache();
    void saveCache();

    GitHubClient* m_client;
    QComboBox* m_repoComboBox;
    QLineEdit* m_titleEdit;
    QTextEdit* m_bodyEdit;
    QLineEdit* m_assigneeEdit;
    QPushButton* m_createButton;
    QPushButton* m_refreshButton;
    QLabel* m_statusLabel;
    QTimer* m_verifyTimer;

    QJsonArray m_allRepos;
    QString m_currentVerifyRepo;
    bool m_isFetchingRepos;
};

#endif  // NEWISSUEDIALOG_H
