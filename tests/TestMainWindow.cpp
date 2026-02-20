#include <QTest>
#include <QSignalSpy>
#include <QSettings>
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
};

QTEST_MAIN(TestMainWindow)
#include "TestMainWindow.moc"
