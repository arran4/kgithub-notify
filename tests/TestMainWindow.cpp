#include <QTest>
#include <QSignalSpy>
#include "../src/MainWindow.h"
#include "../src/GitHubClient.h"

class TestMainWindow : public QObject {
    Q_OBJECT

private slots:
    void testAuthErrorSwitch() {
        // MainWindow requires QApplication (created by QTEST_MAIN)
        MainWindow w;
        GitHubClient client;
        w.setClient(&client);

        // Initially should be on notification list (or at least index 0, which is notification list)
        // Wait, MainWindow constructor checks token. If token is empty, it might do something.
        // But let's assume default state.

        QStackedWidget* stack = w.getStackWidget();
        QVERIFY(stack != nullptr);
        QVERIFY(w.getNotificationList() != nullptr);
        QVERIFY(w.getErrorPage() != nullptr);

        // Initially notification list is visible
        QCOMPARE(stack->currentWidget(), w.getNotificationList());

        // Trigger Auth Error
        emit client.authError("Test Error");

        // Should switch to error page
        QCOMPARE(stack->currentWidget(), w.getErrorPage());
    }

    void testUpdateNotificationsSwitch() {
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
};

QTEST_MAIN(TestMainWindow)
#include "TestMainWindow.moc"
