1. Edit `src/WalletManager.h` and `src/WalletManager.cpp` to implement `QFuture<QString> saveTokenAsync(QString)` and `QFuture<QString> clearTokenAsync()`. These will use `KWallet::Wallet::Asynchronous` and `QFutureInterface` to perform operations asynchronously and return an empty string on success, or an error message on failure. Remove synchronous `saveToken`.
2. Verify edits to `WalletManager` using `read_file`.
3. Edit `src/SettingsDialog.h` and `src/SettingsDialog.cpp` to use `QFutureWatcher` in `saveSettings` to wait for the result of `WalletManager::saveTokenAsync`. On failure, update `statusLabel` to display the error, do not close the dialog, and preserve the token edit text. On success, accept and close. Add a similar async flow for clearing the token.
4. Verify edits to `SettingsDialog` using `read_file`.
5. Edit `src/GitHubClient.h` to define a `TokenCapabilities` struct with enums/booleans (Available, Limited, Unknown, Unavailable) for Notifications, RepoMetadata, PrivateRepos, CreateIssues, PostComments. Update `tokenVerified` signal to pass this struct. Define a static `getPermissionGuidance()` string to centralize guidance.
6. Verify edits to `GitHubClient.h` using `read_file`.
7. Edit `src/GitHubClient.cpp` to modify `verifyToken` to sequentially check `/user`, `/user/repos?per_page=1`, and `/notifications?per_page=1`. Parse `X-OAuth-Scopes` to infer `TokenCapabilities`. Implement `getPermissionGuidance()`.
8. Verify edits to `GitHubClient.cpp` using `read_file`.
9. Edit `src/SettingsDialog.cpp` to update `onVerificationResult` to present a compact capabilities breakdown (Notifications, Repository Metadata, Issues/PR capabilities) using `statusLabel` formatted as HTML. Update guidance label to use `GitHubClient::getPermissionGuidance()`.
10. Verify edits to `SettingsDialog.cpp` using `read_file`.
11. Edit `README.md` to update the token permission guidance to match the text returned by `GitHubClient::getPermissionGuidance()`.
12. Verify edits to `README.md` using `read_file`.
13. Add specific test methods (e.g., `testVerifyClassicToken`, `testVerifyFineGrainedToken`) to `tests/TestGitHubClient.cpp` that mock the `/user` and `/notifications` endpoints and assert the corresponding boolean/enum values in the `TokenCapabilities` struct.
14. Verify edits to `tests/TestGitHubClient.cpp` using `read_file`.
15. Add a new test method (e.g., `testWalletPersistence`) to `tests/TestRequestConsumers.cpp` to verify the `SettingsDialog` UI state (e.g., `statusLabel` updates and button toggles) when `WalletManager::saveTokenAsync` and `clearTokenAsync` succeed or fail.
16. Verify edits to `tests/TestRequestConsumers.cpp` using `read_file`.
17. Build the project and run all tests using `.jules/bootstrap.sh && .jules/run.sh cmake -S . -B build -G Ninja -DBUILD_TESTING=ON && .jules/run.sh cmake --build build -j$(nproc)` and `.jules/run.sh bash -c 'cd build && QT_QPA_PLATFORM=offscreen ctest --output-on-failure'` to ensure correctness and prevent regressions.
18. Complete pre-commit steps to ensure proper testing, verification, review, and reflection are done.
19. Submit the changes via GitHub PR.
