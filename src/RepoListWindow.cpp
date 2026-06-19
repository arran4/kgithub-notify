#include "RepoListWindow.h"

#include <KActionCollection>
#include <KStandardAction>
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
#include <QtGui/QAction>

#include "NewIssueDialog.h"
#include "utils/FilterParser.h"

class RepoAccessor : public FilterDataAccessor {
   public:
    RepoAccessor(const QJsonObject& repo) : m_repo(repo) {}

    QString getValue(const QString& key) const override {
        QString lowerKey = key.toLower();
        if (lowerKey == "fork") return m_repo["fork"].toBool() ? "true" : "false";
        if (lowerKey == "archived") return m_repo["archived"].toBool() ? "true" : "false";
        if (lowerKey == "name") return m_repo["name"].toString();
        if (lowerKey == "owner") return m_repo["owner"].toObject()["login"].toString();
        if (lowerKey == "visibility") return m_repo["visibility"].toString();
        if (lowerKey == "createdat") return m_repo["created_at"].toString();
        if (lowerKey == "updatedat") return m_repo["updated_at"].toString();
        return "";
    }

    QList<QString> getAllValues() const override {
        return {m_repo["fork"].toBool() ? "true" : "false",
                m_repo["archived"].toBool() ? "true" : "false",
                m_repo["name"].toString(),
                m_repo["owner"].toObject()["login"].toString(),
                m_repo["visibility"].toString(),
                m_repo["created_at"].toString(),
                m_repo["updated_at"].toString()};
    }

   private:
    QJsonObject m_repo;
};

RepoListWindow::RepoListWindow(GitHubClient* client, QWidget* parent)
    : KXmlGuiWindow(parent),
      m_client(client),
      m_table(nullptr),
      m_toolbar(nullptr),
      m_filterCombo(nullptr),
      m_filterEdit(nullptr),
      m_statusBar(nullptr),
      m_timerLabel(nullptr),
      m_updateTimer(nullptr) {
    setupUI();
    loadCache();

    connect(m_client, &GitHubClient::userReposReceived, this, &RepoListWindow::onReposReceived);
    connect(m_client, &GitHubClient::errorOccurred, this, &RepoListWindow::onError);

    m_updateTimer = new QTimer(this);
    connect(m_updateTimer, &QTimer::timeout, this, &RepoListWindow::updateTimerLabel);
    m_updateTimer->start(60000);  // 1 minute
}

void RepoListWindow::setupUI() {
    setWindowTitle(tr("Repositories"));
    resize(1000, 600);

    // Table
    m_table = new QTableWidget(this);
    m_table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_table->setColumnCount(ColumnCount);
    m_table->setHorizontalHeaderLabels({tr("Name"), tr("Owner"), tr("Visibility"), tr("Stars"), tr("Forks"),
                                        tr("Open Issues"), tr("Created"), tr("Updated"), tr("Archived"), tr("Fork"),
                                        tr("URL")});
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->verticalHeader()->setVisible(false);
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    m_table->setSortingEnabled(true);
    connect(m_table, &QWidget::customContextMenuRequested, this, &RepoListWindow::onCustomContextMenuRequested);

    QHeaderView* header = m_table->horizontalHeader();
    header->setSectionResizeMode(QHeaderView::ResizeToContents);
    header->setStretchLastSection(true);

    setObjectName("RepoListWindow");
    setCentralWidget(m_table);

    // Actions
    QAction* refreshAction = KStandardAction::redisplay(this, &RepoListWindow::onRefreshClicked, actionCollection());

    QAction* exportAction = new QAction(QIcon::fromTheme("document-export"), tr("Export to CSV"), this);
    connect(exportAction, &QAction::triggered, this, &RepoListWindow::onExportClicked);
    actionCollection()->addAction(QStringLiteral("export_csv"), exportAction);

    KStandardAction::close(this, &RepoListWindow::close, actionCollection());

    KStandardAction::copy(
        this,
        [this]() {
            QList<QTableWidgetItem*> items = m_table->selectedItems();
            if (!items.isEmpty()) {
                int row = items.first()->row();
                QTableWidgetItem* urlItem = m_table->item(row, ColUrl);
                if (urlItem) {
                    QApplication::clipboard()->setText(urlItem->text());
                }
            }
        },
        actionCollection());

    setupGUI(Default, ":/kgithub-notifyui.rc");

    // Toolbar
    m_toolbar = addToolBar(tr("Main Toolbar"));
    m_toolbar->setObjectName("RepoListMainToolBar");
    m_toolbar->setMovable(false);

    m_filterCombo = new QComboBox(this);
    m_filterCombo->addItem(tr("Custom"), "fork:false AND archived:false");
    m_filterCombo->addItem(tr("All"), "");
    m_filterCombo->addItem(tr("Not Forks"), "fork:false");
    m_filterCombo->addItem(tr("Not Archived"), "archived:false");

    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText(tr("Filter repositories... e.g. fork:false AND archived:false"));
    m_filterEdit->setText("fork:false AND archived:false");

    connect(m_filterCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        QString preset = m_filterCombo->itemData(index).toString();
        m_filterEdit->setText(preset);
    });
    connect(m_filterEdit, &QLineEdit::textChanged, this, &RepoListWindow::onFilterChanged);

    m_toolbar->addAction(refreshAction);
    m_toolbar->addAction(exportAction);
    m_toolbar->addSeparator();
    m_toolbar->addWidget(new QLabel(tr("Filter: "), this));
    m_toolbar->addWidget(m_filterCombo);
    m_toolbar->addWidget(m_filterEdit);

    // Status bar
    m_statusBar = statusBar();
    m_timerLabel = new QLabel(tr("Last refresh: Never"), this);
    m_statusBar->addPermanentWidget(m_timerLabel);
}

