#include "TrendingWindow.h"
#include "../GitHubClient.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDesktopServices>
#include <QUrl>
#include <QMenu>
#include <QAction>
#include <QLabel>
#include <QIcon>
#include <QStyle>
#include <QApplication>
#include <QClipboard>

TrendingWindow::TrendingWindow(GitHubClient *client, QWidget *parent)
    : QWidget(parent, Qt::Window), m_client(client) {
    setWindowTitle(tr("Trending Repos & Devs"));
    resize(600, 400);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    QHBoxLayout *topLayout = new QHBoxLayout();

    modeComboBox = new QComboBox(this);
    modeComboBox->addItem(tr("Repositories"));
    modeComboBox->addItem(tr("Developers"));

    timeframeComboBox = new QComboBox(this);
    timeframeComboBox->addItem(tr("Today"));
    timeframeComboBox->addItem(tr("This Week"));
    timeframeComboBox->addItem(tr("This Month"));

    refreshButton = new QPushButton(tr("Refresh"), this);

    topLayout->addWidget(new QLabel(tr("Mode:")));
    topLayout->addWidget(modeComboBox);
    topLayout->addWidget(new QLabel(tr("Timeframe:")));
    topLayout->addWidget(timeframeComboBox);
    topLayout->addStretch();
    topLayout->addWidget(refreshButton);

    listWidget = new QListWidget(this);
    listWidget->setWordWrap(true);
    listWidget->setContextMenuPolicy(Qt::CustomContextMenu);

    mainLayout->addLayout(topLayout);
    mainLayout->addWidget(listWidget);

    connect(refreshButton, &QPushButton::clicked, this, &TrendingWindow::onRefreshClicked);
    connect(modeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &TrendingWindow::onModeChanged);
    connect(timeframeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &TrendingWindow::onRefreshClicked);
    connect(listWidget, &QListWidget::itemActivated, this, &TrendingWindow::onItemActivated);
    connect(listWidget, &QListWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        QListWidgetItem *item = listWidget->itemAt(pos);
        if (!item) return;

        QMenu menu(this);
        QAction *openAction = menu.addAction(tr("Open in Browser"));
        QAction *copyAction = menu.addAction(tr("Copy Link"));

        QAction *selectedAction = menu.exec(listWidget->mapToGlobal(pos));
        if (selectedAction == openAction) {
            onItemActivated(item);
        } else if (selectedAction == copyAction) {
            QString url = item->data(Qt::UserRole).toString();
            QApplication::clipboard()->setText(url);
        }
    });

    if (m_client) {
        connect(m_client, &GitHubClient::rawDataReceived, this, &TrendingWindow::onRawDataReceived);
    }

    // Initial fetch
    onRefreshClicked();
}

void TrendingWindow::onModeChanged(int) {
    onRefreshClicked();
}

void TrendingWindow::onRefreshClicked() {
    if (!m_client) return;

    listWidget->clear();
    listWidget->addItem(tr("Loading..."));

    int daysToSubtract = 1;
    switch (timeframeComboBox->currentIndex()) {
        case 0: daysToSubtract = 1; break;  // Today
        case 1: daysToSubtract = 7; break;  // This Week
        case 2: daysToSubtract = 30; break; // This Month
    }

    QDateTime date = QDateTime::currentDateTime().addDays(-daysToSubtract);
    QString dateStr = date.toString("yyyy-MM-dd");

    QString endpoint;
    if (modeComboBox->currentIndex() == 0) {
        // Repositories
        endpoint = QString("/search/repositories?q=created:>%1&sort=stars&order=desc").arg(dateStr);
    } else {
        // Developers
        endpoint = QString("/search/users?q=created:>%1&sort=followers&order=desc").arg(dateStr);
    }

    lastRequestedUrl = endpoint;
    m_client->requestRaw(endpoint);
}

void TrendingWindow::onRawDataReceived(const QByteArray &data) {
    // Only process if it looks like a search response. We use lastRequestedUrl to match loosely if possible.
    // In our client, requestRaw doesn't pass back the endpoint it requested.
    // So we'll try to parse and check if it's the right format.

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);

    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        return; // Ignore invalid JSON (might be for something else)
    }

    QJsonObject root = doc.object();
    if (!root.contains("items")) {
        return; // Not a search result
    }

    // Clear the loading text
    listWidget->clear();

    QJsonArray items = root["items"].toArray();
    for (int i = 0; i < items.size(); ++i) {
        QJsonObject itemObj = items[i].toObject();
        QString htmlUrl = itemObj["html_url"].toString();

        QListWidgetItem *item = new QListWidgetItem();

        if (modeComboBox->currentIndex() == 0) {
            // Repositories
            QString name = itemObj["full_name"].toString();
            QString desc = itemObj["description"].toString();
            int stars = itemObj["stargazers_count"].toInt();
            QString lang = itemObj["language"].toString();

            QString text = QString("%1\n★ %2 | %3\n%4").arg(name).arg(stars).arg(lang).arg(desc);
            item->setText(text);
        } else {
            // Developers
            QString login = itemObj["login"].toString();
            item->setText(login);
        }

        item->setData(Qt::UserRole, htmlUrl);
        listWidget->addItem(item);
    }

    if (items.isEmpty()) {
        listWidget->addItem(tr("No results found."));
    }
}

void TrendingWindow::onItemActivated(QListWidgetItem *item) {
    if (!item) return;
    QString url = item->data(Qt::UserRole).toString();
    if (!url.isEmpty()) {
        QDesktopServices::openUrl(QUrl(url));
    }
}
