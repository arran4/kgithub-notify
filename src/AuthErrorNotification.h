#ifndef AUTHERRORNOTIFICATION_H
#define AUTHERRORNOTIFICATION_H

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

class AuthErrorNotification : public QWidget {
    Q_OBJECT

   public:
    explicit AuthErrorNotification(QWidget *parent = nullptr);
    void setMessage(const QString &message);

   signals:
    void settingsClicked();
    void dismissed();

   private slots:
    void onSettingsClicked();
    void onDismissClicked();

   private:
    QLabel *messageLabel;
    QPushButton *settingsButton;
    QPushButton *dismissButton;
};

#endif  // AUTHERRORNOTIFICATION_H
