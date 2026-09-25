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

struct PREvent {
    QString id;
    QString sourceFamily;  // E.g., the raw event string like 'labeled', 'committed', 'review_dismissed' for stable identity
    QString sourceFingerprint; // A serialized deterministic subset of source-specific keys required to disambiguate identical-family ID-less events (e.g. cross-referenced 'source')
    enum Type { Body, IssueComment, ReviewComment, TimelineEvent } type;
    QDateTime timestamp;
    QString author;
    QString body;
    QString path;
    QString diffHunk;
    QString actionText;

    bool operator<(const PREvent& other) const {
        if (timestamp != other.timestamp) return timestamp < other.timestamp;
        if (type != other.type) return type < other.type;
        if (sourceFamily != other.sourceFamily) return sourceFamily < other.sourceFamily;
        if (sourceFingerprint != other.sourceFingerprint) return sourceFingerprint < other.sourceFingerprint;
        if (!id.isEmpty() && !other.id.isEmpty() && id != other.id) return id < other.id;
        if (id != other.id)
            return id < other.id;  // Only applies if one is empty and the other isn't, providing deterministic fallback order
        if (author != other.author) return author < other.author;
        if (body != other.body) return body < other.body;
        if (actionText != other.actionText) return actionText < other.actionText;
        if (path != other.path) return path < other.path;
        return diffHunk < other.diffHunk;
    }

    bool operator==(const PREvent& other) const {
        if (type != other.type) {
            return false;
        }
        if (sourceFamily != other.sourceFamily) {
            return false;
        }
        if (sourceFingerprint != other.sourceFingerprint) {
            return false;
        }
        if (!id.isEmpty() && !other.id.isEmpty()) {
            return id == other.id;
        }
        if (timestamp != other.timestamp) {
            return false;
        }
        return author == other.author && body == other.body && actionText == other.actionText && path == other.path &&
               diffHunk == other.diffHunk;
    }
};

struct CollectionState {
    QString nextUrl;
    bool isLoading = false;
    bool isComplete = false;
    bool isFailed = false;
    QString errorString;
    QUuid generation;
};

class PullRequestWindow : public KXmlGuiWindow {
    Q_OBJECT
    friend class TestRequestConsumers;

   public:
    explicit PullRequestWindow(const Notification& n, GitHubClient* client, QWidget* parent = nullptr,
                               QNetworkAccessManager* networkManager = nullptr);

   private slots:
    void fetchPrDetails();
    void onPrDetailsReply(QNetworkReply* reply);

    void fetchTimeline(const QString& urlStr = QString());
    void onTimelineReply(QNetworkReply* reply);

    void fetchReviewComments(const QString& urlStr = QString());
    void onReviewCommentsReply(QNetworkReply* reply);

    void fetchCommits(const QString& urlStr = QString());
    void onCommitsReply(QNetworkReply* reply);

    void fetchFiles(const QString& urlStr = QString());
    void onFilesReply(QNetworkReply* reply);

    void onFileDoubleClicked(int row, int column);

    void onCommentButtonClicked();
    void onPostCommentReply(QNetworkReply* reply);
    void onViewRawJson();

   private:
    Notification m_notification;
    GitHubClient* m_client;
    QNetworkAccessManager* m_manager;
    QUuid m_detailsGeneration;
    QUuid m_commentRequestId;
    QLabel* m_requestStatus;
    QPushButton* m_retryButton;

    QTabWidget* m_tabWidget;

    CollectionState m_timelineState;
    CollectionState m_reviewState;
    CollectionState m_commitsState;
    CollectionState m_filesState;

    QList<PREvent> m_events;
    void updateConversationUi();
    void updateCollectionStatusUi();
    QString parseNextLink(QNetworkReply* reply);

    // Conversation Tab
    QWidget* m_conversationTab;
    QHBoxLayout* m_conversationLayout;
    QLabel* m_conversationStatusLabel;
    QPushButton* m_conversationRetryBtn;
    QScrollArea* m_commentsScrollArea;
    QWidget* m_commentsContainer;
    QVBoxLayout* m_commentsContainerLayout;
    QTextEdit* m_replyEdit;
    QPushButton* m_commentButton;

    // Commits Tab
    QWidget* m_commitsTab;
    QVBoxLayout* m_commitsLayout;
    QLabel* m_commitsStatusLabel;
    QPushButton* m_commitsRetryBtn;
    QTableWidget* m_commitsTable;

    // Changed Files Tab
    QWidget* m_filesTab;
    QVBoxLayout* m_filesLayout;
    QLabel* m_filesStatusLabel;
    QPushButton* m_filesRetryBtn;
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
