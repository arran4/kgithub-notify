#include "RepoListWindow.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTableWidget>
#include <QTextStream>
#include <QTimer>
#include <QToolBar>
#include <QUrl>
#include <QVBoxLayout>

RepoListWindow::RepoListWindow(GitHubClient *client, QWidget *parent)
    : QMainWindow(parent), m_client(client), m_table(nullptr), m_toolbar(nullptr), m_statusBar(nullptr), m_timerLabel(nullptr), m_updateTimer(nullptr) {

    setupUI();
    loadCache();

    connect(m_client, &GitHubClient::userReposReceived, this, &RepoListWindow::onReposReceived);
    connect(m_client, &GitHubClient::errorOccurred, this, &RepoListWindow::onError);

    m_updateTimer = new QTimer(this);
    connect(m_updateTimer, &QTimer::timeout, this, &RepoListWindow::updateTimerLabel);
    m_updateTimer->start(60000); // 1 minute
}

void RepoListWindow::setupUI() {
    setWindowTitle(tr("Repositories"));
    resize(1000, 600);

    // Table
    m_table = new QTableWidget(this);
    m_table->setColumnCount(8);
    m_table->setHorizontalHeaderLabels({tr("Name"), tr("Owner"), tr("Visibility"), tr("Stars"), tr("Forks"), tr("Open Issues"), tr("Updated"), tr("URL")});
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->verticalHeader()->setVisible(false);
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    m_table->setSortingEnabled(true);
    connect(m_table, &QWidget::customContextMenuRequested, this, &RepoListWindow::onCustomContextMenuRequested);

    QHeaderView *header = m_table->horizontalHeader();
    header->setSectionResizeMode(QHeaderView::ResizeToContents);
    header->setStretchLastSection(true);

    setCentralWidget(m_table);

    // Toolbar
    m_toolbar = addToolBar(tr("Main Toolbar"));
    m_toolbar->setMovable(false);

    QAction *refreshAction = new QAction(QIcon::fromTheme("view-refresh"), tr("Refresh"), this);
    connect(refreshAction, &QAction::triggered, this, &RepoListWindow::onRefreshClicked);
    m_toolbar->addAction(refreshAction);

    QAction *exportAction = new QAction(QIcon::fromTheme("document-export"), tr("Export to CSV"), this);
    connect(exportAction, &QAction::triggered, this, &RepoListWindow::onExportClicked);
    m_toolbar->addAction(exportAction);

    // Status bar
    m_statusBar = statusBar();
    m_timerLabel = new QLabel(tr("Last refresh: Never"), this);
    m_statusBar->addPermanentWidget(m_timerLabel);
}

void RepoListWindow::onRefreshClicked() {
    m_allRepos = QJsonArray(); // Clear previous
    m_client->fetchUserRepos();
    if(m_statusBar) m_statusBar->showMessage(tr("Fetching repositories..."));
}

void RepoListWindow::onExportClicked() {
    QString fileName = QFileDialog::getSaveFileName(this, tr("Export Repositories"), QDir::homePath() + "/repositories.csv", tr("CSV Files (*.csv)"));
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Export Error"), tr("Could not open file for writing: %1").arg(file.errorString()));
        return;
    }

    QTextStream out(&file);

    // Headers
    QStringList headers;
    for (int col = 0; col < m_table->columnCount(); ++col) {
        headers << "\"" + m_table->horizontalHeaderItem(col)->text().replace("\"", "\"\"") + "\"";
    }
    out << headers.join(",") << "\n";

    // Data
    for (int row = 0; row < m_table->rowCount(); ++row) {
        QStringList rowData;
        for (int col = 0; col < m_table->columnCount(); ++col) {
            QTableWidgetItem *item = m_table->item(row, col);
            QString text = item ? item->text() : "";
            rowData << "\"" + text.replace("\"", "\"\"") + "\"";
        }
        out << rowData.join(",") << "\n";
    }

    file.close();
    m_statusBar->showMessage(tr("Exported to %1").arg(fileName), 5000);
}

void RepoListWindow::onReposReceived(const QJsonArray &repos, const QString &nextPageUrl) {
    for (const QJsonValue &val : repos) {
        m_allRepos.append(val);
    }

    if (!nextPageUrl.isEmpty()) {
        m_client->fetchUserRepos(nextPageUrl);
    } else {
        m_lastRefresh = QDateTime::currentDateTime();
        saveCache();
        addReposToTable(m_allRepos);
        updateTimerLabel();
        if(m_statusBar) m_statusBar->showMessage(tr("Finished fetching repositories."), 5000);
    }
}

