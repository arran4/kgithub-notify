#include "FilterParser.h"

#include <QRegularExpression>
#include <QStringList>
#include <QtGlobal>

struct Token {
    enum Type { LPAREN, RPAREN, AND, OR, NOT, IN, KV, STR, WORD, LEX_ERROR };
    Type type = WORD;
    QString val1;
    QString val2;
    int pos = 0;
};

static QList<Token> tokenize(const QString& query, QString* lexError, int* lexErrorPos) {
    QList<Token> tokens;
    int i = 0;
    int len = query.length();

    while (i < len) {
        // Skip whitespace
        while (i < len && query.at(i).isSpace()) {
            i++;
        }
        if (i >= len) break;

        int startPos = i;
        QChar c = query.at(i);

        if (c == QChar('(')) {
            tokens.append({Token::LPAREN, QStringLiteral("("), QString(), startPos});
            i++;
            continue;
        }
        if (c == QChar(')')) {
            tokens.append({Token::RPAREN, QStringLiteral(")"), QString(), startPos});
            i++;
            continue;
        }

        if (c == QChar('"') || c == QChar('\'')) {
            // Quoted string
            QChar quoteChar = c;
            i++;
            int strStart = i;
            bool closed = false;
            while (i < len) {
                if (query.at(i) == quoteChar) {
                    closed = true;
                    break;
                }
                i++;
            }
            if (!closed) {
                if (lexError) *lexError = QStringLiteral("Unterminated quote");
                if (lexErrorPos) *lexErrorPos = startPos;
                tokens.append({Token::LEX_ERROR, QStringLiteral("Unterminated quote"), QString(), startPos});
                return tokens;
            }
            QString strVal = query.mid(strStart, i - strStart);
            i++;  // skip closing quote
            tokens.append({Token::STR, strVal, QString(), startPos});
            continue;
        }

        // Check if this token starts with a key: [a-zA-Z0-9_-]+:
        int keyEnd = i;
        while (keyEnd < len && (query.at(keyEnd).isLetterOrNumber() || query.at(keyEnd) == QChar('-') ||
                                query.at(keyEnd) == QChar('_'))) {
            keyEnd++;
        }
        if (keyEnd > i && keyEnd < len && query.at(keyEnd) == QChar(':')) {
            QString key = query.mid(i, keyEnd - i);
            i = keyEnd + 1;  // skip ':'
            if (i < len && (query.at(i) == QChar('"') || query.at(i) == QChar('\''))) {
                // key:"quoted value"
                QChar quoteChar = query.at(i);
                int qStart = i;
                i++;  // skip opening quote
                int valStart = i;
                bool closed = false;
                while (i < len) {
                    if (query.at(i) == quoteChar) {
                        closed = true;
                        break;
                    }
                    i++;
                }
                if (!closed) {
                    if (lexError) *lexError = QStringLiteral("Unterminated quote");
                    if (lexErrorPos) *lexErrorPos = qStart;
                    tokens.append({Token::LEX_ERROR, QStringLiteral("Unterminated quote"), QString(), qStart});
                    return tokens;
                }
                QString val = query.mid(valStart, i - valStart);
                i++;  // skip closing quote
                tokens.append({Token::KV, key, val, startPos});
                continue;
            } else {
                // key:unquoted_value
                int valStart = i;
                while (i < len && !query.at(i).isSpace() && query.at(i) != QChar('(') && query.at(i) != QChar(')')) {
                    i++;
                }
                QString val = query.mid(valStart, i - valStart);
                if (val.isEmpty()) {
                    if (lexError) *lexError = QStringLiteral("Missing value for key '%1'").arg(key);
                    if (lexErrorPos) *lexErrorPos = startPos;
                    tokens.append(
                        {Token::LEX_ERROR, QStringLiteral("Missing value for key '%1'").arg(key), QString(), startPos});
                    return tokens;
                }
                tokens.append({Token::KV, key, val, startPos});
                continue;
            }
        }

        // Word or operator
        int wordStart = i;
        while (i < len && !query.at(i).isSpace() && query.at(i) != QChar('(') && query.at(i) != QChar(')') &&
               query.at(i) != QChar('"')) {
            i++;
        }
        QString word = query.mid(wordStart, i - wordStart);
        if (word == QStringLiteral("AND")) {
            tokens.append({Token::AND, word, QString(), wordStart});
        } else if (word == QStringLiteral("OR")) {
            tokens.append({Token::OR, word, QString(), wordStart});
        } else if (word == QStringLiteral("NOT")) {
            tokens.append({Token::NOT, word, QString(), wordStart});
        } else if (word == QStringLiteral("IN")) {
            tokens.append({Token::IN, word, QString(), wordStart});
        } else {
            tokens.append({Token::WORD, word, QString(), wordStart});
        }
    }

    return tokens;
}

