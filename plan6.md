The user wants to refactor the menus into a more structured way:
"What I thought I was suggesting was having a list: [Assigned to me, applies to [issues, pull requests], ....] Like data structure, and keeping the submenus for both. some of these apply to "repos" as well so let's expand into that, we also need an "all" option which is unfiltered. The word "open" infront of pull requests and issues, needs to be moved into the filter as I also want to be able to see closed and other statuses. (merged, for prs) we may need to think through the data format of the "what & filters & properties; where" and refactor it so we can show this complexity."

So the menu structure should be:
Tools ->
  Issues ->
    Open ->
      Created by me
      Assigned to me
      Mentioned in
      ...
      All (Unfiltered)
    Closed ->
      Created by me
      ...
    All Statuses ->
      ...
  Pull Requests ->
    Open ->
      ...
    Closed ->
      ...
    Merged ->
      ...
    All Statuses ->
      ...
  Repositories ->
    Created by me
    ...
    All Statuses (Actually repos don't have open/closed/merged like this, maybe just one level?)

Wait, creating submenus dynamically for properties and statuses is exactly what's needed.
Let's define a data structure:

```cpp
enum Target {
    Issues = 1 << 0,
    PullRequests = 1 << 1,
    Repositories = 1 << 2
};

struct StatusFilter {
    QString name;
    QString issueQuery; // e.g., "is:open"
    QString prQuery;
    QString repoQuery;
};

struct Variant {
    QString name;
    QString query;
    int targets; // Bitmask of Target
};
```

Let's look at the menus:
Issues
  Open
  Closed
  All Statuses
Pull Requests
  Open
  Closed
  Merged
  All Statuses
Repositories
  All Statuses (?)

Wait, `WorkItemWindow` uses the GitHub Search API for Issues (`https://api.github.com/search/issues?q=...`). Can we search repositories there?
No, searching repositories is `https://api.github.com/search/repositories?q=...`.
Our `WorkItemWindow` `loadData()` hardcodes the URL:
`QUrl url("https://api.github.com/search/issues?q=" + QUrl::toPercentEncoding(m_baseQuery) + "&per_page=100&page=" + QString::number(m_currentPage));`

If we want to support Repositories, we'd need to change `WorkItemWindow` to handle different search endpoints (`search/issues` vs `search/repositories`), and different JSON parsing (`items` array is the same, but `pull_request` field is missing, state is missing, etc. Actually Repositories don't have "state", they have "fork", "private", etc.).
Wait, do they mean "repos" as in a top-level menu? "some of these apply to 'repos' as well so let's expand into that".
If we do need to support repos in `WorkItemWindow`, we should add an `enum WindowType { SearchIssues, SearchRepos }` or pass the URL base explicitly.
Passing the URL base explicitly is easiest!
```cpp
explicit WorkItemWindow(GitHubClient *client, const QString& windowTitle, const QString& searchEndpoint, const QString& baseQuery, QWidget *parent = nullptr);
```
`searchEndpoint` would be `search/issues` or `search/repositories`.
And in `appendRow`, we can dynamically populate columns depending on what fields are present.
Wait, `WorkItemWindow` sets fixed columns: "Repository", "Title", "State", "Author", "Created At".
For repositories, we'd have "Repository Name" (full_name), "Description", "Language", "Owner", "Created At".
It might be better to just keep `WorkItemWindow` for Issues/PRs and maybe make a new one for Repos? Or just dynamically set columns based on the type.

Actually, the user already mentioned earlier: "My repos", "My forks", "Repos I have admin access to", "Repos I have contributed to". All these are repository searches if we take "repos" literally, but earlier I mapped them to `is:issue` queries. Now the user says "some of these apply to 'repos' as well so let's expand into that". So yes, we should support searching Repositories directly.

