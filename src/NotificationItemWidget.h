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
    QLabel *titleLabel;
    QLabel *repoLabel;
    QLabel *typeLabel;
    QLabel *dateLabel;
    QLabel *urlLabel;
    QLabel *authorLabel;

    QString getTitle() const { return titleLabel->text(); }
    void setDetails(const QString &details);

signals:
    void linkActivated(const QString &id, const QString &apiUrl, const QString &htmlUrl);

private:
    QString m_notificationId;
    QString m_apiUrl;
};

#endif // NOTIFICATIONITEMWIDGET_H