class Parser {
    QList<Token> m_tokens;
    int m_pos;
    bool m_error = false;
    QString m_errorMsg;
    int m_errorPos = -1;

   public:
    explicit Parser(const QList<Token>& tokens) : m_tokens(tokens), m_pos(0) {}

    FilterParseResult parse() {
        FilterParseResult res;
        if (m_tokens.isEmpty()) {
            res.ok = true;
            res.ast = QSharedPointer<ASTNode>();
            return res;
        }

        for (const Token& tok : m_tokens) {
            if (tok.type == Token::LEX_ERROR) {
                res.ok = false;
                res.error = tok.val1;
                res.errorPos = tok.pos;
                return res;
            }
        }

        QSharedPointer<ASTNode> ast = parseOr();
        if (m_error) {
            res.ok = false;
            res.error = m_errorMsg;
            res.errorPos = m_errorPos;
            return res;
        }

        if (!atEnd()) {
            Token trailing = current();
            res.ok = false;
            if (trailing.type == Token::RPAREN) {
                res.error = QStringLiteral("Unmatched closing parenthesis");
            } else {
                res.error = QStringLiteral("Unexpected trailing token: %1").arg(trailing.val1);
            }
            res.errorPos = trailing.pos;
            return res;
        }

        res.ok = true;
        res.ast = ast;
        return res;
    }

   private:
    Token current() const {
        if (m_pos < m_tokens.size()) return m_tokens[m_pos];
        return {Token::WORD, QString(), QString(), -1};
    }

    Token next() {
        if (m_pos < m_tokens.size()) return m_tokens[m_pos++];
        return {Token::WORD, QString(), QString(), -1};
    }

    bool atEnd() const { return m_pos >= m_tokens.size(); }

    void setError(const QString& msg, int pos) {
        if (!m_error) {
            m_error = true;
            m_errorMsg = msg;
            m_errorPos = pos;
        }
    }

    QSharedPointer<ASTNode> parseOr() {
        QSharedPointer<ASTNode> firstChild = parseAnd();
        if (m_error) return QSharedPointer<ASTNode>();
        if (!firstChild) return QSharedPointer<ASTNode>();

        QList<QSharedPointer<ASTNode>> nodes;
        if (auto* childOr = dynamic_cast<OrNode*>(firstChild.data())) {
            nodes.append(childOr->children());
        } else {
            nodes.append(firstChild);
        }

        while (!atEnd() && current().type == Token::OR) {
            Token orTok = next();
            if (atEnd() || current().type == Token::RPAREN || current().type == Token::OR ||
                current().type == Token::AND) {
                setError(QStringLiteral("Incomplete OR: missing right operand"), orTok.pos);
                return QSharedPointer<ASTNode>();
            }

            QSharedPointer<ASTNode> child = parseAnd();
            if (m_error) return QSharedPointer<ASTNode>();
            if (!child) {
                setError(QStringLiteral("Incomplete OR: missing right operand"), orTok.pos);
                return QSharedPointer<ASTNode>();
            }

            if (auto* childOr = dynamic_cast<OrNode*>(child.data())) {
                nodes.append(childOr->children());
            } else {
                nodes.append(child);
            }
        }

        if (nodes.size() == 1) return nodes.first();
        return QSharedPointer<ASTNode>(new OrNode(nodes));
    }

