#ifndef NOTIFICATIONITEMWIDGET_H
#define NOTIFICATIONITEMWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QCheckBox>
#include <QToolButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include "GitHubClient.h"

class NotificationItemWidget : public QWidget {
    Q_OBJECT
public:
    explicit NotificationItemWidget(const Notification &notification, QWidget *parent = nullptr);

    QToolButton *doneButton;
    QToolButton *saveButton;
    QToolButton *openButton;
    QLabel *avatarLabel;
    QLabel *titleLabel;
    QLabel *repoLabel;
    QLabel *authorLabel;
    QLabel *typeLabel;
    QLabel *dateLabel;
    QLabel *urlLabel;
    QLabel *errorLabel;
    QLabel *loadingLabel;
    QLabel *unreadIndicator;
    QLabel *savedIndicator;
    QLabel *doneIndicator;

    QString getTitle() const { return titleLabel->text(); }
    void setAuthor(const QString &name, const QPixmap &avatar);
    void setHtmlUrl(const QString &url);
    void setError(const QString &error);
    void setRead(bool read);
    void setSaved(bool saved);
    void setDone(bool done);
    void setLoading(bool loading);
    bool isLoading() const { return m_isLoading; }

signals:
    void doneClicked();
    void saveClicked();
    void openClicked();

private:
    bool m_isLoading;
};

#endif // NOTIFICATIONITEMWIDGET_H
