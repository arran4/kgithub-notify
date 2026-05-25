#ifndef FILTERPARSER_H
#define FILTERPARSER_H

#include <QDateTime>
#include <QList>
#include <QMap>
#include <QSharedPointer>
#include <QString>

class FilterDataAccessor {
public:
  virtual ~FilterDataAccessor() = default;
  virtual QString getValue(const QString &key) const = 0;
  virtual QList<QString> getAllValues() const = 0;
};

class ASTNode {
public:
  virtual ~ASTNode() = default;
  virtual bool evaluate(const FilterDataAccessor &accessor) const = 0;
  virtual QString toString() const = 0;
};

class AndNode : public ASTNode {
public:
  AndNode(QList<QSharedPointer<ASTNode>> children) : m_children(children) {}
  bool evaluate(const FilterDataAccessor &accessor) const override;
  QString toString() const override;
  QList<QSharedPointer<ASTNode>> children() const { return m_children; }

private:
  QList<QSharedPointer<ASTNode>> m_children;
};

class OrNode : public ASTNode {
public:
  OrNode(QList<QSharedPointer<ASTNode>> children) : m_children(children) {}
  bool evaluate(const FilterDataAccessor &accessor) const override;
  QString toString() const override;
  QList<QSharedPointer<ASTNode>> children() const { return m_children; }

private:
  QList<QSharedPointer<ASTNode>> m_children;
};

class NotNode : public ASTNode {
public:
  NotNode(QSharedPointer<ASTNode> child) : m_child(child) {}
  bool evaluate(const FilterDataAccessor &accessor) const override;
  QString toString() const override;
  QSharedPointer<ASTNode> child() const { return m_child; }

private:
  QSharedPointer<ASTNode> m_child;
};

class InNode : public ASTNode {
public:
  InNode(const QString &key, const QString &valuesStr)
      : m_key(key), m_valuesStr(valuesStr) {
    QStringList parts = valuesStr.split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (const QString &p : parts) {
      QString trimmed = p.trimmed();
      if (!trimmed.isEmpty()) {
        m_values.append(trimmed);
      }
    }
  }
  bool evaluate(const FilterDataAccessor &accessor) const override;
  QString toString() const override;
  QString key() const { return m_key; }
  QString valuesStr() const { return m_valuesStr; }

private:
  QString m_key;
  QString m_valuesStr;
  QStringList m_values;
};

class KeyValueNode : public ASTNode {
public:
  KeyValueNode(const QString &key, const QString &value)
      : m_key(key), m_value(value) {}
  bool evaluate(const FilterDataAccessor &accessor) const override;
  QString toString() const override;
  QString key() const { return m_key; }
  QString value() const { return m_value; }

private:
  QString m_key;
  QString m_value;
};

class KeywordNode : public ASTNode {
public:
  KeywordNode(const QString &keyword) : m_keyword(keyword) {}
  bool evaluate(const FilterDataAccessor &accessor) const override;
  QString toString() const override;
  QString keyword() const { return m_keyword; }

private:
  QString m_keyword;
};

class FilterParser {
public:
  static QSharedPointer<ASTNode> parse(const QString &query);
};

#endif // FILTERPARSER_H
