#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QLineEdit>

class QComboBox;
class QPushButton;
class QLabel;
class GitHubClient;

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    static QString getToken();
    static int getInterval();

private slots:
    void saveSettings();
    void onTestClicked();
    void onVerificationResult(bool valid, const QString &message);

private:
    QLineEdit *tokenEdit;
    QComboBox *intervalCombo;
    QPushButton *testButton;
    QLabel *statusLabel;
    GitHubClient *testClient;
};

#endif // SETTINGSDIALOG_H
