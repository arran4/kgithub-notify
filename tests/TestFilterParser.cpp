#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

#include "../src/utils/FilterParser.h"

class TestRepoAccessor : public FilterDataAccessor {
   public:
    TestRepoAccessor(const QJsonObject& repo) : m_repo(repo) {}

    QString getValue(const QString& key) const override {
        QString lowerKey = key.toLower();
        if (lowerKey == "fork") return m_repo["fork"].toBool() ? "true" : "false";
        if (lowerKey == "archived") return m_repo["archived"].toBool() ? "true" : "false";
        if (lowerKey == "name") return m_repo["name"].toString();
        if (lowerKey == "owner") return m_repo["owner"].toObject()["login"].toString();
        return "";
    }

    QList<QString> getAllValues() const override {
        return {m_repo["fork"].toBool() ? "true" : "false", m_repo["archived"].toBool() ? "true" : "false",
                m_repo["name"].toString(), m_repo["owner"].toObject()["login"].toString()};
    }

   private:
    QJsonObject m_repo;
};

class TestFilterParser : public QObject {
    Q_OBJECT
   private slots:
    void testBasicFilter() {
        QJsonObject repo;
        repo["fork"] = false;
        repo["archived"] = false;
        repo["name"] = "test-repo";

        QSharedPointer<ASTNode> ast = FilterParser::parse("fork:false AND archived:false");
        QVERIFY(ast != nullptr);

        TestRepoAccessor accessor(repo);
        QVERIFY(ast->evaluate(accessor) == true);

        repo["fork"] = true;
        TestRepoAccessor accessor2(repo);
        QVERIFY(ast->evaluate(accessor2) == false);
    }

    void testToStringAndSimplification() {
        QSharedPointer<ASTNode> ast;

        // Test basic AND
        ast = FilterParser::parse("fork:false AND archived:false");
        QCOMPARE(ast->toString(), QString("(fork:false AND archived:false)"));

        // Test basic OR
        ast = FilterParser::parse("fork:false OR archived:false");
        QCOMPARE(ast->toString(), QString("(fork:false OR archived:false)"));

        // Test redundant brackets removal (simplification of AND within AND)
        ast = FilterParser::parse("(fork:false AND archived:false) AND name:foo");
        QCOMPARE(ast->toString(), QString("(fork:false AND archived:false AND name:foo)"));

        // Test redundant brackets removal (simplification of OR within OR)
        ast = FilterParser::parse("fork:false OR (archived:false OR name:foo)");
        QCOMPARE(ast->toString(), QString("(fork:false OR archived:false OR name:foo)"));

        // Test precedence: AND binds tighter than OR
        ast = FilterParser::parse("fork:false OR archived:false AND name:foo");
        QCOMPARE(ast->toString(), QString("(fork:false OR (archived:false AND name:foo))"));

        ast = FilterParser::parse("fork:false AND archived:false OR name:foo");
        QCOMPARE(ast->toString(), QString("((fork:false AND archived:false) OR name:foo)"));

        // Test implicit AND between keywords
        ast = FilterParser::parse("fork:false foo bar");
        QCOMPARE(ast->toString(), QString("(fork:false AND foo AND bar)"));

        // Test NOT
        ast = FilterParser::parse("NOT fork:true");
        QCOMPARE(ast->toString(), QString("NOT fork:true"));

        // Test IN
        ast = FilterParser::parse("owner IN \"user1, user2\"");
        QCOMPARE(ast->toString(), QString("owner IN \"user1, user2\""));

        // Test complex query
        ast = FilterParser::parse("repo:foo* AND (fork:false OR archived:false) NOT owner:bar");
        // Implicit ANDs connect these three chunks
        // Left: repo:foo* AND (fork:false OR archived:false) -> actually explicit AND
        // Right: NOT owner:bar
        // Output should be correctly parenthesized.
        QCOMPARE(ast->toString(), QString("(repo:foo* AND (fork:false OR archived:false) AND NOT owner:bar)"));

        // Edge cases
        ast = FilterParser::parse("NOT NOT fork:true");
        QCOMPARE(ast->toString(), QString("NOT NOT fork:true"));

        ast = FilterParser::parse("(((fork:false)))");
        QCOMPARE(ast->toString(), QString("fork:false"));

        ast = FilterParser::parse("((fork:false AND archived:false)) OR (name:foo)");
        QCOMPARE(ast->toString(), QString("((fork:false AND archived:false) OR name:foo)"));

        ast = FilterParser::parse("");
        QVERIFY(ast == nullptr);

        ast = FilterParser::parse("   ");
        QVERIFY(ast == nullptr);
    }

    void testEvaluationLogic() {
        QJsonObject repo;
        repo["fork"] = false;
        repo["archived"] = true;
        repo["name"] = "my-awesome-repo";
        repo["owner"] = QJsonObject{{"login", "john-doe"}};
        TestRepoAccessor accessor(repo);

        QVERIFY(FilterParser::parse("fork:false")->evaluate(accessor) == true);
        QVERIFY(FilterParser::parse("archived:true")->evaluate(accessor) == true);
        QVERIFY(FilterParser::parse("archived:false")->evaluate(accessor) == false);

        // Name wildcard
        QVERIFY(FilterParser::parse("name:my-*")->evaluate(accessor) == true);
        QVERIFY(FilterParser::parse("name:*repo")->evaluate(accessor) == true);
        QVERIFY(FilterParser::parse("name:foo")->evaluate(accessor) == false);

        // Owner exact
        QVERIFY(FilterParser::parse("owner:john-doe")->evaluate(accessor) == true);
        QVERIFY(FilterParser::parse("owner:john")->evaluate(accessor) == false);

        // Precedence: true OR false AND false -> true OR (false AND false) -> true
        QVERIFY(FilterParser::parse("fork:false OR archived:false AND name:foo")->evaluate(accessor) == true);

        // Precedence: false AND false OR true -> (false AND false) OR true -> true
        QVERIFY(FilterParser::parse("fork:true AND archived:false OR name:my-awesome-repo")->evaluate(accessor) ==
                true);

        // Implicit AND
        QVERIFY(FilterParser::parse("fork:false archived:true")->evaluate(accessor) == true);
        QVERIFY(FilterParser::parse("fork:false archived:false")->evaluate(accessor) == false);

        // Keyword text search
        QVERIFY(FilterParser::parse("awesome")->evaluate(accessor) == true);
        QVERIFY(FilterParser::parse("john")->evaluate(accessor) == true);  // matches owner substring in getAllValues
        QVERIFY(FilterParser::parse("terrible")->evaluate(accessor) == false);

        // NOT
        QVERIFY(FilterParser::parse("NOT awesome")->evaluate(accessor) == false);
        QVERIFY(FilterParser::parse("NOT terrible")->evaluate(accessor) == true);

        // Double NOT
        QVERIFY(FilterParser::parse("NOT NOT awesome")->evaluate(accessor) == true);

        // IN
        QVERIFY(FilterParser::parse("owner IN \"john-doe, jane-doe\"")->evaluate(accessor) == true);
        QVERIFY(FilterParser::parse("owner IN \"jim-doe, jane-doe\"")->evaluate(accessor) == false);
    }
};

QTEST_MAIN(TestFilterParser)
#include "TestFilterParser.moc"
