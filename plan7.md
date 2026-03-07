```cpp
// WorkItemWindow.h
class WorkItemWindow : public QDialog {
    Q_OBJECT

public:
    enum EndpointType {
        EndpointIssues,
        EndpointRepositories
    };
    explicit WorkItemWindow(GitHubClient *client, const QString& windowTitle, EndpointType endpointType, const QString& baseQuery, QWidget *parent = nullptr);
    // ...
private:
    EndpointType m_endpointType;
    // ...
```

In `WorkItemWindow.cpp`:
```cpp
WorkItemWindow::WorkItemWindow(GitHubClient *client, const QString& windowTitle, EndpointType endpointType, const QString& baseQuery, QWidget *parent)
    : QDialog(parent), m_client(client), m_windowTitle(windowTitle), m_endpointType(endpointType), m_baseQuery(baseQuery), m_currentPage(1), m_manager(new QNetworkAccessManager(this))
{
    // ...
}

void WorkItemWindow::setupUi()
{
    // ... table initialization
    if (m_endpointType == EndpointIssues) {
        m_table->setColumnCount(5);
        m_table->setHorizontalHeaderLabels({tr("Repository"), tr("Title"), tr("State"), tr("Author"), tr("Created At")});
    } else {
        m_table->setColumnCount(5);
        m_table->setHorizontalHeaderLabels({tr("Repository Name"), tr("Description"), tr("Language"), tr("Owner"), tr("Created At")});
    }
}

void WorkItemWindow::loadData(int page)
{
    m_currentPage = page;
    QString endpointStr = (m_endpointType == EndpointIssues) ? "issues" : "repositories";
    QUrl url("https://api.github.com/search/" + endpointStr + "?q=" + QUrl::toPercentEncoding(m_baseQuery) + "&per_page=100&page=" + QString::number(m_currentPage));
    // ...
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

        QString repoUrl = item["repository_url"].toString();
        QString repo = repoUrl.section('/', -2);

        QTableWidgetItem *repoItem = new QTableWidgetItem(repo);
        QTableWidgetItem *titleItem = new QTableWidgetItem(title);
        QTableWidgetItem *stateItem = new QTableWidgetItem(state);
        QTableWidgetItem *authorItem = new QTableWidgetItem(author);
        QTableWidgetItem *createdItem = new QTableWidgetItem(createdAt);

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

        repoItem->setData(Qt::UserRole, htmlUrl);

        m_table->setItem(row, 0, repoItem);
        m_table->setItem(row, 1, descItem);
        m_table->setItem(row, 2, langItem);
        m_table->setItem(row, 3, ownerItem);
        m_table->setItem(row, 4, createdItem);
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
```

Wait, `MainWindow::showWorkItems`:
```cpp
void MainWindow::showWorkItems(const QString &title, int endpointType, const QString &query) {
    WorkItemWindow::EndpointType type = (endpointType == 0) ? WorkItemWindow::EndpointIssues : WorkItemWindow::EndpointRepositories;
    WorkItemWindow *window = new WorkItemWindow(client, title, type, query, this);
    window->setAttribute(Qt::WA_DeleteOnClose);
    window->show();
}
```
