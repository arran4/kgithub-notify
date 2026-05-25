1.  **Refactor CMakeLists.txt:** Add the new filter parser files `src/utils/FilterParser.h` and `src/utils/FilterParser.cpp` to the `kgithub-notify` executable target.
2.  **Integrate FilterParser into `RepoListWindow`:**
    *   Add a header include `#include "utils/FilterParser.h"` to `src/RepoListWindow.cpp`.
    *   Implement an inner class `RepoAccessor : public FilterDataAccessor` inside `RepoListWindow.cpp` (or as a private class in the header) to evaluate repository attributes:
        *   `fork`: `repo["fork"].toBool() ? "true" : "false"`
        *   `archived`: `repo["archived"].toBool() ? "true" : "false"`
        *   `name`: `repo["name"].toString()`
        *   `owner`: `repo["owner"]["login"].toString()`
        *   `visibility`: `repo["visibility"].toString()`
        *   `createdat`: `repo["created_at"].toString()`
        *   `updatedat`: `repo["updated_at"].toString()`
    *   Add a combo box (for preset filters) and a `QLineEdit` to the `RepoListWindow` toolbar to allow filtering (like kjules). Default the query to `fork:false AND archived:false`.
    *   In `addReposToTable(const QJsonArray& repos)`, before adding a repository to the table, check if the filter is set. If set, parse the filter query and call `ast->evaluate(accessor)` with the current repository's `RepoAccessor`. If it evaluates to false, skip adding the repository to the table.
    *   Connect the `textChanged` signal of the filter input to re-populate the table (`addReposToTable(m_allRepos)`).
    *   In the table setup, expand the columns. From `m_table->setColumnCount(8)` to `m_table->setColumnCount(11)`.
    *   Add headers: "Created", "Archived", "Fork".
    *   Populate the new columns in `addReposToTable`.
3.  **Adjust `GitHubClient::fetchUserRepos` API Call:**
    *   Currently, it hardcodes `sort=updated`. This is correct according to the prompt's `sort them by modified date` requirement. Ensure it stays that way.
4.  **Complete pre-commit steps:**
    *   Complete pre-commit steps to ensure proper testing, verification, review, and reflection are done.
5.  **Submit the code:**
    *   Once all tests pass, submit the change.
