// 1. Includes
#include "WorkItemWindow.h"

#include <KActionCollection>
#include <KStandardAction>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QDateTime>
#include <QDesktopServices>
#include <QFile>
#include <QFileDialog>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTextStream>
#include <QToolBar>
#include <QUrl>

#include "utils/UrlHelper.h"

// 2. Constants / Static Helpers

// 3. Constructor / Destructor
WorkItemWindow::WorkItemWindow(GitHubClient* client, const QString& windowTitle, EndpointType endpointType,
                               const QString& baseQuery, QWidget* parent, QNetworkAccessManager* manager)
    : KXmlGuiWindow(parent),
      m_client(client),
      m_windowTitle(windowTitle),
      m_endpointType(endpointType),
      m_baseQuery(baseQuery),
      m_manager(manager ? manager : new QNetworkAccessManager(this)),
      m_ownsManager(manager == nullptr),
      m_totalCount(0) {
    setupUi();
    connect(m_manager, &QNetworkAccessManager::finished, this, &WorkItemWindow::onReplyFinished);
    loadCache();
    startRefresh();
}

WorkItemWindow::~WorkItemWindow() {
    m_currentGenerationId = QUuid();
    if (m_manager) {
        m_manager->disconnect(this);
    }
    for (QNetworkReply* reply : m_inFlightReplies) {
        if (!reply) continue;
        reply->disconnect(this);
        reply->abort();
        reply->deleteLater();
    }
    m_inFlightReplies.clear();
}

// 4. Public Methods
void WorkItemWindow::startRefresh() {
    m_currentGenerationId = QUuid::createUuid();
    m_stagedPages.clear();
    m_inFlightPages.clear();
    m_totalCount = 0;

    m_statusLabel->setText(tr("Refreshing data..."));
    fetchPage(m_currentGenerationId, 1);
}

// 5. Protected Methods
void WorkItemWindow::closeEvent(QCloseEvent* event) {
    m_currentGenerationId = QUuid();
    for (QNetworkReply* reply : m_inFlightReplies) {
        if (!reply) continue;
        reply->disconnect(this);
        reply->abort();
        reply->deleteLater();
    }
    m_inFlightReplies.clear();
    KXmlGuiWindow::closeEvent(event);
}

// 6. Slots
void WorkItemWindow::onReplyFinished(QNetworkReply* reply) {
    if (!reply) return;

    m_inFlightReplies.remove(reply);

    QString genStr = reply->property("generationId").toString();
    QUuid replyGen = QUuid::fromString(genStr);
    int page = reply->property("page").toInt();

    // Check if reply belongs to active generation
    if (replyGen.isNull() || replyGen != m_currentGenerationId) {
        reply->deleteLater();
        return;
    }

    m_inFlightPages.remove(page);

    if (reply->error() != QNetworkReply::NoError) {
        m_stagedPages.clear();
        m_inFlightPages.clear();
        m_currentGenerationId = QUuid();
        m_statusLabel->setText(tr("Error fetching data."));
        if (isVisible()) {
            QMessageBox::warning(this, tr("Error"), tr("Failed to fetch data: %1").arg(reply->errorString()));
        }
        reply->deleteLater();
        return;
    }

    QByteArray data = reply->readAll();
    reply->deleteLater();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        m_stagedPages.clear();
        m_inFlightPages.clear();
        m_currentGenerationId = QUuid();
        m_statusLabel->setText(tr("Error fetching data."));
        return;
    }

    QJsonObject obj = doc.object();
    QJsonArray items = obj["items"].toArray();
    int totalCount = obj["total_count"].toInt();
    m_totalCount = totalCount;

    // Incorporate each page exactly once
    if (!m_stagedPages.contains(page)) {
        m_stagedPages.insert(page, items);
    }

    int stagedItemCount = 0;
    for (const QJsonArray& arr : m_stagedPages) {
        stagedItemCount += arr.size();
    }

    int maxSearchItems = qMin(totalCount, 1000);
    int maxPages = (maxSearchItems + 99) / 100;
    if (maxPages == 0) maxPages = 1;

    int nextPage = page + 1;
    if (items.size() > 0 && stagedItemCount < maxSearchItems && nextPage <= maxPages) {
        if (!m_stagedPages.contains(nextPage) && !m_inFlightPages.contains(nextPage)) {
            m_statusLabel->setText(
                tr("Loading page %1 / %2... (Total: %3)").arg(nextPage).arg(maxPages).arg(totalCount));
            fetchPage(m_currentGenerationId, nextPage);
        }
    }

    if (m_inFlightPages.isEmpty()) {
        commitStagedData(totalCount, maxPages);
    }
}