void RepoListWindow::addReposToTable(const QJsonArray &repos) {
    m_table->setSortingEnabled(false);
    m_table->setRowCount(repos.size());

    for (int i = 0; i < repos.size(); ++i) {
        QJsonObject repo = repos[i].toObject();

        QTableWidgetItem *nameItem = new QTableWidgetItem(repo["name"].toString());
        QTableWidgetItem *ownerItem = new QTableWidgetItem(repo["owner"].toObject()["login"].toString());
        QTableWidgetItem *visItem = new QTableWidgetItem(repo["visibility"].toString());

        QTableWidgetItem *starsItem = new QTableWidgetItem();
        starsItem->setData(Qt::DisplayRole, repo["stargazers_count"].toInt());

        QTableWidgetItem *forksItem = new QTableWidgetItem();
        forksItem->setData(Qt::DisplayRole, repo["forks_count"].toInt());

        QTableWidgetItem *issuesItem = new QTableWidgetItem();
        issuesItem->setData(Qt::DisplayRole, repo["open_issues_count"].toInt());

        QString updatedStr = repo["updated_at"].toString();
        QDateTime dt = QDateTime::fromString(updatedStr, Qt::ISODate);

        class DateTableItem : public QTableWidgetItem {
        public:
            DateTableItem(const QString &text, const QDateTime &date) : QTableWidgetItem(text), m_date(date) {}
            bool operator<(const QTableWidgetItem &other) const override {
                const DateTableItem *otherDateItem = dynamic_cast<const DateTableItem*>(&other);
                if (otherDateItem) {
                    return m_date < otherDateItem->m_date;
                }
                return QTableWidgetItem::operator<(other);
            }
        private:
            QDateTime m_date;
        };

        QTableWidgetItem *updatedItem = new DateTableItem(QLocale::system().toString(dt, QLocale::ShortFormat), dt);

        QTableWidgetItem *urlItem = new QTableWidgetItem(repo["html_url"].toString());

        m_table->setItem(i, 0, nameItem);
        m_table->setItem(i, 1, ownerItem);
        m_table->setItem(i, 2, visItem);
        m_table->setItem(i, 3, starsItem);
        m_table->setItem(i, 4, forksItem);
        m_table->setItem(i, 5, issuesItem);
        m_table->setItem(i, 6, updatedItem);
        m_table->setItem(i, 7, urlItem);
    }
    m_table->setSortingEnabled(true);
}

void RepoListWindow::updateTimerLabel() {
    if (!m_lastRefresh.isValid()) {
        m_timerLabel->setText(tr("Last refresh: Never"));
        return;
    }

    qint64 secs = m_lastRefresh.secsTo(QDateTime::currentDateTime());
    if (secs < 60) {
        m_timerLabel->setText(tr("Last refresh: just now"));
    } else {
        qint64 mins = secs / 60;
        m_timerLabel->setText(tr("Last refresh: %n minute(s) ago", "", mins));
    }
}

void RepoListWindow::onCustomContextMenuRequested(const QPoint &pos) {
    QTableWidgetItem *item = m_table->itemAt(pos);
    if (!item) return;

    int row = item->row();
    QTableWidgetItem *urlItem = m_table->item(row, 7); // URL is col 7
    if (!urlItem) return;

    QString url = urlItem->text();

    QMenu menu(this);
    QAction *openAction = menu.addAction(QIcon::fromTheme("internet-web-browser"), tr("Open in Browser"));
    QAction *copyAction = menu.addAction(QIcon::fromTheme("edit-copy"), tr("Copy URL"));

    QAction *selected = menu.exec(m_table->viewport()->mapToGlobal(pos));

    if (selected == openAction) {
        QDesktopServices::openUrl(QUrl(url));
    } else if (selected == copyAction) {
        QApplication::clipboard()->setText(url);
    }
}

void RepoListWindow::onError(const QString &error) {
    if (m_statusBar) {
        m_statusBar->showMessage(tr("Error fetching repositories: %1").arg(error), 5000);
    }
}

void RepoListWindow::loadCache() {
    QString cachePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/repos_cache.json";
    QFile file(cachePath);

    if (file.open(QIODevice::ReadOnly)) {
        QByteArray data = file.readAll();
        file.close();

        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isObject()) {
            QJsonObject obj = doc.object();

            if (obj.contains("last_refresh")) {
                m_lastRefresh = QDateTime::fromString(obj["last_refresh"].toString(), Qt::ISODate);
            }

            if (obj.contains("repos") && obj["repos"].isArray()) {
                QJsonArray repos = obj["repos"].toArray();
                addReposToTable(repos);
            }
        }
    }
    updateTimerLabel();
}

void RepoListWindow::saveCache() {
    QString dirPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir;
    dir.mkpath(dirPath);

    QString cachePath = dirPath + "/repos_cache.json";
    QFile file(cachePath);

    if (file.open(QIODevice::WriteOnly)) {
        QJsonObject obj;
        if (m_lastRefresh.isValid()) {
            obj["last_refresh"] = m_lastRefresh.toString(Qt::ISODate);
        }
        obj["repos"] = m_allRepos;

        QJsonDocument doc(obj);
        file.write(doc.toJson());
        file.close();
    }
}
