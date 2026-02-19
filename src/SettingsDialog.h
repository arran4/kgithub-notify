#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QLineEdit>

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    static QString getToken();

private slots:
    void saveSettings();

private:
    QLineEdit *tokenEdit;
};

#endif // SETTINGSDIALOG_H
