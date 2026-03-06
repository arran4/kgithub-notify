#ifndef NOTIFICATIONWINDOW_H
#define NOTIFICATIONWINDOW_H

#include <QWidget>
#include "Notification.h"

class NotificationWindow : public QWidget {
    Q_OBJECT
public:
    explicit NotificationWindow(const Notification &notification, QWidget *parent = nullptr);
};

#endif // NOTIFICATIONWINDOW_H
