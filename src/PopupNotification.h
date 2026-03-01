#ifndef POPUPNOTIFICATION_H
#define POPUPNOTIFICATION_H

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

class PopupNotification : public QWidget {
    Q_OBJECT

   public:
    explicit PopupNotification(QWidget *parent = nullptr);
    void setMessage(const QString &message);
    void setSettingsVisible(bool visible);

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

#endif  // POPUPNOTIFICATION_H
