#include "WorkItemWindow.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QHeaderView>
#include <QMenu>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QUrl>
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QDateTime>

WorkItemWindow::WorkItemWindow(GitHubClient *client, const QString& windowTitle, EndpointType endpointType, const QString& baseQuery, QWidget *parent)
    : KXmlGuiWindow(parent), m_client(client), m_windowTitle(windowTitle), m_endpointType(endpointType), m_baseQuery(baseQuery), m_currentPage(1), m_manager(new QNetworkAccessManager(this))
{
    setupUi();
    connect(m_manager, &QNetworkAccessManager::finished, this, &WorkItemWindow::onReplyFinished);
    loadCache();
    loadData(1);
}

WorkItemWindow::~WorkItemWindow()
{
}

void WorkItemWindow::setupUi()
{
    setWindowTitle(m_windowTitle);
    resize(800, 600);

    // Table
    m_table = new QTableWidget(this);
    m_table->setColumnCount(5);
    if (m_endpointType == EndpointIssues) {
        m_table->setHorizontalHeaderLabels({tr("Repository"), tr("Title"), tr("State"), tr("Author"), tr("Created At")});
    } else {
        m_table->setHorizontalHeaderLabels({tr("Repository Name"), tr("Description"), tr("Language"), tr("Owner"), tr("Created At")});
    }
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_table, &QTableWidget::customContextMenuRequested, this, &WorkItemWindow::onCustomContextMenuRequested);
    connect(m_table, &QTableWidget::itemDoubleClicked, this, &WorkItemWindow::onItemDoubleClicked);

    setupGUI(Default, "kgithub-notifyui.rc");
    setCentralWidget(m_table);

    // Actions
    QAction *exportCsvAction = new QAction(tr("Export to CSV"), this);
    connect(exportCsvAction, &QAction::triggered, this, &WorkItemWindow::exportToCsv);
    QAction *exportJsonAction = new QAction(tr("Export to JSON"), this);
    connect(exportJsonAction, &QAction::triggered, this, &WorkItemWindow::exportToJson);
    QAction *refreshAction = new QAction(tr("Refresh"), this);
    connect(refreshAction, &QAction::triggered, this, [this]() {
        m_table->setRowCount(0); // Give immediate visual feedback of refresh
        loadData(1);
    });
    QAction *closeAction = new QAction(QIcon::fromTheme("window-close"), tr("Close"), this);
    closeAction->setShortcut(QKeySequence::Close);
    connect(closeAction, &QAction::triggered, this, &WorkItemWindow::close);

    // Menu Bar
    QMenuBar *menuBarWidget = menuBar();
    QMenu *fileMenu = menuBarWidget->addMenu(tr("&File"));
    fileMenu->addAction(exportCsvAction);
    fileMenu->addAction(exportJsonAction);
    fileMenu->addSeparator();
    fileMenu->addAction(closeAction);

    QMenu *editMenu = menuBarWidget->addMenu(tr("&Edit"));
    m_copyAction = new QAction(QIcon::fromTheme("edit-copy"), tr("Copy Link"), this);
    m_copyAction->setShortcut(QKeySequence::Copy);
    connect(m_copyAction, &QAction::triggered, this, &WorkItemWindow::copyLink);
    editMenu->addAction(m_copyAction);

    QMenu *viewMenu = menuBarWidget->addMenu(tr("&View"));
    refreshAction->setShortcut(QKeySequence::Refresh);
    viewMenu->addAction(refreshAction);

    // Tool Bar
    QToolBar *toolBar = addToolBar(tr("Main Toolbar"));
    toolBar->addAction(refreshAction);
    toolBar->addSeparator();
    toolBar->addAction(exportCsvAction);
    toolBar->addAction(exportJsonAction);

    // Status Bar
    m_statusLabel = new QLabel(this);
    statusBar()->addWidget(m_statusLabel);

    // Context Menu Actions
    m_openAction = new QAction(QIcon::fromTheme("internet-web-browser"), tr("Open in Browser"), this);
    connect(m_openAction, &QAction::triggered, this, &WorkItemWindow::openInBrowser);

}

QString WorkItemWindow::getCacheFilePath() const
{
    QString hash = QString::number(qHash(m_baseQuery), 16);
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/workitems_" + hash + ".json";
}

