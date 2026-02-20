#include <QTest>
#include <QSignalSpy>
#include <QSettings>
#include "../src/MainWindow.h"
#include "../src/GitHubClient.h"
#include "../src/SettingsDialog.h"
#include "../src/AuthErrorNotification.h"

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

    void testUpdateNotificationsEmpty() {
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
    }
};

QTEST_MAIN(TestMainWindow)
#include "TestMainWindow.moc"
