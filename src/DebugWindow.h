#ifndef DEBUGWINDOW_H
#define DEBUGWINDOW_H

#include <QDialog>
#include <QLineEdit>
#include <QTextEdit>
#include <QPushButton>
#include <QComboBox>
#include <QFormLayout>
#include <QMap>
#include <QList>
#include <QLabel>
#include "GitHubClient.h"

struct ApiPreset {
    QString name;
    QString method;
    QString endpoint;
    QStringList params;
};

class DebugWindow : public QDialog {
    Q_OBJECT
public:
    explicit DebugWindow(GitHubClient *client, QWidget *parent = nullptr);

private slots:
    void sendRequest();
    void displayResponse(const QByteArray &data);
    void onApiSelected(int index);
    void onParamChanged();

private:
    GitHubClient *m_client;

    QComboBox *m_apiSelector;
    QComboBox *m_methodSelector;
    QWidget *m_paramsContainer;
    QFormLayout *m_paramsLayout;

    QLineEdit *m_endpointInput;
    QTextEdit *m_bodyInput;
    QTextEdit *m_responseOutput;
    QPushButton *m_sendButton;

    QList<ApiPreset> m_presets;
};

#endif // DEBUGWINDOW_H
