The user wants two things:
1. "also the "menu" needs to be fully populated, ie "close" should also be an option."
   This means adding a "Close" action to the `File` menu that closes the window (`close()`).

2. "PLus when I press refresh (on the toolbar) it doesn't cause the data to refresh. Or if it does it's not clear it is."
   When the user clicks "Refresh", it calls `loadData(1)`, which does a network request. However, `loadData(1)` does NOT clear `m_allData` or `m_table` *until* the reply finishes (`onReplyFinished`). If the request takes a second, there is no visual feedback that it started.
   We should update `m_statusLabel` immediately in `loadData()` to say "Refreshing..." or "Loading...". And maybe disable the refresh button while loading? But just setting the status text is good enough.
   Also, `onReplyFinished` has:
   ```cpp
        if (m_currentPage == 1) {
            m_allData = QJsonArray();
            m_table->setRowCount(0);
        }
   ```
   We can move `m_table->setRowCount(0)` directly into `loadData(1)` so it visually clears the table to signify a refresh. But that might cause an empty flash if it's the initial load. Wait, initial load already has cache. So clearing it might flash. Maybe just update the `m_statusLabel` to "Refreshing...".

Let's modify `loadData(1)`:
```cpp
void WorkItemWindow::loadData(int page)
{
    m_currentPage = page;
    QString endpointStr = (m_endpointType == EndpointIssues) ? "issues" : "repositories";
    QUrl url("https://api.github.com/search/" + endpointStr + "?q=" + QUrl::toPercentEncoding(m_baseQuery) + "&per_page=100&page=" + QString::number(m_currentPage));

    if (page == 1) {
        m_statusLabel->setText(tr("Refreshing data..."));
    }

    QNetworkRequest request = m_client->createAuthenticatedRequest(url);
    m_manager->get(request);
}
```

Wait, what if they press refresh and it immediately fetches from cache? `loadData` doesn't fetch from cache, it fetches from network. The cache is only loaded once in the constructor.

Let's add `Close` action to File menu:
```cpp
    QAction *closeAction = new QAction(tr("Close"), this);
    connect(closeAction, &QAction::triggered, this, &WorkItemWindow::close);

    QMenuBar *menuBarWidget = menuBar();
    QMenu *fileMenu = menuBarWidget->addMenu(tr("&File"));
    fileMenu->addAction(exportCsvAction);
    fileMenu->addAction(exportJsonAction);
    fileMenu->addSeparator();
    fileMenu->addAction(closeAction);
```

Let's do this.
