We don't need ui testing or network based testing.
Never disable github workflows unless specified.

## Code Structure & Ordering

To maintain consistency and reduce merge conflicts, please follow this ordering for class members in `.cpp` files:

1.  **Includes** (grouped by library/module)
2.  **Constants / Static Helpers**
3.  **Constructor / Destructor**
4.  **Public Methods**
5.  **Slots** (grouped by functionality: Tray, List, Toolbar, etc.)
6.  **Private Helpers** (Setup, Logic)

In header files (`.h`), group declarations similarly and use comments to separate sections.

## Never Nest Principle

Avoid deep nesting of `if/else` blocks. Use guard clauses (early returns) to handle edge cases and error conditions first. This makes the "happy path" of the function less indented and easier to read.

## Function Size & Complexity

Break down large functions into smaller, single-purpose helper functions. This improves readability and makes the code easier to test and maintain.

## Building and Compiling (Qt6 / KF6 Migration)
This project is strictly a Qt6 and KDE Frameworks 6 (KF6) only project.
If your environment does not have the required Qt6 or KF6 development headers, this repository uses a shared public KDE/Qt Debian rootfs for its isolated development and testing environment.
Do not use Docker for the development/test environment.
Do not use Qt5 or KF5 idioms, dependencies, or classes.

To build and test within the isolated environment, use the provided scripts:
1. Provision the shared KDE development rootfs:
   `.jules/bootstrap.sh`
2. Run build and test commands through the chroot wrapper:
   `.jules/run.sh cmake -S . -B build -G Ninja -DBUILD_TESTING=ON`
   `.jules/run.sh cmake --build build --parallel`
   `.jules/run.sh ctest --test-dir build --output-on-failure`


## GitHub Personal Access Tokens (PATs)
When token requirements change for API endpoints (Classic vs. Fine-grained scopes), update the corresponding hints in the UI (e.g., SettingsDialog, NewIssueDialog) and documentation (README.md) to reflect the new requirements.