void RepoListWindow::onRefreshClicked() {
    m_allRepos = QJsonArray();  // Clear previous
    m_client->fetchUserRepos();
    if (m_statusBar) m_statusBar->showMessage(tr("Fetching repositories..."));
}

void RepoListWindow::onFilterChanged() { addReposToTable(m_allRepos); }

void RepoListWindow::onExportClicked() {
    QString fileName = QFileDialog::getSaveFileName(this, tr("Export Repositories"),
                                                    QDir::homePath() + "/repositories.csv", tr("CSV Files (*.csv)"));
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Export Error"),
                             tr("Could not open file for writing: %1").arg(file.errorString()));
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
            QTableWidgetItem* item = m_table->item(row, col);
            QString text = item ? item->text() : "";
            rowData << "\"" + text.replace("\"", "\"\"") + "\"";
        }
        out << rowData.join(",") << "\n";
    }

    file.close();
    m_statusBar->showMessage(tr("Exported to %1").arg(fileName), 5000);
}

void RepoListWindow::onReposReceived(const QJsonArray& repos, const QString& nextPageUrl) {
    for (const QJsonValue& val : repos) {
        m_allRepos.append(val);
    }

    if (!nextPageUrl.isEmpty()) {
        m_client->fetchUserRepos(nextPageUrl);
    } else {
        m_lastRefresh = QDateTime::currentDateTime();
        saveCache();
        addReposToTable(m_allRepos);
        updateTimerLabel();
        if (m_statusBar) m_statusBar->showMessage(tr("Finished fetching repositories."), 5000);
    }
}

