#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QFuture>

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

   private slots:
    void saveSettings();
    void onTestClicked();
    void onVerificationResult(bool valid, const QString &message);

   private:
    void updateAutostartEntry();
    bool isAutostartEnabled();

    QLineEdit *tokenEdit;
    QComboBox *intervalCombo;
    QComboBox *dataOptionCombo;
    QCheckBox *autostartCheckBox;
    QCheckBox *startMinimizedCheckBox;
    QPushButton *testButton;
    QLabel *statusLabel;
    GitHubClient *testClient;
};

#endif  // SETTINGSDIALOG_H
