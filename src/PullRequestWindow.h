#ifndef PULLREQUESTWINDOW_H
#define PULLREQUESTWINDOW_H

#include <KXmlGuiWindow>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPushButton>
#include <QScrollArea>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "GitHubClient.h"
#include "Notification.h"

class PullRequestWindow : public KXmlGuiWindow {
    Q_OBJECT
   public:
    explicit PullRequestWindow(const Notification& n, GitHubClient* client, QWidget* parent = nullptr);

   private slots:
    void fetchPrDetails();
    void onPrDetailsReply(QNetworkReply* reply);

    void fetchTimeline();
    void onTimelineReply(QNetworkReply* reply);

    void fetchReviewComments();
    void onReviewCommentsReply(QNetworkReply* reply);

    void fetchCommits();
    void onCommitsReply(QNetworkReply* reply);

    void fetchFiles();
    void onFilesReply(QNetworkReply* reply);

    void onFileDoubleClicked(int row, int column);

    void onCommentButtonClicked();
    void onPostCommentReply(QNetworkReply* reply);
    void onViewRawJson();

   private:
    Notification m_notification;
    GitHubClient* m_client;
    QNetworkAccessManager* m_manager;

    QTabWidget* m_tabWidget;

    // Conversation Tab
    QWidget* m_conversationTab;
    QHBoxLayout* m_conversationLayout;
    QScrollArea* m_commentsScrollArea;
    QWidget* m_commentsContainer;
    QVBoxLayout* m_commentsContainerLayout;
    QTextEdit* m_replyEdit;
    QPushButton* m_commentButton;

    // Commits Tab
    QWidget* m_commitsTab;
    QVBoxLayout* m_commitsLayout;
    QTableWidget* m_commitsTable;

    // Changed Files Tab
    QWidget* m_filesTab;
    QVBoxLayout* m_filesLayout;
    QTableWidget* m_filesTable;

    // Conversation Tab RHS (Tool Window)
    QLabel* m_openedByLabel;
    QLabel* m_createdAtLabel;
    QLabel* m_updatedAtLabel;
    QLabel* m_convAssigneesLabel;
    QLabel* m_convLabelsLabel;
    QLabel* m_convMilestoneLabel;

    // Metadata Tab
    QWidget* m_metadataTab;
    QVBoxLayout* m_metadataLayout;
    QLabel* m_labelsLabel;
    QLabel* m_assigneesLabel;
    QLabel* m_milestoneLabel;

    QString m_commentsUrl;
    QString m_reviewCommentsUrl;
    QString m_commitsUrl;
    QString m_issueCommentsUrl;
    QString m_timelineUrl;

    void setupUi();
    void addCommentToUI(const QString& author, const QString& body, const QString& createdAt);
    void setupMenus();
    QString m_rawJsonStr;
};

#endif  // PULLREQUESTWINDOW_H