void WorkItemWindow::exportToCsv() {
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
    headers << "\"URL\"";
    out << headers.join(",") << "\n";

    // Write rows
    for (int row = 0; row < m_table->rowCount(); ++row) {
        QStringList rowData;
        for (int col = 0; col < m_table->columnCount(); ++col) {
            QTableWidgetItem* item = m_table->item(row, col);
            QString text = item ? item->text() : "";
            text.replace("\"", "\"\"");
            rowData << QString("\"%1\"").arg(text);
        }
        QString url = getHtmlUrlForRow(row);
        rowData << QString("\"%1\"").arg(url);
        out << rowData.join(",") << "\n";
    }

    file.close();
}

void WorkItemWindow::exportToJson() {
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
            QTableWidgetItem* item = m_table->item(row, col);
            jsonObj[header] = item ? item->text() : "";
        }
        jsonObj["URL"] = getHtmlUrlForRow(row);
        jsonArray.append(jsonObj);
    }

    QJsonDocument doc(jsonArray);
    file.write(doc.toJson());
    file.close();
}

void WorkItemWindow::onCustomContextMenuRequested(const QPoint& pos) {
    QMenu menu(this);
    menu.addAction(m_openAction);
    menu.addAction(m_copyAction);
    menu.exec(m_table->viewport()->mapToGlobal(pos));
}

void WorkItemWindow::onItemDoubleClicked(QTableWidgetItem* item) {
    if (!item) return;
    openInBrowser();
}

void WorkItemWindow::openInBrowser() {
    QModelIndexList selection = m_table->selectionModel()->selectedRows();
    if (selection.isEmpty()) return;

    int row = selection.first().row();
    QString url = getHtmlUrlForRow(row);
    if (!url.isEmpty()) {
        UrlHelper::openUrl(url);
    }
}

void WorkItemWindow::copyLink() {
    QModelIndexList selection = m_table->selectionModel()->selectedRows();
    if (selection.isEmpty()) return;

    int row = selection.first().row();
    QString url = getHtmlUrlForRow(row);
    if (!url.isEmpty()) {
        QApplication::clipboard()->setText(url);
    }
}

// 7. Private Helpers
void WorkItemWindow::setupUi() {
    setWindowTitle(m_windowTitle);
    resize(800, 600);

    // Table
    m_table = new QTableWidget(this);
    m_table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_table->setColumnCount(5);
    if (m_endpointType == EndpointIssues) {
        m_table->setHorizontalHeaderLabels(
            {tr("Repository"), tr("Title"), tr("State"), tr("Author"), tr("Created At")});
    } else {
        m_table->setHorizontalHeaderLabels(
            {tr("Repository Name"), tr("Description"), tr("Language"), tr("Owner"), tr("Created At")});
    }
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_table, &QTableWidget::customContextMenuRequested, this, &WorkItemWindow::onCustomContextMenuRequested);
    connect(m_table, &QTableWidget::itemDoubleClicked, this, &WorkItemWindow::onItemDoubleClicked);

    setObjectName("WorkItemWindow");
    setCentralWidget(m_table);

    // Actions
    QAction* exportCsvAction = new QAction(tr("Export to CSV"), this);
    connect(exportCsvAction, &QAction::triggered, this, &WorkItemWindow::exportToCsv);
    actionCollection()->addAction(QStringLiteral("export_csv"), exportCsvAction);

    QAction* exportJsonAction = new QAction(tr("Export to JSON"), this);
    connect(exportJsonAction, &QAction::triggered, this, &WorkItemWindow::exportToJson);
    actionCollection()->addAction(QStringLiteral("export_json"), exportJsonAction);

    QAction* refreshAction = KStandardAction::redisplay(this, [this]() { startRefresh(); }, actionCollection());

    KStandardAction::close(this, &WorkItemWindow::close, actionCollection());

    m_copyAction = KStandardAction::copy(this, &WorkItemWindow::copyLink, actionCollection());
    m_copyAction->setText(tr("Copy Link"));

    setupGUI(Default, ":/kgithub-notifyui.rc");

    // Tool Bar
    QToolBar* toolBar = addToolBar(tr("Main Toolbar"));
    toolBar->setObjectName("WorkItemMainToolBar");
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

void WorkItemWindow::loadData(int page) {
    if (page == 1) {
        startRefresh();
    } else {
        fetchPage(m_currentGenerationId, page);
    }
}

void WorkItemWindow::fetchPage(const QUuid& generationId, int page) {
    if (generationId.isNull() || generationId != m_currentGenerationId) {
        return;
    }

    if (m_inFlightPages.contains(page)) {
        return;
    }
    m_inFlightPages.insert(page);

    QString endpointStr = (m_endpointType == EndpointIssues) ? "issues" : "repositories";
    QUrl url("https://api.github.com/search/" + endpointStr + "?q=" + QUrl::toPercentEncoding(m_baseQuery) +
             "&per_page=100&page=" + QString::number(page));

    QNetworkRequest request = m_client ? m_client->createAuthenticatedRequest(url) : QNetworkRequest(url);
    QNetworkReply* reply = m_manager->get(request);
    if (!reply) return;

    reply->setProperty("generationId", generationId.toString());
    reply->setProperty("page", page);

    m_inFlightReplies.insert(reply);
}

