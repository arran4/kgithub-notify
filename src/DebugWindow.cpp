#include "DebugWindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QScrollArea>
#include <QJsonDocument>

DebugWindow::DebugWindow(GitHubClient *client, QWidget *parent)
    : QDialog(parent), m_client(client)
{
    setWindowTitle(tr("Debug GitHub API"));
    resize(700, 600);

    // Initialize Presets
    m_presets.append({"Custom Request", "GET", "", {}});
    m_presets.append({"User Profile", "GET", "/user", {}});
    m_presets.append({"All Notifications", "GET", "/notifications?all=true", {}});
    m_presets.append({"Repo Notifications", "GET", "/repos/{owner}/{repo}/notifications", {"owner", "repo"}});
    m_presets.append({"Mark Thread Read", "PATCH", "/notifications/threads/{thread_id}", {"thread_id"}});
    m_presets.append({"List Issues", "GET", "/repos/{owner}/{repo}/issues", {"owner", "repo"}});
    m_presets.append({"Rate Limit", "GET", "/rate_limit", {}});

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // API Selection
    QHBoxLayout *topLayout = new QHBoxLayout();
    topLayout->addWidget(new QLabel(tr("API Action:")));
    m_apiSelector = new QComboBox(this);
    for (const auto &preset : m_presets) {
        m_apiSelector->addItem(preset.name);
    }
    // Use the Qt 5 connect syntax for overloaded signals if needed, or function pointer
    // connect(m_apiSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DebugWindow::onApiSelected);
    // But basic SIGNAL/SLOT is safer if not sure about overload support in this env.
    // QComboBox::currentIndexChanged(int)
    connect(m_apiSelector, SIGNAL(currentIndexChanged(int)), this, SLOT(onApiSelected(int)));
    topLayout->addWidget(m_apiSelector);

    topLayout->addWidget(new QLabel(tr("Method:")));
    m_methodSelector = new QComboBox(this);
    m_methodSelector->addItems({"GET", "POST", "PUT", "PATCH", "DELETE"});
    topLayout->addWidget(m_methodSelector);

    mainLayout->addLayout(topLayout);

    // Dynamic Parameters
    m_paramsContainer = new QWidget(this);
    m_paramsLayout = new QFormLayout(m_paramsContainer);
    mainLayout->addWidget(m_paramsContainer);

    // Endpoint
    mainLayout->addWidget(new QLabel(tr("Endpoint (e.g. /notifications):")));
    m_endpointInput = new QLineEdit(this);
    mainLayout->addWidget(m_endpointInput);

    // Body
    mainLayout->addWidget(new QLabel(tr("Body (JSON):")));
    m_bodyInput = new QTextEdit(this);
    m_bodyInput->setMaximumHeight(100);
    mainLayout->addWidget(m_bodyInput);

    // Send Button
    m_sendButton = new QPushButton(tr("Send Request"), this);
    connect(m_sendButton, &QPushButton::clicked, this, &DebugWindow::sendRequest);
    mainLayout->addWidget(m_sendButton);

    // Response
    mainLayout->addWidget(new QLabel(tr("Response:")));
    m_responseOutput = new QTextEdit(this);
    m_responseOutput->setReadOnly(true);
    mainLayout->addWidget(m_responseOutput);

    connect(m_client, &GitHubClient::rawDataReceived, this, &DebugWindow::displayResponse);

    // Trigger initial selection
    onApiSelected(0);
}

void DebugWindow::onApiSelected(int index) {
    if (index < 0 || index >= m_presets.size()) return;

    const ApiPreset &preset = m_presets[index];

    // Update Method
    int methodIndex = m_methodSelector->findText(preset.method);
    if (methodIndex != -1) {
        m_methodSelector->setCurrentIndex(methodIndex);
    }

    // Update Endpoint
    m_endpointInput->setText(preset.endpoint);

    // Clear existing params
    QLayoutItem *child;
    while ((child = m_paramsLayout->takeAt(0)) != nullptr) {
        if (child->widget()) {
            child->widget()->deleteLater();
        }
        delete child;
    }

    // Generate new params
    for (const QString &param : preset.params) {
        QLineEdit *input = new QLineEdit(this);
        // Connect changes to update endpoint
        connect(input, &QLineEdit::textChanged, this, &DebugWindow::onParamChanged);
        m_paramsLayout->addRow(param + ":", input);
    }
}

void DebugWindow::onParamChanged() {
    int index = m_apiSelector->currentIndex();
    if (index < 0 || index >= m_presets.size()) return;

    const ApiPreset &preset = m_presets[index];
    QString endpoint = preset.endpoint;

    // Iterate through params layout
    for (int i = 0; i < preset.params.size(); ++i) {
        QString param = preset.params[i];
        // itemAt(row, role)
        // QFormLayout rows are indexed 0..count
        // But adding rows might interleave? No, addRow appends.
        QLayoutItem *item = m_paramsLayout->itemAt(i, QFormLayout::FieldRole);
        if (item && item->widget()) {
            QLineEdit *input = qobject_cast<QLineEdit*>(item->widget());
            if (input) {
                QString value = input->text();
                endpoint.replace("{" + param + "}", value);
            }
        }
    }

    m_endpointInput->setText(endpoint);
}

void DebugWindow::sendRequest() {
    QString endpoint = m_endpointInput->text();
    if (endpoint.isEmpty()) return;

    QString method = m_methodSelector->currentText();
    QByteArray body = m_bodyInput->toPlainText().toUtf8();

    m_responseOutput->setText(tr("Loading..."));
    m_client->requestRaw(endpoint, method, body);
}

void DebugWindow::displayResponse(const QByteArray &data) {
    // Only update if we are visible
    // Check if JSON and format it?
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isNull()) {
        m_responseOutput->setText(QString::fromUtf8(doc.toJson(QJsonDocument::Indented)));
    } else {
        m_responseOutput->setText(QString::fromUtf8(data));
    }
}
