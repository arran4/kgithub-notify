#ifndef NOTIFICATIONITEMWIDGET_H
#define NOTIFICATIONITEMWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPixmap>
#include "GitHubClient.h"

class NotificationItemWidget : public QWidget {
    Q_OBJECT
public:
    explicit NotificationItemWidget(const Notification &notification, QWidget *parent = nullptr);

    QCheckBox *checkBox;
    QLabel *avatarLabel;
    QLabel *titleLabel;
    QLabel *repoLabel;
    QLabel *typeLabel;
    QLabel *authorLabel;
    QLabel *dateLabel;
    QLabel *urlLabel;

    QString getTitle() const { return titleLabel->text(); }
    void setAuthor(const QString &name, const QPixmap &avatar);
    void updateFromNotification(const Notification &n);

private:
    QString m_notificationId;

signals:
    // No specific signals yet
};

#endif // NOTIFICATIONITEMWIDGET_H
