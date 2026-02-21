#ifndef NOTIFICATIONITEMWIDGET_H
#define NOTIFICATIONITEMWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include "GitHubClient.h"

class NotificationItemWidget : public QWidget {
    Q_OBJECT
public:
    explicit NotificationItemWidget(const Notification &notification, QWidget *parent = nullptr);

    QCheckBox *checkBox;
    QLabel *avatarLabel;
    QLabel *titleLabel;
    QLabel *repoLabel;
    QLabel *authorLabel;
    QLabel *typeLabel;
    QLabel *dateLabel;
    QLabel *urlLabel;

    QString getTitle() const { return titleLabel->text(); }
    void setAuthor(const QString &name, const QPixmap &avatar);
    void setHtmlUrl(const QString &url);

signals:
    // No specific signals yet
};

#endif // NOTIFICATIONITEMWIDGET_H
