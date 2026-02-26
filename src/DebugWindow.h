#ifndef DEBUGWINDOW_H
#define DEBUGWINDOW_H

#include <QDialog>
#include <QLineEdit>
#include <QTextEdit>
#include <QPushButton>
#include "GitHubClient.h"

class DebugWindow : public QDialog {
    Q_OBJECT
public:
    explicit DebugWindow(GitHubClient *client, QWidget *parent = nullptr);

private slots:
    void sendRequest();
    void displayResponse(const QByteArray &data);

private:
    GitHubClient *m_client;
    QLineEdit *m_endpointInput;
    QTextEdit *m_responseOutput;
    QPushButton *m_sendButton;
};

#endif // DEBUGWINDOW_H