void WorkItemWindow::loadCache()
{
    QFile file(getCacheFilePath());
    if (file.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        QJsonObject obj = doc.object();
        m_allData = obj["items"].toArray();
        QString lastRefresh = obj["lastRefresh"].toString();

        m_table->setRowCount(0);
        for (int i = 0; i < m_allData.size(); ++i) {
            appendRow(m_allData[i].toObject());
        }
        m_statusLabel->setText(tr("Cached data from %1 - Items: %2").arg(lastRefresh).arg(m_allData.size()));
    }
}

void WorkItemWindow::saveCache()
{
    QFile file(getCacheFilePath());
    if (file.open(QIODevice::WriteOnly)) {
        QJsonObject obj;
        obj["items"] = m_allData;
        obj["lastRefresh"] = QDateTime::currentDateTime().toString();
        QJsonDocument doc(obj);
        file.write(doc.toJson());
    }
}

void WorkItemWindow::loadData(int page)
{
    m_currentPage = page;
    if (page == 1) {
        m_statusLabel->setText(tr("Refreshing data..."));
    }

    QString endpointStr = (m_endpointType == EndpointIssues) ? "issues" : "repositories";
    QUrl url("https://api.github.com/search/" + endpointStr + "?q=" + QUrl::toPercentEncoding(m_baseQuery) + "&per_page=100&page=" + QString::number(m_currentPage));

    QNetworkRequest request = m_client->createAuthenticatedRequest(url);
    m_manager->get(request);
}

void WorkItemWindow::onReplyFinished(QNetworkReply *reply)
{
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonObject obj = doc.object();
        QJsonArray items = obj["items"].toArray();
        int totalCount = obj["total_count"].toInt();

        if (m_currentPage == 1) {
            m_allData = QJsonArray();
            m_table->setRowCount(0);
        }

        for (int i = 0; i < items.size(); ++i) {
            m_allData.append(items[i]);
            appendRow(items[i].toObject());
        }

        if (items.size() > 0 && m_allData.size() < totalCount && m_allData.size() < 1000) {
            int maxPages = (qMin(totalCount, 1000) + 99) / 100;
            m_statusLabel->setText(tr("Loading page %1 / %2... (Total: %3)").arg(m_currentPage + 1).arg(maxPages).arg(totalCount));
            loadData(m_currentPage + 1);
        } else {
            saveCache();
            int maxPages = (qMin(totalCount, 1000) + 99) / 100;
            QString limitMsg = (totalCount > 1000) ? tr(" (GitHub Search Limit Reached)") : "";
            m_statusLabel->setText(tr("Items: %1%2 | Pages loaded: %3 / %4 | Last refresh: %5")
                .arg(m_allData.size())
                .arg(limitMsg)
                .arg(m_currentPage)
                .arg(maxPages)
                .arg(QDateTime::currentDateTime().toString()));
        }
    } else {
        QMessageBox::warning(this, tr("Error"), tr("Failed to fetch data: %1").arg(reply->errorString()));
        m_statusLabel->setText(tr("Error fetching data."));
    }
    reply->deleteLater();
}

void WorkItemWindow::appendRow(const QJsonObject &item)
{
    int row = m_table->rowCount();
    m_table->insertRow(row);

    if (m_endpointType == EndpointIssues) {
        QString htmlUrl = item["html_url"].toString();
        QString title = item["title"].toString();
        QString state = item["state"].toString();
        QString createdAt = item["created_at"].toString();

        QJsonObject user = item["user"].toObject();
        QString author = user["login"].toString();

        // Extract repository from repository_url
        QString repoUrl = item["repository_url"].toString();
        QString repo = repoUrl.section('/', -2); // Gets owner/repo

        QTableWidgetItem *repoItem = new QTableWidgetItem(repo);
        QTableWidgetItem *titleItem = new QTableWidgetItem(title);
        QTableWidgetItem *stateItem = new QTableWidgetItem(state);
        QTableWidgetItem *authorItem = new QTableWidgetItem(author);
        QTableWidgetItem *createdItem = new QTableWidgetItem(createdAt);

        // Store the URL in the title item for easy access later
        titleItem->setData(Qt::UserRole, htmlUrl);

        m_table->setItem(row, 0, repoItem);
        m_table->setItem(row, 1, titleItem);
        m_table->setItem(row, 2, stateItem);
        m_table->setItem(row, 3, authorItem);
        m_table->setItem(row, 4, createdItem);
    } else {
        QString htmlUrl = item["html_url"].toString();
        QString fullName = item["full_name"].toString();
        QString description = item["description"].toString();
        QString language = item["language"].toString();
        QString owner = item["owner"].toObject()["login"].toString();
        QString createdAt = item["created_at"].toString();

        QTableWidgetItem *repoItem = new QTableWidgetItem(fullName);
        QTableWidgetItem *descItem = new QTableWidgetItem(description);
        QTableWidgetItem *langItem = new QTableWidgetItem(language);
        QTableWidgetItem *ownerItem = new QTableWidgetItem(owner);
        QTableWidgetItem *createdItem = new QTableWidgetItem(createdAt);

        // Store the URL in the repo item
        repoItem->setData(Qt::UserRole, htmlUrl);

        m_table->setItem(row, 0, repoItem);
        m_table->setItem(row, 1, descItem);
        m_table->setItem(row, 2, langItem);
        m_table->setItem(row, 3, ownerItem);
        m_table->setItem(row, 4, createdItem);
    }
}

