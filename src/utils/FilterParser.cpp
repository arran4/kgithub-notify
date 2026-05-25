#include "FilterParser.h"

#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QRegularExpressionMatchIterator>
#include <QStringList>
#include <QtGlobal>

struct Token {
    enum Type { LPAREN, RPAREN, AND, OR, NOT, IN, KV, STR, WORD };
    Type type;
    QString val1;
    QString val2;
};

static QList<Token> tokenize(const QString& query) {
    QList<Token> tokens;
    QRegularExpression re(
        QStringLiteral("\\s*(?:"
                       "(\\()|"
                       "(\\))|"
                       "\\b(OR|AND|NOT|IN)\\b|"
                       "([a-zA-Z0-9_-]+:)(?:\"([^\"]*)\"|([^\\s\\(\\)]+))|"
                       "(=)\"([^\"]+)\"|"
                       "(=)([^\\s\\(\\)]+)|"
                       "(\"([^\"]+)\")|"
                       "([^\\s\\(\\)]+)"
                       ")\\s*"));

    QRegularExpressionMatchIterator i = re.globalMatch(query);
    while (i.hasNext()) {
        QRegularExpressionMatch match = i.next();
        if (!match.captured(1).isEmpty()) {
            tokens.append({Token::LPAREN, QStringLiteral("("), QString()});
        } else if (!match.captured(2).isEmpty()) {
            tokens.append({Token::RPAREN, QStringLiteral(")"), QString()});
        } else if (!match.captured(3).isEmpty()) {
            QString op = match.captured(3);
            if (op == QStringLiteral("OR"))
                tokens.append({Token::OR, op, QString()});
            else if (op == QStringLiteral("AND"))
                tokens.append({Token::AND, op, QString()});
            else if (op == QStringLiteral("NOT"))
                tokens.append({Token::NOT, op, QString()});
            else if (op == QStringLiteral("IN"))
                tokens.append({Token::IN, op, QString()});
        } else if (!match.captured(4).isEmpty()) {
            QString key = match.captured(4);
            key.chop(1);  // remove ':'
            QString val = match.captured(5).isEmpty() ? match.captured(6) : match.captured(5);
            tokens.append({Token::KV, key, val});
        } else if (!match.captured(7).isEmpty()) {
            tokens.append({Token::WORD, match.captured(8), QString()});
        } else if (!match.captured(9).isEmpty()) {
            tokens.append({Token::WORD, match.captured(10), QString()});
        } else if (!match.captured(11).isEmpty()) {
            tokens.append({Token::STR, match.captured(12), QString()});
        } else if (!match.captured(13).isEmpty()) {
            tokens.append({Token::WORD, match.captured(13), QString()});
        }
    }
    return tokens;
}

class Parser {
    QList<Token> m_tokens;
    int m_pos;

   public:
    Parser(const QList<Token>& tokens) : m_tokens(tokens), m_pos(0) {}

    QSharedPointer<ASTNode> parse() {
        if (m_tokens.isEmpty()) return QSharedPointer<ASTNode>();
        return parseAnd();
    }

   private:
    Token current() const {
        if (m_pos < m_tokens.size()) return m_tokens[m_pos];
        return {Token::WORD, QString(), QString()};
    }
    Token next() {
        if (m_pos < m_tokens.size()) return m_tokens[m_pos++];
        return {Token::WORD, QString(), QString()};
    }
    bool atEnd() const { return m_pos >= m_tokens.size(); }

    QSharedPointer<ASTNode> parseAnd() {
        QList<QSharedPointer<ASTNode>> nodes;
        nodes.append(parseOr());
        while (!atEnd() && current().type != Token::RPAREN) {
            if (current().type == Token::OR) break;
            if (current().type == Token::AND) {
                next();
            }
            nodes.append(parseOr());
        }
        if (nodes.size() == 1) return nodes.first();
        return QSharedPointer<ASTNode>(new AndNode(nodes));
    }

    QSharedPointer<ASTNode> parseOr() {
        QList<QSharedPointer<ASTNode>> nodes;
        nodes.append(parseUnary());
        while (!atEnd() && current().type == Token::OR) {
            next();
            nodes.append(parseUnary());
        }
        if (nodes.size() == 1) return nodes.first();
        return QSharedPointer<ASTNode>(new OrNode(nodes));
    }

    QSharedPointer<ASTNode> parseUnary() {
        if (!atEnd() && current().type == Token::NOT) {
            next();
            return QSharedPointer<ASTNode>(new NotNode(parseUnary()));
        }
        return parsePrimary();
    }