    QSharedPointer<ASTNode> parseAnd() {
        QSharedPointer<ASTNode> firstChild = parseUnary();
        if (m_error) return QSharedPointer<ASTNode>();
        if (!firstChild) return QSharedPointer<ASTNode>();

        QList<QSharedPointer<ASTNode>> nodes;
        if (auto* childAnd = dynamic_cast<AndNode*>(firstChild.data())) {
            nodes.append(childAnd->children());
        } else {
            nodes.append(firstChild);
        }

        while (!atEnd() && current().type != Token::RPAREN && current().type != Token::OR) {
            if (current().type == Token::AND) {
                Token andTok = next();
                if (atEnd() || current().type == Token::RPAREN || current().type == Token::OR ||
                    current().type == Token::AND) {
                    setError(QStringLiteral("Incomplete AND: missing right operand"), andTok.pos);
                    return QSharedPointer<ASTNode>();
                }
            }

            QSharedPointer<ASTNode> child = parseUnary();
            if (m_error) return QSharedPointer<ASTNode>();
            if (!child) {
                break;
            }

            if (auto* childAnd = dynamic_cast<AndNode*>(child.data())) {
                nodes.append(childAnd->children());
            } else {
                nodes.append(child);
            }
        }

        if (nodes.size() == 1) return nodes.first();
        return QSharedPointer<ASTNode>(new AndNode(nodes));
    }

    QSharedPointer<ASTNode> parseUnary() {
        if (!atEnd() && current().type == Token::NOT) {
            Token notTok = next();
            if (atEnd() || current().type == Token::RPAREN || current().type == Token::OR ||
                current().type == Token::AND) {
                setError(QStringLiteral("Incomplete NOT: missing operand"), notTok.pos);
                return QSharedPointer<ASTNode>();
            }

            QSharedPointer<ASTNode> inner = parseUnary();
            if (m_error) return QSharedPointer<ASTNode>();
            if (!inner) {
                setError(QStringLiteral("Incomplete NOT: missing operand"), notTok.pos);
                return QSharedPointer<ASTNode>();
            }
            return QSharedPointer<ASTNode>(new NotNode(inner));
        }

        return parsePrimary();
    }

    QSharedPointer<ASTNode> parsePrimary() {
        if (atEnd()) return QSharedPointer<ASTNode>();

        Token tok = current();
        if (tok.type == Token::LPAREN) {
            next();
            if (atEnd()) {
                setError(QStringLiteral("Unmatched opening parenthesis"), tok.pos);
                return QSharedPointer<ASTNode>();
            }
            if (current().type == Token::RPAREN) {
                setError(QStringLiteral("Empty parentheses"), tok.pos);
                return QSharedPointer<ASTNode>();
            }

            QSharedPointer<ASTNode> node = parseOr();
            if (m_error) return QSharedPointer<ASTNode>();
            if (!node) {
                setError(QStringLiteral("Empty parentheses or missing expression"), tok.pos);
                return QSharedPointer<ASTNode>();
            }

            if (atEnd() || current().type != Token::RPAREN) {
                setError(QStringLiteral("Unmatched opening parenthesis"), tok.pos);
                return QSharedPointer<ASTNode>();
            }
            next();  // consume RPAREN
            return node;
        } else if (tok.type == Token::RPAREN) {
            setError(QStringLiteral("Unmatched closing parenthesis"), tok.pos);
            return QSharedPointer<ASTNode>();
        } else if (tok.type == Token::AND || tok.type == Token::OR || tok.type == Token::IN) {
            setError(QStringLiteral("Missing operand before '%1'").arg(tok.val1), tok.pos);
            return QSharedPointer<ASTNode>();
        } else if (tok.type == Token::KV) {
            next();
            if (!FilterParser::isSupportedKey(tok.val1)) {
                setError(QStringLiteral("Unknown filter key: '%1'").arg(tok.val1), tok.pos);
                return QSharedPointer<ASTNode>();
            }
            return QSharedPointer<ASTNode>(new KeyValueNode(tok.val1, tok.val2));
        } else if (tok.type == Token::STR || tok.type == Token::WORD) {
            next();
            if (!atEnd() && current().type == Token::IN) {
                Token inTok = next();
                if (atEnd() || (current().type != Token::STR && current().type != Token::WORD)) {
                    setError(QStringLiteral("Incomplete IN: missing value list"), inTok.pos);
                    return QSharedPointer<ASTNode>();
                }
                Token valTok = next();
                if (!FilterParser::isSupportedKey(tok.val1)) {
                    setError(QStringLiteral("Unknown filter key: '%1'").arg(tok.val1), tok.pos);
                    return QSharedPointer<ASTNode>();
                }
                return QSharedPointer<ASTNode>(new InNode(tok.val1, valTok.val1));
            }
            return QSharedPointer<ASTNode>(new KeywordNode(tok.val1));
        }

        next();
        return QSharedPointer<ASTNode>(new KeywordNode(tok.val1));
    }
};

