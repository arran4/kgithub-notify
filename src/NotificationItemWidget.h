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

    QString getTitle() const { return titleLabel->text(); }

signals:
    // No specific signals yet
};

#endif // NOTIFICATIONITEMWIDGET_H
