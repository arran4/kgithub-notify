#include <QTest>
#include <QSignalSpy>
#include <QSettings>
#include <QMenuBar>
#include <QToolBar>
#include <QAction>
#include "../src/MainWindow.h"
#include "../src/GitHubClient.h"
#include "../src/SettingsDialog.h"
#include "../src/AuthErrorNotification.h"
#include "../src/NotificationItemWidget.h"

class TestMainWindow : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        QCoreApplication::setOrganizationName("Kgithub-notify-test");
        QCoreApplication::setApplicationName("Kgithub-notify-test");
    }

    void testInitialStateNoToken() {
        // Clear token
        QSettings settings;
        settings.setValue("token", "");

        MainWindow w;
        GitHubClient client;
        w.setClient(&client);

        QVERIFY(w.getLoginPage() != nullptr);
        // Should show login page when no token
        QCOMPARE(w.getStackWidget()->currentWidget(), w.getLoginPage());
    }

    void testInitialStateWithToken() {
        // Set token
        QSettings settings;
        settings.setValue("token", "dummy_token");

        MainWindow w;
        GitHubClient client;
        w.setClient(&client);

        QVERIFY(w.getNotificationList() != nullptr);
        // Should show notification list when token exists
        QCOMPARE(w.getStackWidget()->currentWidget(), w.getNotificationList());
    }

    void testAuthErrorSwitch() {
        // Ensure we start with token so we are on notification list
        QSettings settings;
        settings.setValue("token", "dummy_token");

        MainWindow w;
        GitHubClient client;
        w.setClient(&client);

        QCOMPARE(w.getStackWidget()->currentWidget(), w.getNotificationList());

        // Trigger Auth Error
        emit client.authError("Test Error");

        // Should switch to error page
        QCOMPARE(w.getStackWidget()->currentWidget(), w.getErrorPage());
    }

    void testUpdateNotificationsSwitch() {
        QSettings settings;
        settings.setValue("token", "dummy_token");

        MainWindow w;
        GitHubClient client;
        w.setClient(&client);

        // Force to error page first
        emit client.authError("Test Error");
        QCOMPARE(w.getStackWidget()->currentWidget(), w.getErrorPage());

        // Simulate success
        QList<Notification> notifications;
        Notification n;
        n.id = "1";
        n.title = "Test";
        notifications.append(n);
        emit client.notificationsReceived(notifications);

        // Should switch back to notification list
        QCOMPARE(w.getStackWidget()->currentWidget(), w.getNotificationList());
    }

    void testAuthNotificationShown() {
        QSettings settings;
        settings.setValue("token", "dummy_token");

        MainWindow w;
        GitHubClient client;
        w.setClient(&client);

        // Trigger Auth Error
        emit client.authError("Notification Test Error");

        // Verify notification is created and shown
        AuthErrorNotification *notif = w.getAuthNotification();
        QVERIFY(notif != nullptr);
        QVERIFY(notif->isVisible());
    }

    void testNotificationItemContent() {
        // Create dummy notification
        Notification n;
        n.id = "1";
        n.title = "Test Title";
        n.type = "Issue";
        n.repository = "owner/repo";
        n.url = "https://api.github.com/repos/owner/repo/issues/123";
        n.updatedAt = "2023-01-01T12:00:00Z";
        n.unread = true;

        QList<Notification> notifications;
        notifications.append(n);

        // Setup MainWindow
        QSettings settings;
        settings.setValue("token", "dummy_token");

        MainWindow w;
        GitHubClient client;
        w.setClient(&client);
        // Inject notifications
        w.updateNotifications(notifications);

        // Verify list item
        QListWidget *list = w.getNotificationList();
        QCOMPARE(list->count(), 1);

        QListWidgetItem *item = list->item(0);
        QVERIFY(item != nullptr);

        QWidget *widget = list->itemWidget(item);
        QVERIFY(widget != nullptr);

        NotificationItemWidget *itemWidget = qobject_cast<NotificationItemWidget*>(widget);
        QVERIFY(itemWidget != nullptr);

        // Verify content
        QCOMPARE(itemWidget->getTitle(), QString("Test Title"));
        QVERIFY(itemWidget->titleLabel->font().bold()); // Check bold for unread
        QCOMPARE(itemWidget->repoLabel->text(), QString("Repo: <b>owner/repo</b>"));
        QCOMPARE(itemWidget->typeLabel->text(), QString("Type: Issue"));

        QVERIFY(itemWidget->dateLabel->text().startsWith("Date: "));

        // URL check
        // "https://github.com/owner/repo/issues/123"
        QString expectedUrl = "https://github.com/owner/repo/issues/123";
        QCOMPARE(itemWidget->urlLabel->text(), expectedUrl);
    }
  
    void testTrayMenuStructure() {
        QSettings settings;
        settings.setValue("token", "dummy_token");

        MainWindow w;
        GitHubClient client;
        w.setClient(&client);

        // Verify initially (assuming dummy_token puts us in list)
        QCOMPARE(w.getStackWidget()->currentWidget(), w.getNotificationList());

        // Emit empty notifications
        QList<Notification> notifications;
        emit client.notificationsReceived(notifications);

        // Verify switch to empty state page
        QVERIFY(w.getEmptyStatePage() != nullptr);
        QCOMPARE(w.getStackWidget()->currentWidget(), w.getEmptyStatePage());
        QMenu *menu = w.getTrayIconMenu();
        QVERIFY(menu != nullptr);

        QList<QAction*> actions = menu->actions();
        // Open, Force Refresh, Settings, Separator, Quit
        QCOMPARE(actions.count(), 5);

        QCOMPARE(actions[0]->text(), "Open");
        QCOMPARE(actions[1]->text(), "Force Refresh");
        QCOMPARE(actions[2]->text(), "Settings");
        QVERIFY(actions[3]->isSeparator());
        QCOMPARE(actions[4]->text(), "Quit");
    }

    void testShortcuts() {
        MainWindow w;

        // Toolbar actions
        QToolBar *toolbar = w.findChild<QToolBar*>();
        QVERIFY(toolbar != nullptr);

        QList<QAction*> actions = toolbar->actions();

        QAction *refreshAction = nullptr;
        QAction *selectAllAction = nullptr;
        QAction *selectNoneAction = nullptr;
        QAction *selectTop10Action = nullptr;
        QAction *dismissSelectedAction = nullptr;
        QAction *openSelectedAction = nullptr;

        for (QAction *action : actions) {
            if (action->text() == "Refresh") refreshAction = action;
            else if (action->text() == "Select All") selectAllAction = action;
            else if (action->text() == "Select None") selectNoneAction = action;
            else if (action->text() == "Top 10") selectTop10Action = action;
            else if (action->text() == "Dismiss Selected") dismissSelectedAction = action;
            else if (action->text() == "Open Selected") openSelectedAction = action;
        }

        QVERIFY(refreshAction);
        QCOMPARE(refreshAction->shortcut(), QKeySequence::Refresh);

        QVERIFY(selectAllAction);
        QCOMPARE(selectAllAction->shortcut(), QKeySequence::SelectAll);

        QVERIFY(selectNoneAction);
        QCOMPARE(selectNoneAction->shortcut(), QKeySequence("Ctrl+Shift+A"));

        QVERIFY(selectTop10Action);
        QCOMPARE(selectTop10Action->shortcut(), QKeySequence("Ctrl+1"));

        QVERIFY(dismissSelectedAction);
        QCOMPARE(dismissSelectedAction->shortcut(), QKeySequence::Delete);

        QVERIFY(openSelectedAction);
        QCOMPARE(openSelectedAction->shortcut(), QKeySequence(Qt::Key_Return));

        // File Menu
        QMenuBar *menuBar = w.menuBar();
        QMenu *fileMenu = nullptr;
        for (QAction* action : menuBar->actions()) {
            if (action->menu() && action->menu()->title() == "&File") {
                fileMenu = action->menu();
                break;
            }
        }
        QVERIFY(fileMenu);

        QAction *quitAction = nullptr;
        for (QAction *action : fileMenu->actions()) {
            if (action->text() == "&Quit") {
                quitAction = action;
                break;
            }
        }

        QVERIFY(quitAction);
        QCOMPARE(quitAction->shortcut(), QKeySequence::Quit);
    }
};

QTEST_MAIN(TestMainWindow)
#include "TestMainWindow.moc"