Let's restructure `MainWindow.cpp`:
```cpp
    struct StatusFilter {
        QString name;
        QString query; // Applied to issues/prs
    };

    QList<StatusFilter> issueStatuses = {
        {tr("Open"), "is:open"},
        {tr("Closed"), "is:closed"},
        {tr("All Statuses"), ""}
    };

    QList<StatusFilter> prStatuses = {
        {tr("Open"), "is:open"},
        {tr("Closed"), "is:closed"},
        {tr("Merged"), "is:merged"},
        {tr("All Statuses"), ""}
    };

    struct Variant {
        QString name;
        QString query; // Applied to all types
        bool appliesToIssues;
        bool appliesToPrs;
        bool appliesToRepos;
    };

    QList<Variant> variants = {
        {tr("Created by me"), "author:@me archived:false", true, true, false}, // author:@me doesn't work for repos, repos use user:@me
        {tr("Assigned to me"), "assignee:@me archived:false", true, true, false},
        {tr("I was mentioned in them"), "mentions:@me archived:false", true, true, false},
        {tr("Review was requested"), "review-requested:@me archived:false", false, true, false},
        {tr("Repos I have contributed to"), "involves:@me archived:false", true, true, false}, // involves doesn't work for repos
        {tr("My repos"), "user:@me archived:false", true, true, true},
        {tr("My forks"), "user:@me fork:true archived:false", true, true, true},
        {tr("Repos I have admin access to"), "user:@me archived:false", true, true, true}, // Admin proxy
        {tr("Archived"), "archived:true", true, true, true},
        {tr("All"), "", true, true, true} // Unfiltered option
    };
```
Wait, if `user:@me` is used for issues/PRs, it searches issues/PRs inside the user's repos. For repositories, `user:@me` searches the user's repos. So `user:@me` works for both!
But `author:@me` doesn't work for repos. For repos, the creator is usually the user, so `user:@me`.

Let's handle `WorkItemWindow` dynamically accepting columns, or we just pass the `Type` (Issues vs Repositories) so `WorkItemWindow` knows how to parse and display.

```cpp
// In WorkItemWindow.h
enum EndpointType {
    EndpointIssues,
    EndpointRepositories
};
explicit WorkItemWindow(GitHubClient *client, const QString& windowTitle, EndpointType type, const QString& baseQuery, QWidget *parent = nullptr);
```

```cpp
// In WorkItemWindow.cpp setupUi:
if (m_endpointType == EndpointIssues) {
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels({tr("Repository"), tr("Title"), tr("State"), tr("Author"), tr("Created At")});
} else {
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels({tr("Repository"), tr("Description"), tr("Language"), tr("Owner"), tr("Created At")});
}
```

```cpp
// In appendRow:
if (m_endpointType == EndpointIssues) {
    // ... parse as issue ...
} else {
    // parse as repo
    QString fullName = item["full_name"].toString();
    QString description = item["description"].toString();
    QString language = item["language"].toString();
    QString owner = item["owner"].toObject()["login"].toString();
    QString createdAt = item["created_at"].toString();
    QString htmlUrl = item["html_url"].toString();

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
```

```cpp
// In loadData:
QString endpointStr = (m_endpointType == EndpointIssues) ? "issues" : "repositories";
QUrl url("https://api.github.com/search/" + endpointStr + "?q=" + QUrl::toPercentEncoding(m_baseQuery) + "&per_page=100&page=" + QString::number(m_currentPage));
```

```cpp
// In getHtmlUrlForRow:
QTableWidgetItem *item = m_table->item(row, (m_endpointType == EndpointIssues) ? 1 : 0);
```

Let's map this in `MainWindow.cpp`:
```cpp
    void showWorkItems(const QString &title, int endpointType, const QString &query);
```

Menu structure:
```
Tools
  Issues
    Open
      Created by me
      ...
    Closed
      ...
    All Statuses
      ...
  Pull Requests
    Open
      ...
    Closed
      ...
    Merged
      ...
    All Statuses
      ...
  Repositories
    Created by me
    ...
```

