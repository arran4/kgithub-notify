Menu creation in `MainWindow.cpp`:

```cpp
    QMenu *issuesMenu = toolsMenu->addMenu(tr("Open Issues")); // Wait, rename to "Issues"
```

The user requested: "The word "open" infront of pull requests and issues, needs to be moved into the filter as I also want to be able to see closed and other statuses. (merged, for prs)".

```cpp
    QMenu *issuesMenu = toolsMenu->addMenu(tr("Issues"));
    QMenu *prsMenu = toolsMenu->addMenu(tr("Pull Requests"));
    QMenu *reposMenu = toolsMenu->addMenu(tr("Repositories"));

    struct Variant {
        QString name;
        QString issueQuery;
        QString prQuery;
        QString repoQuery;
    };

    QList<Variant> variants = {
        {tr("Created by me"), "author:@me archived:false", "author:@me archived:false", "user:@me archived:false"},
        {tr("Assigned to me"), "assignee:@me archived:false", "assignee:@me archived:false", ""},
        {tr("I was mentioned in them"), "mentions:@me archived:false", "mentions:@me archived:false", ""},
        {tr("Review was requested"), "", "review-requested:@me archived:false", ""},
        {tr("Repos I have contributed to"), "involves:@me archived:false", "involves:@me archived:false", ""},
        {tr("My repos"), "user:@me archived:false", "user:@me archived:false", "user:@me archived:false"},
        {tr("My forks"), "user:@me fork:true archived:false", "user:@me fork:true archived:false", "user:@me fork:true archived:false"},
        {tr("Repos I have admin access to"), "user:@me archived:false", "user:@me archived:false", "user:@me archived:false"},
        {tr("Archived"), "archived:true involves:@me", "archived:true involves:@me", "archived:true user:@me"},
        {tr("All (Unfiltered)"), "involves:@me", "involves:@me", "user:@me"} // Fallback since github requires *some* query
    };

    auto createSubMenu = [&](QMenu* parentMenu, const QString& statusName, const QString& statusQuery, const QString& typeQuery, int endpointType) {
        QMenu* statusMenu = parentMenu;
        if (!statusName.isEmpty()) {
            statusMenu = parentMenu->addMenu(statusName);
        }

        for (const auto& v : variants) {
            QString vQuery;
            if (endpointType == 0) vQuery = v.issueQuery;
            else if (endpointType == 1) vQuery = v.prQuery;
            else vQuery = v.repoQuery;

            if (vQuery.isEmpty()) continue;

            QAction *action = new QAction(v.name, this);
            connect(action, &QAction::triggered, this, [=]() {
                QStringList queryParts;
                if (!typeQuery.isEmpty()) queryParts << typeQuery;
                if (!statusQuery.isEmpty()) queryParts << statusQuery;
                if (!vQuery.isEmpty()) queryParts << vQuery;

                QString finalQuery = queryParts.join(" ");

                QString fullTitle = parentMenu->title();
                if (!statusName.isEmpty()) fullTitle += " - " + statusName;
                fullTitle += " - " + v.name;

                int actualEndpoint = (endpointType == 2) ? 1 : 0; // 1 = repos, 0 = issues
                showWorkItems(fullTitle, actualEndpoint, finalQuery);
            });
            statusMenu->addAction(action);
        }
    };

    createSubMenu(issuesMenu, tr("Open"), "is:open", "is:issue", 0);
    createSubMenu(issuesMenu, tr("Closed"), "is:closed", "is:issue", 0);
    createSubMenu(issuesMenu, tr("All Statuses"), "", "is:issue", 0);

    createSubMenu(prsMenu, tr("Open"), "is:open", "is:pr", 1);
    createSubMenu(prsMenu, tr("Closed"), "is:closed", "is:pr", 1);
    createSubMenu(prsMenu, tr("Merged"), "is:merged", "is:pr", 1);
    createSubMenu(prsMenu, tr("All Statuses"), "", "is:pr", 1);

    createSubMenu(reposMenu, "", "", "", 2);
```

Wait, `actualEndpoint` passes `1` for repos, `0` for issues.
Let's modify `showWorkItems` signature:
```cpp
    void showWorkItems(const QString &title, int endpointType, const QString &query);
```
