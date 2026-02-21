#include <QHBoxLayout>
#include <QLabel>
#include <QTest>
#include <QVBoxLayout>

#include "../src/GitHubClient.h"
#include "../src/NotificationItemWidget.h"

class TestNotificationWidget : public QObject {
    Q_OBJECT

private slots:
    void testHtmlInjection() {
        Notification n;
        n.title = "<b>Title</b>";
        n.type = "<b>Type</b>";
        n.repository = "<b>Repo</b>";
        n.url = "http://example.com";
        n.htmlUrl = "http://example.com";
        n.updatedAt = "2023-01-01T00:00:00Z";
        n.unread = true;

        NotificationItemWidget widget(n);

        // Find all QLabels
        QList<QLabel*> labels = widget.findChildren<QLabel*>();

        QLabel* repoLabel = nullptr;
        QLabel* typeLabel = nullptr;
        QLabel* titleLabel = nullptr;
        QLabel* urlLabel = nullptr;

        for (QLabel* label : labels) {
            QString text = label->text();
            if (text.startsWith("Repo: ")) {
                repoLabel = label;
            } else if (text.startsWith("Type: ")) {
                typeLabel = label;
            } else if (text == n.title) {
                titleLabel = label;
            } else if (text.contains("example.com")) {
                urlLabel = label;
            }
        }

        QVERIFY2(repoLabel != nullptr, "Repo label not found");
        QVERIFY2(typeLabel != nullptr, "Type label not found");
        // titleLabel and urlLabel might be found or not depending on processing

        // Check Repo Label
        // Current Vulnerable behavior: text is "Repo: <b><b>Repo</b></b>"
        // Expected Fix: text should contain escaped HTML
        // "&lt;b&gt;Repo&lt;/b&gt;"
        QString repoText = repoLabel->text();

        // This assertion will FAIL if the code is vulnerable (it contains
        // <b>Repo</b>) And PASS if the code is fixed (it contains
        // &lt;b&gt;Repo&lt;/b&gt;) Actually, to make the test FAIL on
        // vulnerability, we assert the SAFE condition.
        QVERIFY2(
            repoText.contains("&lt;b&gt;Repo&lt;/b&gt;"),
            qPrintable(
                QString("Repository name should be HTML escaped. Actual: %1")
                    .arg(repoText)));

        // Check Type Label
        // Vulnerable: AutoText (default)
        // Fixed: PlainText
        QCOMPARE(typeLabel->textFormat(), Qt::PlainText);

        // Check Title Label
        if (titleLabel) {
            QCOMPARE(titleLabel->textFormat(), Qt::PlainText);
        }

        // Check URL Label
        if (urlLabel) {
            // URL label should also be PlainText
            QCOMPARE(urlLabel->textFormat(), Qt::PlainText);
        }
    }
};

QTEST_MAIN(TestNotificationWidget)
#include "TestNotificationWidget.moc"
