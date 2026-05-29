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
};

QTEST_MAIN(TestFilterParser)
#include "TestFilterParser.moc"
