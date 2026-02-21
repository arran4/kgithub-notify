#include <QTest>
#include "../src/NotificationTextFormatter.h"

class TestNotificationTextFormatter : public QObject {
    Q_OBJECT

private slots:
    void testHtmlInjection() {
        Notification n;
        n.title = "<b>Title</b>";
        n.type = "<b>Type</b>";
        n.repository = "<b>Repo</b>";
        n.url = "http://example.com";
        n.updatedAt = "2023-01-01T00:00:00Z";
        n.unread = true;

        // Check Repo
        // Expected: "Repo: <b>" + escaped(n.repository) + "</b>"
        // escaped("<b>Repo</b>") -> "&lt;b&gt;Repo&lt;/b&gt;"
        TextWithFormat repoData = NotificationTextFormatter::formatRepo(n);
        QCOMPARE(repoData.format, Qt::RichText);
        QString expectedRepoText = "Repo: <b>&lt;b&gt;Repo&lt;/b&gt;</b>";
        QCOMPARE(repoData.text, expectedRepoText);

        // Check Type
        // Expected: "Type: " + n.type (plain text label handles display)
        TextWithFormat typeData = NotificationTextFormatter::formatType(n);
        QCOMPARE(typeData.format, Qt::PlainText);
        QString expectedTypeText = "Type: <b>Type</b>";
        QCOMPARE(typeData.text, expectedTypeText);

        // Check Title
        TextWithFormat titleData = NotificationTextFormatter::formatTitle(n);
        QCOMPARE(titleData.format, Qt::PlainText);
        QCOMPARE(titleData.text, n.title);

        // Check URL
        TextWithFormat urlData = NotificationTextFormatter::formatUrl(n);
        QCOMPARE(urlData.format, Qt::RichText);
        QString expectedUrlText = QString("<a href=\"%1\">Open on GitHub</a>").arg(n.url.toHtmlEscaped());
        QCOMPARE(urlData.text, expectedUrlText);
    }
};

QTEST_MAIN(TestNotificationTextFormatter)
#include "TestNotificationTextFormatter.moc"