    QSharedPointer<ASTNode> parsePrimary() {
        if (atEnd()) return QSharedPointer<ASTNode>(new KeywordNode(QString()));

        Token tok = next();
        if (tok.type == Token::LPAREN) {
            QSharedPointer<ASTNode> node = parseAnd();
            if (!atEnd() && current().type == Token::RPAREN) {
                next();
            }
            return node;
        } else if (tok.type == Token::KV) {
            return QSharedPointer<ASTNode>(new KeyValueNode(tok.val1, tok.val2));
        } else if (tok.type == Token::STR || tok.type == Token::WORD) {
            if (!atEnd() && current().type == Token::IN) {
                next();
                QString valuesStr = QString();
                if (!atEnd() && (current().type == Token::STR || current().type == Token::WORD)) {
                    valuesStr = next().val1;
                }
                return QSharedPointer<ASTNode>(new InNode(tok.val1, valuesStr));
            }
            return QSharedPointer<ASTNode>(new KeywordNode(tok.val1));
        }
        return QSharedPointer<ASTNode>(new KeywordNode(tok.val1));
    }
};

QSharedPointer<ASTNode> FilterParser::parse(const QString& query) {
    QString q = query.trimmed();
    if (q.startsWith(QLatin1Char('='))) {
        q = q.mid(1);
    }
    QList<Token> tokens = tokenize(q);
    Parser parser(tokens);
    return parser.parse();
}

bool AndNode::evaluate(const FilterDataAccessor& accessor) const {
    for (const auto& child : m_children) {
        if (!child->evaluate(accessor)) return false;
    }
    return true;
}
QString AndNode::toString() const {
    QStringList parts;
    for (const auto& child : m_children) parts.append(child->toString());
    return QStringLiteral("(") + parts.join(QStringLiteral(" AND ")) + QStringLiteral(")");
}

bool OrNode::evaluate(const FilterDataAccessor& accessor) const {
    for (const auto& child : m_children) {
        if (child->evaluate(accessor)) return true;
    }
    return false;
}
QString OrNode::toString() const {
    QStringList parts;
    for (const auto& child : m_children) parts.append(child->toString());
    return QStringLiteral("(") + parts.join(QStringLiteral(" OR ")) + QStringLiteral(")");
}

bool NotNode::evaluate(const FilterDataAccessor& accessor) const { return !m_child->evaluate(accessor); }
QString NotNode::toString() const { return QStringLiteral("NOT ") + m_child->toString(); }

bool InNode::evaluate(const FilterDataAccessor& accessor) const {
    if (m_values.isEmpty()) return false;
    QString val = accessor.getValue(m_key);
    if (val.isEmpty()) return false;
    for (const QString& v : m_values) {
        if (val.compare(v, Qt::CaseInsensitive) == 0) return true;
    }
    return false;
}
QString InNode::toString() const { return m_key + QStringLiteral(" IN \"") + m_valuesStr + QStringLiteral("\""); }

static bool checkDateFilter(const QString& filterVal, const QString& dateStr, bool isBefore) {
    QDateTime filterDate = QDateTime::fromString(filterVal, Qt::ISODate);
    QDateTime dataDate = QDateTime::fromString(dateStr, Qt::ISODate);
    if (!filterDate.isValid() || !dataDate.isValid()) return false;
    if (isBefore) return dataDate < filterDate;
    return dataDate > filterDate;
}

bool KeyValueNode::evaluate(const FilterDataAccessor& accessor) const {
    QString lowerKey = m_key.toLower();
    if (lowerKey == QStringLiteral("created-before") || lowerKey == QStringLiteral("updated-before") ||
        lowerKey == QStringLiteral("created-after") || lowerKey == QStringLiteral("updated-after")) {
        QString dateStr;
        if (lowerKey.startsWith(QStringLiteral("created")))
            dateStr = accessor.getValue(QStringLiteral("createdat"));
        else
            dateStr = accessor.getValue(QStringLiteral("updatedat"));
        bool isBefore = lowerKey.endsWith(QStringLiteral("before"));
        return checkDateFilter(m_value, dateStr, isBefore);
    }
    QString val = accessor.getValue(m_key);
    if (lowerKey == QStringLiteral("repo") || lowerKey == QStringLiteral("owner")) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 3, 0)
        QRegularExpression re = QRegularExpression::fromWildcard(m_value, Qt::CaseInsensitive);
        return re.match(val).hasMatch();
#else
        QRegularExpression re(QRegularExpression::wildcardToRegularExpression(m_value),
                              QRegularExpression::CaseInsensitiveOption);
        return re.match(val).hasMatch();
#endif
    }
    return val.contains(m_value, Qt::CaseInsensitive);
}
QString KeyValueNode::toString() const {
    if (m_value.contains(QLatin1Char(' ')))
        return m_key + QStringLiteral(":") + QStringLiteral("\"") + m_value + QStringLiteral("\"");
    return m_key + QStringLiteral(":") + m_value;
}

bool KeywordNode::evaluate(const FilterDataAccessor& accessor) const {
    QList<QString> allVals = accessor.getAllValues();
    for (const QString& v : allVals) {
        if (v.contains(m_keyword, Qt::CaseInsensitive)) return true;
    }
    return false;
}
QString KeywordNode::toString() const {
    if (m_keyword.contains(QLatin1Char(' '))) return QStringLiteral("\"") + m_keyword + QStringLiteral("\"");
    return m_keyword;
}
