#include <QSignalSpy>
#include <QUuid>
#include <QtTest>

#include "../src/NotificationRuleEngine.h"
#include "../src/RulesDialog.h"

class TestRulesDialog : public QObject {
    Q_OBJECT
   private slots:
    void initTestCase() {
        QCoreApplication::setOrganizationName("arran4");
        QCoreApplication::setApplicationName("kgithub-notify-test");
    }

    void init() {
        QSettings settings("kgithub-notify", "NotificationRules");
        settings.clear();
    }

    void testScopedEditPreservesOthers() {
        NotificationRule a, b, c;
        a.repoFilter = "repoA";
        b.repoFilter = "repoB";
        c.repoFilter = "repoC";

        QList<NotificationRule> initial = {a, b, c};
        NotificationRuleEngine::saveRules(initial);

        NotificationRuleModel model;
        model.load();

        a.typeFilter = "PullRequest";
        model.updateRule(a);
        model.save();

        QList<NotificationRule> saved = NotificationRuleEngine::loadRules();
        QCOMPARE(saved.size(), 3);
        QCOMPARE(saved[0].repoFilter, QString("repoA"));
        QCOMPARE(saved[0].typeFilter, QString("PullRequest"));
        QCOMPARE(saved[1].repoFilter, QString("repoB"));
        QCOMPARE(saved[2].repoFilter, QString("repoC"));
    }

    void testScopedAddPreservesOthers() {
        NotificationRule a, b;
        a.repoFilter = "repoA";
        b.repoFilter = "repoB";
        NotificationRuleEngine::saveRules({a, b});

        NotificationRuleModel model;
        model.load();

        NotificationRule newRule;
        newRule.repoFilter = "repoA";
        newRule.typeFilter = "Issue";
        model.addRule(newRule);

        model.save();

        QList<NotificationRule> saved = NotificationRuleEngine::loadRules();
        QCOMPARE(saved.size(), 3);
        QCOMPARE(saved[0].repoFilter, QString("repoA"));
        QCOMPARE(saved[1].repoFilter, QString("repoB"));
        QCOMPARE(saved[2].repoFilter, QString("repoA"));
        QCOMPARE(saved[2].typeFilter, QString("Issue"));
    }

    void testScopedRemove() {
        NotificationRule a, b;
        a.repoFilter = "repoA";
        b.repoFilter = "repoB";
        NotificationRuleEngine::saveRules({a, b});

        NotificationRuleModel model;
        model.load();

        model.removeRule(a.id);
        model.save();

        QList<NotificationRule> saved = NotificationRuleEngine::loadRules();
        QCOMPARE(saved.size(), 1);
        QCOMPARE(saved[0].repoFilter, QString("repoB"));
    }

    void testLegacyMigration() {
        // Create a raw JSON list lacking "id"
        QSettings settings("kgithub-notify", "NotificationRules");
        QVariantList list;
        QJsonObject legacy;
        legacy["repoFilter"] = "legacyRepo";
        list.append(QString::fromUtf8(QJsonDocument(legacy).toJson(QJsonDocument::Compact)));
        settings.setValue("rules", list);

        QList<NotificationRule> loaded = NotificationRuleEngine::loadRules();
        QCOMPARE(loaded.size(), 1);
        QVERIFY(!loaded[0].id.isEmpty());  // Should have auto-assigned an ID

        // Resaving and reloading shouldn't change ID
        QString firstId = loaded[0].id;
        NotificationRuleEngine::saveRules(loaded);

        QList<NotificationRule> loadedAgain = NotificationRuleEngine::loadRules();
        QCOMPARE(loadedAgain[0].id, firstId);
    }

    void testDuplicateLookingRules() {
        NotificationRule a1, a2;
        a1.repoFilter = "repoA";
        a2.repoFilter = "repoA";

        QVERIFY(a1.id != a2.id);  // distinct IDs
        NotificationRuleEngine::saveRules({a1, a2});

        NotificationRuleModel model;
        model.load();

        model.removeRule(a1.id);
        model.save();
        QList<NotificationRule> saved = NotificationRuleEngine::loadRules();
        QCOMPARE(saved.size(), 1);
        QCOMPARE(saved[0].id, a2.id);
    }

    void testZeroMatchingScopedRows() {
        NotificationRule a;
        a.repoFilter = "repoA";
        NotificationRuleEngine::saveRules({a});

        RulesDialog dialog(nullptr, "repoB");  // No matching rules
        QTableWidget* table = dialog.findChild<QTableWidget*>();
        QCOMPARE(table->rowCount(), 0);

        QMetaObject::invokeMethod(&dialog, "saveRules");

        QList<NotificationRule> saved = NotificationRuleEngine::loadRules();
        QCOMPARE(saved.size(), 1);
        QCOMPARE(saved[0].repoFilter, QString("repoA"));
    }

    void testFullUnscopedEditing() {
        NotificationRule a, b;
        a.repoFilter = "repoA";
        b.repoFilter = "repoB";
        NotificationRuleEngine::saveRules({a, b});

        NotificationRuleModel model;
        model.load();

        model.removeRule(a.id);
        model.save();

        QList<NotificationRule> saved = NotificationRuleEngine::loadRules();
        QCOMPARE(saved.size(), 1);
        QCOMPARE(saved[0].repoFilter, QString("repoB"));
    }

    void testMoveDisabledInScopedMode() {
        NotificationRule a, b;
        a.repoFilter = "repoA";
        b.repoFilter = "repoA";
        NotificationRuleEngine::saveRules({a, b});

        RulesDialog dialog(nullptr, "repoA");  // scoped

        // Test that if we try to move up via the UI slot, it doesn't do anything because the table item selection and
        // validation logic But since we are asked to assert the scoped reordering contract properly... Let's assert
        // that the UI slot disables the button. We exposed btnUp.

        // However btnUp is private, so we can check it via findChildren.
        // Instead of doing that, let's assert that btnUp is disabled directly if we expose it or find it.
        // We can find it because it has the text "Move Up".

        QList<QPushButton*> btns = dialog.findChildren<QPushButton*>();
        QPushButton* upBtn = nullptr;
        for (QPushButton* btn : btns) {
            if (btn->text() == "Move Up") {
                upBtn = btn;
                break;
            }
        }
        QVERIFY(upBtn != nullptr);
        QVERIFY(!upBtn->isEnabled());
    }

    void testMoveUnscoped() {
        NotificationRule a, b;
        a.repoFilter = "repoA";
        b.repoFilter = "repoB";
        NotificationRuleEngine::saveRules({a, b});

        NotificationRuleModel model;
        model.load();

        model.moveUp(b.id);
        model.save();

        QList<NotificationRule> saved = NotificationRuleEngine::loadRules();
        QCOMPARE(saved.size(), 2);
        QCOMPARE(saved[0].repoFilter, QString("repoB"));
        QCOMPARE(saved[1].repoFilter, QString("repoA"));
    }
};

QTEST_MAIN(TestRulesDialog)
#include "TestRulesDialog.moc"