void RepoListWindow::addReposToTable(const QJsonArray& repos) {
    m_table->setSortingEnabled(false);
    m_table->setRowCount(0);  // clear rows

    QString filterQuery = m_filterEdit ? m_filterEdit->text().trimmed() : "";
    QSharedPointer<ASTNode> ast;
    if (!filterQuery.isEmpty()) {
        ast = FilterParser::parse(filterQuery);
    }

    int row = 0;
    for (int i = 0; i < repos.size(); ++i) {
        QJsonObject repo = repos[i].toObject();

        if (ast) {
            RepoAccessor accessor(repo);
            if (!ast->evaluate(accessor)) {
                continue;
            }
        }

        m_table->insertRow(row);

        QTableWidgetItem* nameItem = new QTableWidgetItem(repo["name"].toString());
        QTableWidgetItem* ownerItem = new QTableWidgetItem(repo["owner"].toObject()["login"].toString());
        QTableWidgetItem* visItem = new QTableWidgetItem(repo["visibility"].toString());

        QTableWidgetItem* starsItem = new QTableWidgetItem();
        starsItem->setData(Qt::DisplayRole, repo["stargazers_count"].toInt());

        QTableWidgetItem* forksItem = new QTableWidgetItem();
        forksItem->setData(Qt::DisplayRole, repo["forks_count"].toInt());

        QTableWidgetItem* issuesItem = new QTableWidgetItem();
        issuesItem->setData(Qt::DisplayRole, repo["open_issues_count"].toInt());

        QString createdStr = repo["created_at"].toString();
        QDateTime createdDt = QDateTime::fromString(createdStr, Qt::ISODate);

        QString updatedStr = repo["updated_at"].toString();
        QDateTime updatedDt = QDateTime::fromString(updatedStr, Qt::ISODate);

        class DateTableItem : public QTableWidgetItem {
           public:
            DateTableItem(const QString& text, const QDateTime& date) : QTableWidgetItem(text), m_date(date) {}
            bool operator<(const QTableWidgetItem& other) const override {
                const DateTableItem* otherDateItem = dynamic_cast<const DateTableItem*>(&other);
                if (otherDateItem) {
                    return m_date < otherDateItem->m_date;
                }
                return QTableWidgetItem::operator<(other);
            }

           private:
            QDateTime m_date;
        };

        QTableWidgetItem* createdItem =
            new DateTableItem(QLocale::system().toString(createdDt, QLocale::ShortFormat), createdDt);
        QTableWidgetItem* updatedItem =
            new DateTableItem(QLocale::system().toString(updatedDt, QLocale::ShortFormat), updatedDt);

        QTableWidgetItem* archivedItem = new QTableWidgetItem(repo["archived"].toBool() ? tr("Yes") : tr("No"));
        QTableWidgetItem* isForkItem = new QTableWidgetItem(repo["fork"].toBool() ? tr("Yes") : tr("No"));

        QTableWidgetItem* urlItem = new QTableWidgetItem(repo["html_url"].toString());

        m_table->setItem(row, ColName, nameItem);
        m_table->setItem(row, ColOwner, ownerItem);
        m_table->setItem(row, ColVisibility, visItem);
        m_table->setItem(row, ColStars, starsItem);
        m_table->setItem(row, ColForks, forksItem);
        m_table->setItem(row, ColOpenIssues, issuesItem);
        m_table->setItem(row, ColCreated, createdItem);
        m_table->setItem(row, ColUpdated, updatedItem);
        m_table->setItem(row, ColArchived, archivedItem);
        m_table->setItem(row, ColIsFork, isForkItem);
        m_table->setItem(row, ColUrl, urlItem);

        row++;
    }
    m_table->setSortingEnabled(true);
    m_table->sortItems(ColUpdated, Qt::DescendingOrder);
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

void RepoListWindow::onCustomContextMenuRequested(const QPoint& pos) {
    QTableWidgetItem* item = m_table->itemAt(pos);
    if (!item) return;

    int row = item->row();
    QTableWidgetItem* urlItem = m_table->item(row, ColUrl);
    if (!urlItem) return;

    QString url = urlItem->text();

    QMenu menu(this);
    QAction* openAction = menu.addAction(QIcon::fromTheme("internet-web-browser"), tr("Open in Browser"));
    QAction* copyAction = menu.addAction(QIcon::fromTheme("edit-copy"), tr("Copy URL"));
    menu.addSeparator();
    QAction* newIssueAction = menu.addAction(QIcon::fromTheme("document-new"), tr("New Issue..."));

    QAction* selected = menu.exec(m_table->viewport()->mapToGlobal(pos));

    if (selected == openAction) {
        QDesktopServices::openUrl(QUrl(url));
    } else if (selected == copyAction) {
        QApplication::clipboard()->setText(url);
    } else if (selected == newIssueAction) {
        if (m_client) {
            NewIssueDialog* dialog = new NewIssueDialog(m_client, this);
            dialog->setAttribute(Qt::WA_DeleteOnClose);
            QString repoName = QUrl(url).path().mid(1);  // removes leading slash
            dialog->setInitialRepo(repoName);
            dialog->show();
        }
    }
}

void RepoListWindow::onError(const QString& error) {
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