Let's prepare the variants:
```cpp
    struct Variant {
        QString name;
        QString issueQuery;
        QString prQuery;
        QString repoQuery;
    };

    QList<Variant> variants = {
        {tr("Created by me"), "author:@me archived:false", "author:@me archived:false", "user:@me archived:false"}, // For repos, "created by me" means my repos
        {tr("Assigned to me"), "assignee:@me archived:false", "assignee:@me archived:false", ""}, // doesn't apply to repos
        {tr("I was mentioned in them"), "mentions:@me archived:false", "mentions:@me archived:false", ""},
        {tr("Review was requested"), "", "review-requested:@me archived:false", ""},
        {tr("Repos I have contributed to"), "involves:@me archived:false", "involves:@me archived:false", ""},
        {tr("My repos"), "user:@me archived:false", "user:@me archived:false", "user:@me archived:false"},
        {tr("My forks"), "user:@me fork:true archived:false", "user:@me fork:true archived:false", "user:@me fork:true archived:false"},
        {tr("Repos I have admin access to"), "user:@me archived:false", "user:@me archived:false", "user:@me archived:false"}, // Proxy
        {tr("Archived"), "archived:true involves:@me", "archived:true involves:@me", "archived:true user:@me"},
        {tr("All"), "", "", ""}
    };
```

Wait, the user wants "All" which is unfiltered.
If "All" has `""` as query, for issues it will be `is:issue`, which is huge. But GitHub search requires at least one filter or query string.
For `All`, maybe we should just query `user:@me` or `involves:@me` so it limits to the user. An empty query returns 422 Unprocessable Entity.
Let's make "All" use `involves:@me` for issues/PRs and `user:@me` for repos.

```cpp
        {tr("All (User related)"), "involves:@me", "involves:@me", "user:@me"}
```

Menu generation logic:
```cpp
    QMenu *issuesMenu = toolsMenu->addMenu(tr("Issues"));
    QMenu *prsMenu = toolsMenu->addMenu(tr("Pull Requests"));
    QMenu *reposMenu = toolsMenu->addMenu(tr("Repositories"));

    auto createSubMenu = [&](QMenu* parentMenu, const QString& statusName, const QString& statusQuery, const QString& typeQuery, int endpointType) {
        QMenu* statusMenu = parentMenu;
        if (!statusName.isEmpty()) {
            statusMenu = parentMenu->addMenu(statusName);
        }

        for (const auto& v : variants) {
            QString vQuery;
            if (endpointType == 0) { // Issues
                vQuery = v.issueQuery;
            } else if (endpointType == 1) { // PRs
                vQuery = v.prQuery;
            } else { // Repos
                vQuery = v.repoQuery;
            }

            if (vQuery.isEmpty() && v.name != tr("All")) continue; // Doesn't apply

            QAction *action = new QAction(v.name, this);
            connect(action, &QAction::triggered, this, [=]() {
                QString finalQuery = typeQuery;
                if (!statusQuery.isEmpty()) finalQuery += " " + statusQuery;
                if (!vQuery.isEmpty()) finalQuery += " " + vQuery;

                QString fullTitle = parentMenu->title();
                if (!statusName.isEmpty()) fullTitle += " - " + statusName;
                fullTitle += " - " + v.name;

                int actualEndpoint = (endpointType == 2) ? 1 : 0; // 0 = Issues, 1 = Repositories
                showWorkItems(fullTitle, actualEndpoint, finalQuery.trimmed());
            });
            statusMenu->addAction(action);
        }
    };

    // Issues
    createSubMenu(issuesMenu, tr("Open"), "is:open", "is:issue", 0);
    createSubMenu(issuesMenu, tr("Closed"), "is:closed", "is:issue", 0);
    createSubMenu(issuesMenu, tr("All Statuses"), "", "is:issue", 0);

    // PRs
    createSubMenu(prsMenu, tr("Open"), "is:open", "is:pr", 1);
    createSubMenu(prsMenu, tr("Closed"), "is:closed", "is:pr", 1);
    createSubMenu(prsMenu, tr("Merged"), "is:merged", "is:pr", 1);
    createSubMenu(prsMenu, tr("All Statuses"), "", "is:pr", 1);

    // Repos
    createSubMenu(reposMenu, "", "", "", 2);
```

Wait, `EndpointType` in `WorkItemWindow`:
```cpp
enum EndpointType {
    EndpointIssues,
    EndpointRepositories
};
```

Let's do the refactoring.
