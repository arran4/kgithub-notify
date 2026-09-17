#include <QtTest>

#include "../src/utils/UrlHelper.h"

class TestUrlHelper : public QObject {
    Q_OBJECT
   private slots:
    void testSafeWebUrls() {
        QVERIFY(UrlHelper::isSafeWebUrl("https://github.com/arran4/kgithub-notify"));
        QVERIFY(UrlHelper::isSafeWebUrl("http://example.com/path?arg=val#hash"));
        QVERIFY(UrlHelper::isSafeWebUrl(QUrl("https://api.github.com/repos")));
    }

    void testRejectEmptyAndMalformed() {
        QVERIFY(!UrlHelper::isSafeWebUrl(""));
        QVERIFY(!UrlHelper::isSafeWebUrl("   "));
        QVERIFY(!UrlHelper::isSafeWebUrl("not a url"));
        QVERIFY(!UrlHelper::isSafeWebUrl(QUrl()));
        QVERIFY(!UrlHelper::isSafeWebUrl("https://"));
        QVERIFY(!UrlHelper::isSafeWebUrl("http://"));
    }

    void testRejectLocalAndFileSchemes() {
        QVERIFY(!UrlHelper::isSafeWebUrl("file:///etc/passwd"));
        QVERIFY(!UrlHelper::isSafeWebUrl("file:///bin/sh"));
        QVERIFY(!UrlHelper::isSafeWebUrl("/usr/bin/bash"));
        QVERIFY(!UrlHelper::isSafeWebUrl("C:\\Windows\\explorer.exe"));
    }

    void testRejectCustomAndDangerousSchemes() {
        QVERIFY(!UrlHelper::isSafeWebUrl("javascript:alert(1)"));
        QVERIFY(!UrlHelper::isSafeWebUrl("data:text/html,<b>hello</b>"));
        QVERIFY(!UrlHelper::isSafeWebUrl("ssh://git@github.com"));
        QVERIFY(!UrlHelper::isSafeWebUrl("ftp://ftp.example.com"));
        QVERIFY(!UrlHelper::isSafeWebUrl("mailto:user@example.com"));
        QVERIFY(!UrlHelper::isSafeWebUrl("about:blank"));
    }

    void testOpenUrlRejection() {
        QVERIFY(!UrlHelper::openUrl(""));
        QVERIFY(!UrlHelper::openUrl("file:///etc/passwd"));
        QVERIFY(!UrlHelper::openUrl("javascript:evil()"));
    }
};

QTEST_MAIN(TestUrlHelper)
#include "TestUrlHelper.moc"
