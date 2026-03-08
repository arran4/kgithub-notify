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
#include <QHeaderView>
#include <QTableWidget>
#include <QMenuBar>
#include <QTextEdit>
#include <QDialog>

TrendingWindow::TrendingWindow(GitHubClient *client, QWidget *parent)
    : QMainWindow(parent, Qt::Window), m_client(client) {
    setWindowTitle(tr("Trending Repos & Devs"));
    resize(800, 600); // make it a bit larger to fit columns

    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

    QHBoxLayout *topLayout = new QHBoxLayout();

    modeComboBox = new QComboBox(this);
    modeComboBox->addItem(tr("Repositories"));
    modeComboBox->addItem(tr("Developers"));

    timeframeComboBox = new QComboBox(this);
    timeframeComboBox->addItem(tr("Today"));
    timeframeComboBox->addItem(tr("This Week"));
    timeframeComboBox->addItem(tr("This Month"));

    langComboBox = new QComboBox(this);
    langComboBox->addItem(tr("All Languages"), "");
    langComboBox->addItem(tr("C++"), "c++");
    langComboBox->addItem(tr("Python"), "python");
    langComboBox->addItem(tr("JavaScript"), "javascript");
    langComboBox->addItem(tr("Java"), "java");
    langComboBox->addItem(tr("Go"), "go");
    langComboBox->addItem(tr("Rust"), "rust");
    langComboBox->addItem(tr("Ruby"), "ruby");

    spokenLangComboBox = new QComboBox(this);
    spokenLangComboBox->addItem(tr("Any Spoken"), "");
    spokenLangComboBox->addItem(tr("English"), "en");
    spokenLangComboBox->addItem(tr("Chinese"), "zh");
    spokenLangComboBox->addItem(tr("Spanish"), "es");
    spokenLangComboBox->addItem(tr("French"), "fr");
    spokenLangComboBox->addItem(tr("German"), "de");
    spokenLangComboBox->addItem(tr("Japanese"), "ja");

    refreshButton = new QPushButton(tr("Refresh"), this);

    topLayout->addWidget(new QLabel(tr("Mode:")));
    topLayout->addWidget(modeComboBox);
    topLayout->addWidget(new QLabel(tr("Timeframe:")));
    topLayout->addWidget(timeframeComboBox);
    topLayout->addWidget(new QLabel(tr("Lang:")));
    topLayout->addWidget(langComboBox);
    topLayout->addWidget(new QLabel(tr("Spoken:")));
    topLayout->addWidget(spokenLangComboBox);
    topLayout->addStretch();
    topLayout->addWidget(refreshButton);

    tableWidget = new QTableWidget(this);
    tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tableWidget->setWordWrap(true);
    tableWidget->verticalHeader()->hide();
    tableWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    tableWidget->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);

    mainLayout->addLayout(topLayout);
    mainLayout->addWidget(tableWidget);

    QMenuBar *menuBarWidget = menuBar();
    QMenu *fileMenu = menuBarWidget->addMenu(tr("&File"));
    QAction *closeAction = new QAction(QIcon::fromTheme("window-close"), tr("Close"), this);
    closeAction->setShortcut(QKeySequence::Close);
    connect(closeAction, &QAction::triggered, this, &TrendingWindow::close);
    fileMenu->addAction(closeAction);

    QMenu *editMenu = menuBarWidget->addMenu(tr("&Edit"));
    QAction *copyAction = new QAction(QIcon::fromTheme("edit-copy"), tr("Copy Link"), this);
    copyAction->setShortcut(QKeySequence::Copy);
    connect(copyAction, &QAction::triggered, this, [this]() {
        QList<QTableWidgetItem *> items = tableWidget->selectedItems();
        if (!items.isEmpty()) {
            QTableWidgetItem *item = items.first();
            if (item) {
                QString url = item->data(Qt::UserRole).toString();
                if (!url.isEmpty()) {
                    QApplication::clipboard()->setText(url);
                }
            }
        }
    });
    editMenu->addAction(copyAction);

    QMenu *viewMenu = menuBarWidget->addMenu(tr("&View"));
    QAction *refreshAction = new QAction(QIcon::fromTheme("view-refresh"), tr("Refresh"), this);
    refreshAction->setShortcut(QKeySequence::Refresh);
    connect(refreshAction, &QAction::triggered, this, &TrendingWindow::onRefreshClicked);
    viewMenu->addAction(refreshAction);

    connect(refreshButton, &QPushButton::clicked, this, &TrendingWindow::onRefreshClicked);
    connect(modeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &TrendingWindow::onModeChanged);
    connect(timeframeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &TrendingWindow::onRefreshClicked);
    connect(langComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &TrendingWindow::onRefreshClicked);
    connect(spokenLangComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &TrendingWindow::onRefreshClicked);
    connect(tableWidget, &QTableWidget::itemActivated, this, &TrendingWindow::onItemActivated);
    connect(tableWidget, &QTableWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        QTableWidgetItem *item = tableWidget->itemAt(pos);
        if (!item) return;

        QMenu menu(this);
        QAction *openAction = menu.addAction(tr("Open in Browser"));
        QAction *copyAction = menu.addAction(tr("Copy Link"));
        QAction *viewRawAction = menu.addAction(tr("View Raw JSON"));

        QAction *selectedAction = menu.exec(tableWidget->mapToGlobal(pos));
        if (selectedAction == openAction) {
            onItemActivated(item);
        } else if (selectedAction == copyAction) {
            QString url = item->data(Qt::UserRole).toString();
            QApplication::clipboard()->setText(url);
        } else if (selectedAction == viewRawAction) {
            QString rawJson = item->data(Qt::UserRole + 1).toString();
            // Display raw JSON in a simple widget or dialog
            if (!rawJson.isEmpty()) {
                QDialog *dialog = new QDialog(this);
                dialog->setWindowTitle(tr("Raw JSON"));
                dialog->resize(400, 300);
                QVBoxLayout *layout = new QVBoxLayout(dialog);
                QTextEdit *textEdit = new QTextEdit(dialog);
                textEdit->setReadOnly(true);
                textEdit->setPlainText(rawJson);
                layout->addWidget(textEdit);
                dialog->setAttribute(Qt::WA_DeleteOnClose);
                dialog->show();
            }
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

    tableWidget->clear();
    tableWidget->setRowCount(1);
    tableWidget->setColumnCount(1);
    tableWidget->setItem(0, 0, new QTableWidgetItem(tr("Loading...")));

    int daysToSubtract = 1;
    switch (timeframeComboBox->currentIndex()) {
        case 0: daysToSubtract = 1; break;  // Today
        case 1: daysToSubtract = 7; break;  // This Week
        case 2: daysToSubtract = 30; break; // This Month
    }

    QDateTime date = QDateTime::currentDateTime().addDays(-daysToSubtract);
    QString dateStr = date.toString("yyyy-MM-dd");

    QString endpoint;
    QString langFilter = langComboBox->currentData().toString();
    QString spokenLangFilter = spokenLangComboBox->currentData().toString();

    QString langQuery = "";
    if (!langFilter.isEmpty()) {
        langQuery += "+language:" + QUrl::toPercentEncoding(langFilter);
    }
    if (!spokenLangFilter.isEmpty()) {
        // Since GitHub API doesn't support searching by spoken_language_code directly via API,
        // we'll filter by language using the standard text search match or a general language filter fallback.
        // E.g., appending it as language:en or standard text to approximate it.
        // Actually it seems adding it directly to query text works better or falling back to language.
        langQuery += "+language:" + QUrl::toPercentEncoding(spokenLangFilter);
    }

    if (modeComboBox->currentIndex() == 0) {
        // Repositories
        endpoint = QString("/search/repositories?q=created:>%1%2&sort=stars&order=desc").arg(dateStr).arg(langQuery);
    } else {
        // Developers
        endpoint = QString("/search/users?q=created:>%1%2&sort=followers&order=desc").arg(dateStr).arg(langQuery);
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
    tableWidget->clear();
    tableWidget->setRowCount(0);

    QJsonArray items = root["items"].toArray();

    if (items.isEmpty()) {
        tableWidget->setColumnCount(1);
        tableWidget->insertRow(0);
        tableWidget->setItem(0, 0, new QTableWidgetItem(tr("No results found.")));
        return;
    }

    if (modeComboBox->currentIndex() == 0) {
        // Repositories
        tableWidget->setColumnCount(5);
        tableWidget->setHorizontalHeaderLabels({tr("Name"), tr("Stars"), tr("Language"), tr("Description"), tr("URL")});

        tableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents); // Name
        tableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents); // Stars
        tableWidget->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents); // Language
        tableWidget->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Interactive);      // Description gets calculated width
        tableWidget->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents); // URL
    } else {
        // Developers
        tableWidget->setColumnCount(2);
        tableWidget->setHorizontalHeaderLabels({tr("Developer"), tr("URL")});
        tableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents); // Developer
        tableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents); // URL
    }

    for (int i = 0; i < items.size(); ++i) {
        QJsonObject itemObj = items[i].toObject();
        QString htmlUrl = itemObj["html_url"].toString();

        // Pretty print raw json for debug action
        QJsonDocument docObj(itemObj);
        QString rawJson = QString::fromUtf8(docObj.toJson(QJsonDocument::Indented));

        tableWidget->insertRow(i);

        if (modeComboBox->currentIndex() == 0) {
            // Repositories
            QString name = itemObj["full_name"].toString();
            QString desc = itemObj["description"].toString();
            QString stars = QString::number(itemObj["stargazers_count"].toInt());
            QString lang = itemObj["language"].toString();

            QTableWidgetItem *nameItem = new QTableWidgetItem(name);
            nameItem->setData(Qt::UserRole, htmlUrl);
            nameItem->setData(Qt::UserRole + 1, rawJson);
            QTableWidgetItem *starsItem = new QTableWidgetItem(stars);
            starsItem->setData(Qt::UserRole, htmlUrl);
            starsItem->setData(Qt::UserRole + 1, rawJson);
            QTableWidgetItem *langItem = new QTableWidgetItem(lang);
            langItem->setData(Qt::UserRole, htmlUrl);
            langItem->setData(Qt::UserRole + 1, rawJson);
            QTableWidgetItem *descItem = new QTableWidgetItem(desc);
            descItem->setData(Qt::UserRole, htmlUrl);
            descItem->setData(Qt::UserRole + 1, rawJson);

            QLabel *linkLabel = new QLabel(QString("<a href='%1'>%1</a>").arg(htmlUrl));
            linkLabel->setOpenExternalLinks(true);
            QTableWidgetItem *urlItem = new QTableWidgetItem(); // Empty item to hold the widget's place and data
            urlItem->setData(Qt::UserRole, htmlUrl);
            urlItem->setData(Qt::UserRole + 1, rawJson);

            tableWidget->setItem(i, 0, nameItem);
            tableWidget->setItem(i, 1, starsItem);
            tableWidget->setItem(i, 2, langItem);
            tableWidget->setItem(i, 3, descItem);
            tableWidget->setItem(i, 4, urlItem);
            tableWidget->setCellWidget(i, 4, linkLabel);
        } else {
            // Developers
            QString login = itemObj["login"].toString();

            QTableWidgetItem *loginItem = new QTableWidgetItem(login);
            loginItem->setData(Qt::UserRole, htmlUrl);
            loginItem->setData(Qt::UserRole + 1, rawJson);

            QLabel *linkLabel = new QLabel(QString("<a href='%1'>%1</a>").arg(htmlUrl));
            linkLabel->setOpenExternalLinks(true);
            QTableWidgetItem *urlItem = new QTableWidgetItem();
            urlItem->setData(Qt::UserRole, htmlUrl);
            urlItem->setData(Qt::UserRole + 1, rawJson);

            tableWidget->setItem(i, 0, loginItem);
            tableWidget->setItem(i, 1, urlItem);
            tableWidget->setCellWidget(i, 1, linkLabel);
        }
    }

    // Force table to wrap words and calculate correct row heights for descriptions
    tableWidget->setWordWrap(true);

    // Explicitly set the width of the description column to be constrained
    // It should have a minimum size, and a maximum size (no bigger than the viewport).
    if (modeComboBox->currentIndex() == 0) {
        int viewportWidth = tableWidget->viewport()->width();
        int minDescWidth = 300;
        int maxDescWidth = viewportWidth > 0 ? viewportWidth : 800;
        // Try to give it 40% of the viewport width initially, bounded by min and max
        int targetWidth = qBound(minDescWidth, (int)(viewportWidth * 0.4), maxDescWidth);
        tableWidget->setColumnWidth(3, targetWidth);
    }

    tableWidget->resizeRowsToContents();
}

void TrendingWindow::onItemActivated(QTableWidgetItem *item) {
    if (!item) return;
    QString url = item->data(Qt::UserRole).toString();
    if (!url.isEmpty()) {
        QDesktopServices::openUrl(QUrl(url));
    }
}
