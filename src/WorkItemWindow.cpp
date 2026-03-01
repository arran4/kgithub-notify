#include "WorkItemWindow.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QHeaderView>
#include <QMenu>
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QUrl>
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>

WorkItemWindow::WorkItemWindow(GitHubClient *client, Type type, QWidget *parent)
    : QDialog(parent), m_client(client), m_type(type), m_manager(new QNetworkAccessManager(this))
{
    setupUi();
    connect(m_manager, &QNetworkAccessManager::finished, this, &WorkItemWindow::onReplyFinished);
    loadData();
}

WorkItemWindow::~WorkItemWindow()
{
}

void WorkItemWindow::setupUi()
{
    if (m_type == Issues) {
        setWindowTitle(tr("My Open Issues"));
    } else {
        setWindowTitle(tr("My Open Pull Requests"));
    }
    resize(800, 600);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Table
    m_table = new QTableWidget(this);
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels({tr("Repository"), tr("Title"), tr("State"), tr("Author"), tr("Created At")});
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_table, &QTableWidget::customContextMenuRequested, this, &WorkItemWindow::onCustomContextMenuRequested);
    connect(m_table, &QTableWidget::itemDoubleClicked, this, &WorkItemWindow::onItemDoubleClicked);

    mainLayout->addWidget(m_table);

    // Buttons Layout
    QHBoxLayout *buttonsLayout = new QHBoxLayout();
    buttonsLayout->addStretch();

    m_exportCsvBtn = new QPushButton(tr("Export to CSV"), this);
    connect(m_exportCsvBtn, &QPushButton::clicked, this, &WorkItemWindow::exportToCsv);
    buttonsLayout->addWidget(m_exportCsvBtn);

    m_exportJsonBtn = new QPushButton(tr("Export to JSON"), this);
    connect(m_exportJsonBtn, &QPushButton::clicked, this, &WorkItemWindow::exportToJson);
    buttonsLayout->addWidget(m_exportJsonBtn);

    mainLayout->addLayout(buttonsLayout);

    // Context Menu Actions
    m_openAction = new QAction(tr("Open in Browser"), this);
    connect(m_openAction, &QAction::triggered, this, &WorkItemWindow::openInBrowser);

    m_copyAction = new QAction(tr("Copy Link"), this);
    connect(m_copyAction, &QAction::triggered, this, &WorkItemWindow::copyLink);
}

void WorkItemWindow::loadData()
{
    QString query = (m_type == Issues) ? "is:open is:issue assignee:@me" : "is:open is:pr assignee:@me";
    QUrl url("https://api.github.com/search/issues?q=" + QUrl::toPercentEncoding(query));

    QNetworkRequest request = m_client->createAuthenticatedRequest(url);
    m_manager->get(request);
}

void WorkItemWindow::onReplyFinished(QNetworkReply *reply)
{
    if (reply->error() == QNetworkReply::NoError) {
        populateTable(reply->readAll());
    } else {
        QMessageBox::warning(this, tr("Error"), tr("Failed to fetch data: %1").arg(reply->errorString()));
    }
    reply->deleteLater();
}

void WorkItemWindow::populateTable(const QByteArray &data)
{
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) return;

    QJsonObject obj = doc.object();
    QJsonArray items = obj["items"].toArray();

    m_table->setRowCount(items.size());

    for (int i = 0; i < items.size(); ++i) {
        QJsonObject item = items[i].toObject();

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

        m_table->setItem(i, 0, repoItem);
        m_table->setItem(i, 1, titleItem);
        m_table->setItem(i, 2, stateItem);
        m_table->setItem(i, 3, authorItem);
        m_table->setItem(i, 4, createdItem);
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
    QTableWidgetItem *titleItem = m_table->item(row, 1); // 1 is Title column
    if (titleItem) {
        return titleItem->data(Qt::UserRole).toString();
    }
    return QString();
}