void WorkItemWindow::commitStagedData(int totalCount, int maxPages) {
    m_allData = QJsonArray();
    m_table->setRowCount(0);

    for (auto it = m_stagedPages.begin(); it != m_stagedPages.end(); ++it) {
        const QJsonArray& pageItems = it.value();
        for (int i = 0; i < pageItems.size(); ++i) {
            m_allData.append(pageItems[i]);
            appendRow(pageItems[i].toObject());
        }
    }

    int loadedCount = m_allData.size();
    m_stagedPages.clear();
    m_inFlightPages.clear();
    m_currentGenerationId = QUuid();
    saveCache();

    QString limitMsg = (totalCount > 1000) ? tr(" (GitHub Search Limit Reached)") : "";
    int pagesLoaded = (totalCount == 0) ? 0 : qMin(maxPages, (loadedCount + 99) / 100);
    m_statusLabel->setText(tr("Items: %1%2 | Pages loaded: %3 / %4 | Last refresh: %5")
                               .arg(loadedCount)
                               .arg(limitMsg)
                               .arg(pagesLoaded)
                               .arg(maxPages)
                               .arg(QDateTime::currentDateTime().toString()));
}

void WorkItemWindow::appendRow(const QJsonObject& item) {
    int row = m_table->rowCount();
    m_table->insertRow(row);

    if (m_endpointType == EndpointIssues) {
        QString htmlUrl = item["html_url"].toString();
        QString title = item["title"].toString();
        QString state = item["state"].toString();
        QString createdAt = item["created_at"].toString();

        QJsonObject user = item["user"].toObject();
        QString author = user["login"].toString();

        QString repoUrl = item["repository_url"].toString();
        QString repo = repoUrl.section('/', -2);

        QTableWidgetItem* repoItem = new QTableWidgetItem(repo);
        QTableWidgetItem* titleItem = new QTableWidgetItem(title);
        QTableWidgetItem* stateItem = new QTableWidgetItem(state);
        QTableWidgetItem* authorItem = new QTableWidgetItem(author);

        QDateTime dt = QDateTime::fromString(createdAt, Qt::ISODate);
        QString displayDate = dt.isValid() ? QLocale().toString(dt.toLocalTime(), QLocale::ShortFormat) : createdAt;
        QTableWidgetItem* createdItem = new QTableWidgetItem(displayDate);

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

        QTableWidgetItem* repoItem = new QTableWidgetItem(fullName);
        QTableWidgetItem* descItem = new QTableWidgetItem(description);
        QTableWidgetItem* langItem = new QTableWidgetItem(language);
        QTableWidgetItem* ownerItem = new QTableWidgetItem(owner);

        QDateTime dt = QDateTime::fromString(createdAt, Qt::ISODate);
        QString displayDate = dt.isValid() ? QLocale().toString(dt.toLocalTime(), QLocale::ShortFormat) : createdAt;
        QTableWidgetItem* createdItem = new QTableWidgetItem(displayDate);

        repoItem->setData(Qt::UserRole, htmlUrl);

        m_table->setItem(row, 0, repoItem);
        m_table->setItem(row, 1, descItem);
        m_table->setItem(row, 2, langItem);
        m_table->setItem(row, 3, ownerItem);
        m_table->setItem(row, 4, createdItem);
    }
}

QString WorkItemWindow::getHtmlUrlForRow(int row) const {
    QTableWidgetItem* item = m_table->item(row, (m_endpointType == EndpointIssues) ? 1 : 0);
    if (!item) return QString();
    return item->data(Qt::UserRole).toString();
}

QString WorkItemWindow::getCacheFilePath() const {
    QString hash = QString::number(qHash(m_baseQuery), 16);
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/workitems_" + hash + ".json";
}

void WorkItemWindow::loadCache() {
    QFile file(getCacheFilePath());
    if (!file.open(QIODevice::ReadOnly)) return;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) return;

    QJsonObject obj = doc.object();
    m_allData = obj["items"].toArray();
    QString lastRefresh = obj["lastRefresh"].toString();

    m_table->setRowCount(0);
    for (int i = 0; i < m_allData.size(); ++i) {
        appendRow(m_allData[i].toObject());
    }
    m_statusLabel->setText(tr("Cached data from %1 - Items: %2").arg(lastRefresh).arg(m_allData.size()));
}

void WorkItemWindow::saveCache() {
    QFile file(getCacheFilePath());
    if (!file.open(QIODevice::WriteOnly)) return;

    QJsonObject obj;
    obj["items"] = m_allData;
    obj["lastRefresh"] = QDateTime::currentDateTime().toString();
    QJsonDocument doc(obj);
    file.write(doc.toJson());
}
