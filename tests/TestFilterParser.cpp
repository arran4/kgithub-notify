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
        if (lowerKey == "name" || lowerKey == "repo") return m_repo["name"].toString();
        if (lowerKey == "owner") return m_repo["owner"].toObject()["login"].toString();
        if (lowerKey == "visibility") return m_repo["visibility"].toString();
        if (lowerKey == "createdat" || lowerKey == "created" || lowerKey == "created-at" || lowerKey == "created_at")
            return m_repo["created_at"].toString();
        if (lowerKey == "updatedat" || lowerKey == "updated" || lowerKey == "updated-at" || lowerKey == "updated_at")
            return m_repo["updated_at"].toString();
        return "";
    }

    QList<QString> getAllValues() const override {
        return {m_repo["fork"].toBool() ? "true" : "false",
                m_repo["archived"].toBool() ? "true" : "false",
                m_repo["name"].toString(),
                m_repo["owner"].toObject()["login"].toString(),
                m_repo["visibility"].toString(),
                m_repo["created_at"].toString(),
                m_repo["updated_at"].toString()};
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

    void testParseDiagnostics() {
        // Unterminated quotes
        {
            FilterParseResult res = FilterParser::parseWithResult("\"unterminated");
            QVERIFY(!res.ok);
            QVERIFY(res.error.contains("Unterminated quote"));
            QVERIFY(res.errorPos >= 0);
        }
        {
            FilterParseResult res = FilterParser::parseWithResult("name:\"unterminated");
            QVERIFY(!res.ok);
            QVERIFY(res.error.contains("Unterminated quote"));
        }
        {
            FilterParseResult res = FilterParser::parseWithResult("'single unterminated");
            QVERIFY(!res.ok);
            QVERIFY(res.error.contains("Unterminated quote"));
        }
        {
            FilterParseResult res = FilterParser::parseWithResult("owner:'single unterminated");
            QVERIFY(!res.ok);
            QVERIFY(res.error.contains("Unterminated quote"));
        }

        // Unmatched parentheses
        {
            FilterParseResult res = FilterParser::parseWithResult("(fork:false");
            QVERIFY(!res.ok);
            QVERIFY(res.error.contains("Unmatched opening parenthesis"));
        }
        {
            FilterParseResult res = FilterParser::parseWithResult("fork:false)");
            QVERIFY(!res.ok);
            QVERIFY(res.error.contains("Unmatched closing parenthesis"));
        }
        {
            FilterParseResult res = FilterParser::parseWithResult("((fork:false)");
            QVERIFY(!res.ok);
            QVERIFY(res.error.contains("Unmatched opening parenthesis"));
        }
        {
            FilterParseResult res = FilterParser::parseWithResult("()");
            QVERIFY(!res.ok);
            QVERIFY(res.error.contains("Empty parentheses"));
        }
        {
            FilterParseResult res = FilterParser::parseWithResult(")");
            QVERIFY(!res.ok);
            QVERIFY(res.error.contains("Unmatched closing parenthesis"));
        }

        // Incomplete operands
        {
            FilterParseResult res = FilterParser::parseWithResult("fork:false AND");
            QVERIFY(!res.ok);
            QVERIFY(res.error.contains("Incomplete AND"));
        }
        {
            FilterParseResult res = FilterParser::parseWithResult("fork:false OR");
            QVERIFY(!res.ok);
            QVERIFY(res.error.contains("Incomplete OR"));
        }
        {
            FilterParseResult res = FilterParser::parseWithResult("NOT");
            QVERIFY(!res.ok);
            QVERIFY(res.error.contains("Incomplete NOT"));
        }
        {
            FilterParseResult res = FilterParser::parseWithResult("fork:false AND NOT");
            QVERIFY(!res.ok);
            QVERIFY(res.error.contains("Incomplete NOT"));
        }
        {
            FilterParseResult res = FilterParser::parseWithResult("owner IN");
            QVERIFY(!res.ok);
            QVERIFY(res.error.contains("Incomplete IN"));
        }
        {
            FilterParseResult res = FilterParser::parseWithResult("AND fork:false");
            QVERIFY(!res.ok);
            QVERIFY(res.error.contains("Missing operand"));
        }
        {
            FilterParseResult res = FilterParser::parseWithResult("OR fork:false");
            QVERIFY(!res.ok);
            QVERIFY(res.error.contains("Missing operand"));
        }
        {
            FilterParseResult res = FilterParser::parseWithResult("fork:");
            QVERIFY(!res.ok);
            QVERIFY(res.error.contains("Missing value for key"));
        }

        // Unexpected trailing tokens
        {
            FilterParseResult res = FilterParser::parseWithResult("(fork:false) )");
            QVERIFY(!res.ok);
            QVERIFY(res.error.contains("Unmatched closing parenthesis"));
        }
    }

    void testParseWithResultValid() {
        {
            FilterParseResult res = FilterParser::parseWithResult("");
            QVERIFY(res.ok);
            QVERIFY(res.ast.isNull());
            QVERIFY(res.error.isEmpty());
        }
        {
            FilterParseResult res = FilterParser::parseWithResult("   ");
            QVERIFY(res.ok);
            QVERIFY(res.ast.isNull());
        }
        {
            FilterParseResult res = FilterParser::parseWithResult("fork:false AND archived:false");
            QVERIFY(res.ok);
            QVERIFY(!res.ast.isNull());
            QVERIFY(res.error.isEmpty());
        }
        {
            FilterParseResult res = FilterParser::parseWithResult("= fork:false AND archived:false");
            QVERIFY(res.ok);
            QVERIFY(!res.ast.isNull());
        }
        {
            FilterParseResult res = FilterParser::parseWithResult("NOT NOT (fork:true OR name:\"test\")");
            QVERIFY(res.ok);
            QVERIFY(!res.ast.isNull());
        }
    }

    void testStructuredKeyDiagnostics() {
        // 1. Valid known keys
        const QStringList validKeys = {
            "name",       "repo",       "owner",          "fork",          "archived",       "visibility",
            "created",    "createdat",  "created-at",     "created_at",    "updated",        "updatedat",
            "updated-at", "updated_at", "created-before", "created-after", "updated-before", "updated-after",
            "FORK",       "Repo",       "Owner"};
        for (const QString& k : validKeys) {
            FilterParseResult res = FilterParser::parseWithResult(QStringLiteral("%1:testval").arg(k));
            QVERIFY2(res.ok,
                     qPrintable(QStringLiteral("Expected key '%1' to be valid, but got: %2").arg(k, res.error)));
            QVERIFY(res.error.isEmpty());
            QVERIFY(!res.ast.isNull());
        }

        // 2. Unknown structured keys
        {
            FilterParseResult res = FilterParser::parseWithResult("state:open");
            QVERIFY(!res.ok);
            QVERIFY(res.error.contains("Unknown filter key: 'state'"));
            QCOMPARE(res.errorPos, 0);
            QVERIFY(res.ast.isNull());
        }
        {
            FilterParseResult res = FilterParser::parseWithResult("foo:bar");
            QVERIFY(!res.ok);
            QVERIFY(res.error.contains("Unknown filter key: 'foo'"));
            QCOMPARE(res.errorPos, 0);
        }
        {
            FilterParseResult res = FilterParser::parseWithResult("unknown:\"quoted value\"");
            QVERIFY(!res.ok);
            QVERIFY(res.error.contains("Unknown filter key: 'unknown'"));
        }
        {
            FilterParseResult res = FilterParser::parseWithResult("invalid IN \"val1, val2\"");
            QVERIFY(!res.ok);
            QVERIFY(res.error.contains("Unknown filter key: 'invalid'"));
        }

        // 3. Unknown key mixed with AND/OR/NOT
        {
            FilterParseResult res = FilterParser::parseWithResult("name:foo AND state:open");
            QVERIFY(!res.ok);
            QVERIFY(res.error.contains("Unknown filter key: 'state'"));
            QCOMPARE(res.errorPos, 13);
        }
        {
            FilterParseResult res = FilterParser::parseWithResult("NOT (invalid:123 OR fork:false)");
            QVERIFY(!res.ok);
            QVERIFY(res.error.contains("Unknown filter key: 'invalid'"));
            QCOMPARE(res.errorPos, 5);
        }
        {
            FilterParseResult res = FilterParser::parseWithResult("state:open OR owner:bar");
            QVERIFY(!res.ok);
            QVERIFY(res.error.contains("Unknown filter key: 'state'"));
            QCOMPARE(res.errorPos, 0);
        }

        // 4. Quoted values for known keys
        {
            FilterParseResult res = FilterParser::parseWithResult("name:\"my awesome repo\"");
            QVERIFY(res.ok);
            QVERIFY(res.error.isEmpty());
        }
        {
            FilterParseResult res = FilterParser::parseWithResult("owner:'john doe'");
            QVERIFY(res.ok);
            QVERIFY(res.error.isEmpty());
        }
        {
            FilterParseResult res = FilterParser::parseWithResult("repo:\"quoted-repo\"");
            QVERIFY(res.ok);
            QVERIFY(res.error.isEmpty());
        }

        // 5. Free-text queries must NOT be rejected as unknown keys
        {
            FilterParseResult res = FilterParser::parseWithResult("awesome");
            QVERIFY(res.ok);
            QVERIFY(!res.ast.isNull());
        }
        {
            FilterParseResult res = FilterParser::parseWithResult("open");
            QVERIFY(res.ok);
            QVERIFY(!res.ast.isNull());
        }
        {
            FilterParseResult res = FilterParser::parseWithResult("state");
            QVERIFY(res.ok);
            QVERIFY(!res.ast.isNull());
        }
        {
            FilterParseResult res = FilterParser::parseWithResult("\"state:open\"");
            QVERIFY(res.ok);
            QVERIFY(!res.ast.isNull());
        }
        {
            FilterParseResult res = FilterParser::parseWithResult("NOT open");
            QVERIFY(res.ok);
            QVERIFY(!res.ast.isNull());
        }

        // 6. Direct evaluation guard test
        {
            QJsonObject repo;
            repo["name"] = "my-repo";
            TestRepoAccessor accessor(repo);

            KeyValueNode badKeyNode("bad_key", "val");
            QVERIFY(!badKeyNode.evaluate(accessor));

            InNode badInNode("bad_key", "val1, val2");
            QVERIFY(!badInNode.evaluate(accessor));
        }
    }
};

QTEST_MAIN(TestFilterParser)
#include "TestFilterParser.moc"