const QSet<QString>& FilterParser::supportedKeys() {
    static const QSet<QString> s_keys = {
        QStringLiteral("name"),          QStringLiteral("repo"),           QStringLiteral("owner"),
        QStringLiteral("fork"),          QStringLiteral("archived"),       QStringLiteral("visibility"),
        QStringLiteral("created"),       QStringLiteral("createdat"),      QStringLiteral("created-at"),
        QStringLiteral("created_at"),    QStringLiteral("updated"),        QStringLiteral("updatedat"),
        QStringLiteral("updated-at"),    QStringLiteral("updated_at"),     QStringLiteral("created-before"),
        QStringLiteral("created-after"), QStringLiteral("updated-before"), QStringLiteral("updated-after"),
    };
    return s_keys;
}

bool FilterParser::isSupportedKey(const QString& key) { return supportedKeys().contains(key.toLower()); }

FilterParseResult FilterParser::parseWithResult(const QString& query) {
    QString q = query.trimmed();
    if (q.startsWith(QLatin1Char('='))) {
        q = q.mid(1).trimmed();
    }
    if (q.isEmpty()) {
        FilterParseResult res;
        res.ok = true;
        res.ast = QSharedPointer<ASTNode>();
        return res;
    }

    QString lexError;
    int lexErrorPos = -1;
    QList<Token> tokens = tokenize(q, &lexError, &lexErrorPos);
    if (!lexError.isEmpty()) {
        FilterParseResult res;
        res.ok = false;
        res.error = lexError;
        res.errorPos = lexErrorPos;
        return res;
    }

    Parser parser(tokens);
    return parser.parse();
}

QSharedPointer<ASTNode> FilterParser::parse(const QString& query) {
    FilterParseResult res = parseWithResult(query);
    return res.ok ? res.ast : QSharedPointer<ASTNode>();
}

bool AndNode::evaluate(const FilterDataAccessor& accessor) const {
    return std::all_of(m_children.begin(), m_children.end(),
                       [&accessor](const auto& child) { return child->evaluate(accessor); });
}
QString AndNode::toString() const {
    QStringList parts;
    for (const auto& child : m_children) parts.append(child->toString());
    return QStringLiteral("(") + parts.join(QStringLiteral(" AND ")) + QStringLiteral(")");
}

bool OrNode::evaluate(const FilterDataAccessor& accessor) const {
    return std::any_of(m_children.begin(), m_children.end(),
                       [&accessor](const auto& child) { return child->evaluate(accessor); });
}
QString OrNode::toString() const {
    QStringList parts;
    for (const auto& child : m_children) parts.append(child->toString());
    return QStringLiteral("(") + parts.join(QStringLiteral(" OR ")) + QStringLiteral(")");
}

bool NotNode::evaluate(const FilterDataAccessor& accessor) const { return !m_child->evaluate(accessor); }
QString NotNode::toString() const { return QStringLiteral("NOT ") + m_child->toString(); }

bool InNode::evaluate(const FilterDataAccessor& accessor) const {
    if (!FilterParser::isSupportedKey(m_key)) return false;
    if (m_values.isEmpty()) return false;
    QString val = accessor.getValue(m_key);
    if (val.isEmpty()) return false;
    return std::any_of(m_values.begin(), m_values.end(),
                       [&val](const QString& v) { return val.compare(v, Qt::CaseInsensitive) == 0; });
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
    if (!FilterParser::isSupportedKey(m_key)) return false;
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
    if (m_value.contains('*') || m_value.contains('?')) {
        QRegularExpression re(QRegularExpression::wildcardToRegularExpression(m_value),
                              QRegularExpression::CaseInsensitiveOption);
        return re.match(val).hasMatch();
    }
    if (lowerKey == QStringLiteral("repo") || lowerKey == QStringLiteral("owner")) {
        return val.compare(m_value, Qt::CaseInsensitive) == 0;
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
    return std::any_of(allVals.begin(), allVals.end(),
                       [this](const QString& v) { return v.contains(m_keyword, Qt::CaseInsensitive); });
}
QString KeywordNode::toString() const {
    if (m_keyword.contains(QLatin1Char(' '))) return QStringLiteral("\"") + m_keyword + QStringLiteral("\"");
    return m_keyword;
}