void WorkItemWindow::exportToCsv()
{
    QString fileName = QFileDialog::getSaveFileName(this, tr("Export CSV"), "", tr("CSV Files (*.csv)"));
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Error"), tr("Could not open file for writing"));
        return;
    }

    QTextStream out(&file);

    // Write headers
    QStringList headers;
    for (int col = 0; col < m_table->columnCount(); ++col) {
        headers << QString("\"%1\"").arg(m_table->horizontalHeaderItem(col)->text());
    }
    headers << "\"URL\""; // Add URL to export
    out << headers.join(",") << "\n";

    // Write rows
    for (int row = 0; row < m_table->rowCount(); ++row) {
        QStringList rowData;
        for (int col = 0; col < m_table->columnCount(); ++col) {
            QTableWidgetItem *item = m_table->item(row, col);
            QString text = item ? item->text() : "";
            text.replace("\"", "\"\""); // Escape quotes
            rowData << QString("\"%1\"").arg(text);
        }
        QString url = getHtmlUrlForRow(row);
        rowData << QString("\"%1\"").arg(url);
        out << rowData.join(",") << "\n";
    }

    file.close();
}

void WorkItemWindow::exportToJson()
{
    QString fileName = QFileDialog::getSaveFileName(this, tr("Export JSON"), "", tr("JSON Files (*.json)"));
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Error"), tr("Could not open file for writing"));
        return;
    }

    QJsonArray jsonArray;

    for (int row = 0; row < m_table->rowCount(); ++row) {
        QJsonObject jsonObj;
        for (int col = 0; col < m_table->columnCount(); ++col) {
            QString header = m_table->horizontalHeaderItem(col)->text();
            QTableWidgetItem *item = m_table->item(row, col);
            jsonObj[header] = item ? item->text() : "";
        }
        jsonObj["URL"] = getHtmlUrlForRow(row);
        jsonArray.append(jsonObj);
    }

    QJsonDocument doc(jsonArray);
    file.write(doc.toJson());
    file.close();
}

void WorkItemWindow::onCustomContextMenuRequested(const QPoint &pos)
{
    QModelIndex index = m_table->indexAt(pos);
    if (!index.isValid()) return;

    QMenu menu(this);
    menu.addAction(m_openAction);
    menu.addAction(m_copyAction);
    menu.exec(m_table->viewport()->mapToGlobal(pos));
}

void WorkItemWindow::onItemDoubleClicked(QTableWidgetItem *item)
{
    if (item) {
        openInBrowser();
    }
}

void WorkItemWindow::openInBrowser()
{
    QModelIndexList selection = m_table->selectionModel()->selectedRows();
    if (selection.isEmpty()) return;

    int row = selection.first().row();
    QString url = getHtmlUrlForRow(row);
    if (!url.isEmpty()) {
        QDesktopServices::openUrl(QUrl(url));
    }
}

void WorkItemWindow::copyLink()
{
    QModelIndexList selection = m_table->selectionModel()->selectedRows();
    if (selection.isEmpty()) return;

    int row = selection.first().row();
    QString url = getHtmlUrlForRow(row);
    if (!url.isEmpty()) {
        QApplication::clipboard()->setText(url);
    }
}

QString WorkItemWindow::getHtmlUrlForRow(int row) const
{
    QTableWidgetItem *item = m_table->item(row, (m_endpointType == EndpointIssues) ? 1 : 0);
    if (item) {
        return item->data(Qt::UserRole).toString();
    }
    return QString();
}
