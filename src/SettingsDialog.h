#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QFuture>
#include <QLineEdit>

class QComboBox;
class QPushButton;
class QLabel;
class GitHubClient;
class QCheckBox;

class SettingsDialog : public QDialog {
    Q_OBJECT
   public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    enum GetDataOption { Manual, FillScreen, GetAll, Infinite };
    Q_ENUM(GetDataOption)

    static QString getToken();
    static QFuture<QString> getTokenAsync();
    static int getInterval();
    static GetDataOption getGetDataOption();
    static int getSummaryThreshold();
    static int getNotificationDelayMs();
    static int getTrayUnreadLimit();
    static bool getNotifyOnce();
    static void setNotifyOnce(bool notify);
    static bool getNotifyRead();
    static void setNotifyRead(bool notify);

   private slots:
    void saveSettings();
    void onTestClicked();
    void onVerificationResult(bool valid, const QString &message);
    void installNotifyRc();

   private:
    void updateAutostartEntry();
    bool isAutostartEnabled();

    QLineEdit *tokenEdit;
    QComboBox *intervalCombo;
    QComboBox *dataOptionCombo;
    QComboBox *summaryThresholdCombo;
    QComboBox *notificationDelayCombo;
    QComboBox *trayUnreadLimitCombo;
    QCheckBox *autostartCheckBox;
    QCheckBox *startMinimizedCheckBox;
    QCheckBox *notifyOnceCheckBox;
    QCheckBox *notifyReadCheckBox;
    QPushButton *testButton;
    QLabel *statusLabel;
    GitHubClient *testClient;
};

#endif  // SETTINGSDIALOG_H
