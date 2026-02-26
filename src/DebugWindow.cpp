#include "DebugWindow.h"
#include <QVBoxLayout>
#include <QLabel>

DebugWindow::DebugWindow(GitHubClient *client, QWidget *parent)
    : QDialog(parent), m_client(client)
{
    setWindowTitle(tr("Debug GitHub API"));
    resize(600, 400);

    QVBoxLayout *layout = new QVBoxLayout(this);

    layout->addWidget(new QLabel(tr("API Endpoint (e.g. /notifications):")));
    m_endpointInput = new QLineEdit(this);
    layout->addWidget(m_endpointInput);

    m_sendButton = new QPushButton(tr("Send Request"), this);
    connect(m_sendButton, &QPushButton::clicked, this, &DebugWindow::sendRequest);
    layout->addWidget(m_sendButton);

    layout->addWidget(new QLabel(tr("Response:")));
    m_responseOutput = new QTextEdit(this);
    m_responseOutput->setReadOnly(true);
    layout->addWidget(m_responseOutput);

    connect(m_client, &GitHubClient::rawDataReceived, this, &DebugWindow::displayResponse);
}

void DebugWindow::sendRequest() {
    QString endpoint = m_endpointInput->text();
    if (endpoint.isEmpty()) return;

    m_responseOutput->setText(tr("Loading..."));
    m_client->requestRaw(endpoint);
}

void DebugWindow::displayResponse(const QByteArray &data) {
    // Only update if we are visible, or maybe checking if we are the active request?
    // For simple debug window, just updating is fine.
    m_responseOutput->setText(QString::fromUtf8(data));
}
