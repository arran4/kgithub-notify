#ifndef NOTIFICATIONPOPUP_H
#define NOTIFICATIONPOPUP_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>

class NotificationPopup : public QWidget {
    Q_OBJECT

public:
    explicit NotificationPopup(QWidget *parent = nullptr);
    void setTitle(const QString &title);
    void setMessage(const QString &message);
    void setOpenUrl(const QString &url);
    void showOpenButton(bool show);

signals:
    void openUrlClicked(const QString &url);
    void openClientClicked();
    void dismissed();

private slots:
    void onOpenClicked();
    void onClientClicked();
    void onDismissClicked();

private:
    QLabel *titleLabel;
    QLabel *messageLabel;
    QPushButton *openButton;
    QPushButton *clientButton;
    QPushButton *dismissButton;
    QString currentUrl;
    QTimer *autoCloseTimer;
};

#endif // NOTIFICATIONPOPUP_H
