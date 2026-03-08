The user wants to:
1. "in the window we should add a toolbar, and menu, where we move the export buttons"
   Currently `WorkItemWindow` is a `QDialog`. `QDialog` does not have `menuBar()` or `addToolBar()`. We need to change it to `QMainWindow`.
   Wait, if we change it to `QMainWindow`, we can easily add a `QMenuBar`, `QToolBar`, and `QStatusBar`. We already have a fake `QLabel` status bar, which we can easily migrate to `QStatusBar` or keep as a widget inside the `QMainWindow::statusBar()`.

2. "can we add a refresh"
   Add a refresh action/button that basically clears the cache, sets `m_currentPage = 1`, and calls `loadData(1)`.

3. "We should also put into the status bar how many pages (out of how many) were loaded."
   When loading finishes, the status bar currently says: `Items: %1 | Last refresh: %2`.
   The user wants: `Items: %1 (Pages: %2/%3) | Last refresh: ...`
   How many pages were loaded? `m_currentPage` tells us how many were fetched.
   Out of how many? `ceil(totalCount / 100)`. Wait, totalCount max is 1000. So `ceil(min(totalCount, 1000) / 100.0)`.

Let's do this:
1. Change `WorkItemWindow` from `QDialog` to `QMainWindow`.
2. Move table to `setCentralWidget(m_table)`.
3. Remove the `QHBoxLayout` with buttons.
4. Add a `QMenuBar`, `QToolBar`. Add `Export CSV`, `Export JSON`, `Refresh` actions.
5. `m_statusLabel` can be added to the `QStatusBar`. `statusBar()->addWidget(m_statusLabel);`

Let's write a python script to do this refactor, or we can just apply diffs directly.

Changes in `WorkItemWindow.h`:
```cpp
<<<<<<< SEARCH
#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
#include <QNetworkReply>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QAction>
#include <QLabel>
#include <QJsonArray>
#include "GitHubClient.h"

class WorkItemWindow : public QDialog {
=======
#include <QMainWindow>
#include <QTableWidget>
#include <QPushButton>
#include <QNetworkReply>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QAction>
#include <QLabel>
#include <QJsonArray>
#include "GitHubClient.h"

class WorkItemWindow : public QMainWindow {
>>>>>>> REPLACE
```

In `WorkItemWindow.cpp`:
```cpp
<<<<<<< SEARCH
WorkItemWindow::WorkItemWindow(GitHubClient *client, const QString& windowTitle, EndpointType endpointType, const QString& baseQuery, QWidget *parent)
    : QDialog(parent), m_client(client), m_windowTitle(windowTitle), m_endpointType(endpointType), m_baseQuery(baseQuery), m_currentPage(1), m_manager(new QNetworkAccessManager(this))
=======
WorkItemWindow::WorkItemWindow(GitHubClient *client, const QString& windowTitle, EndpointType endpointType, const QString& baseQuery, QWidget *parent)
    : QMainWindow(parent), m_client(client), m_windowTitle(windowTitle), m_endpointType(endpointType), m_baseQuery(baseQuery), m_currentPage(1), m_manager(new QNetworkAccessManager(this))
>>>>>>> REPLACE
```

```cpp
// In setupUi:
<<<<<<< SEARCH
    resize(800, 600);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

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

    m_statusLabel = new QLabel(this);
    mainLayout->addWidget(m_statusLabel);
=======
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

    setCentralWidget(m_table);

    QMenuBar *menuBar = this->menuBar();
    QMenu *fileMenu = menuBar->addMenu(tr("&File"));
    QAction *exportCsvAction = new QAction(tr("Export to CSV"), this);
    connect(exportCsvAction, &QAction::triggered, this, &WorkItemWindow::exportToCsv);
    fileMenu->addAction(exportCsvAction);
    QAction *exportJsonAction = new QAction(tr("Export to JSON"), this);
    connect(exportJsonAction, &QAction::triggered, this, &WorkItemWindow::exportToJson);
    fileMenu->addAction(exportJsonAction);

    QMenu *viewMenu = menuBar->addMenu(tr("&View"));
    QAction *refreshAction = new QAction(tr("Refresh"), this);
    connect(refreshAction, &QAction::triggered, this, [this]() {
        loadData(1);
    });
    viewMenu->addAction(refreshAction);

    QToolBar *toolBar = addToolBar(tr("Main Toolbar"));
    toolBar->addAction(refreshAction);
    toolBar->addSeparator();
    toolBar->addAction(exportCsvAction);
    toolBar->addAction(exportJsonAction);

    m_statusLabel = new QLabel(this);
    statusBar()->addWidget(m_statusLabel);
>>>>>>> REPLACE
```

Wait, `statusBar()` requires `#include <QStatusBar>` and `menuBar()` requires `#include <QMenuBar>` and `addToolBar()` requires `#include <QToolBar>`.

```cpp
#include <QStatusBar>
#include <QMenuBar>
#include <QToolBar>
```
Wait, we need to add these includes.
Let's add them to `WorkItemWindow.h` or `.cpp`.

And update `m_statusLabel->setText` calls in `loadData`:
When saving cache or reporting completion:
"We should also put into the status bar how many pages (out of how many) were loaded."
```cpp
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
```

Wait, what if `totalCount` isn't available when we update status? It is available in `onReplyFinished`.
Wait, what about the cache loading? We don't have `totalCount` or `m_currentPage` (well we have `m_currentPage` which is 1 at that time but maxPages is unknown).
```cpp
        m_statusLabel->setText(tr("Cached data from %1 - Items: %2").arg(lastRefresh).arg(m_allData.size()));
```
This is fine.

Wait, `totalCount` variable needs to be saved to `m_lastTotalCount` if we want it stored, but here it's fine just within `onReplyFinished`.

Let's do this directly.
