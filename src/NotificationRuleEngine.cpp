#include "NotificationRuleEngine.h"

#include <QRegularExpression>
#include <QStringList>

QJsonObject NotificationRule::toJson() const {
    QJsonObject obj;
    obj["repoFilter"] = repoFilter;
    obj["typeFilter"] = typeFilter;
    obj["reasonFilter"] = reasonFilter;
    obj["titleFilter"] = titleFilter;
    obj["action"] = action;
    return obj;
}

NotificationRule NotificationRule::fromJson(const QJsonObject& obj) {
    NotificationRule rule;
    rule.repoFilter = obj["repoFilter"].toString();
    rule.typeFilter = obj["typeFilter"].toString();
    rule.reasonFilter = obj["reasonFilter"].toString();
    rule.titleFilter = obj["titleFilter"].toString();
    rule.action = obj["action"].toString();

    // Backwards compatibility for the string based "condition"
    if (obj.contains("condition")) {
        QString c = obj["condition"].toString().trimmed();
        QStringList parts = c.split(' ', Qt::SkipEmptyParts);
        for (const QString& part : parts) {
            if (part.startsWith("repo:", Qt::CaseInsensitive)) {
                rule.repoFilter = part.mid(5);
            } else if (part.startsWith("type:", Qt::CaseInsensitive)) {
                rule.typeFilter = part.mid(5);
            } else if (part.startsWith("reason:", Qt::CaseInsensitive)) {
                rule.reasonFilter = part.mid(7);
            } else {
                rule.titleFilter = part;
            }
        }
    }
    return rule;
}

QString NotificationRule::displayCondition() const {
    QStringList parts;
    if (!repoFilter.isEmpty()) parts << "Repo: " + repoFilter;
    if (!typeFilter.isEmpty()) parts << "Type: " + typeFilter;
    if (!reasonFilter.isEmpty()) parts << "Reason: " + reasonFilter;
    if (!titleFilter.isEmpty()) parts << "Title: " + titleFilter;

    if (parts.isEmpty()) return "All Notifications";
    return parts.join(" | ");
}

bool NotificationRule::matches(const Notification& n) const {
    auto matchField = [](const QString& filter, const QString& value, bool isWildcard = false) -> bool {
        if (filter.isEmpty()) return true;  // Empty filter means it matches anything

        bool isNegative = filter.startsWith("!");
        QString actualFilter = isNegative ? filter.mid(1) : filter;

        bool isMatch = false;
        if (isWildcard) {
            QRegularExpression rx(QRegularExpression::wildcardToRegularExpression(actualFilter),
                                  QRegularExpression::CaseInsensitiveOption);
            isMatch = rx.match(value).hasMatch();
        } else {
            isMatch = value.contains(actualFilter, Qt::CaseInsensitive);
            // exact match for type/reason if not using wildcard logic natively
            if (!isWildcard && (actualFilter.compare(value, Qt::CaseInsensitive) == 0)) {
                isMatch = true;
            }
        }

        return isNegative ? !isMatch : isMatch;
    };

    if (!matchField(repoFilter, n.repository, true)) return false;
    if (!matchField(typeFilter, n.type)) return false;
    if (!matchField(reasonFilter, n.reason)) return false;
    if (!matchField(titleFilter, n.title)) return false;

    return true;
}

QList<NotificationRule> NotificationRuleEngine::loadRules() {
    QSettings settings("kgithub-notify", "NotificationRules");
    QList<NotificationRule> rules;
    QVariantList list = settings.value("rules").toList();
    for (const QVariant& v : list) {
        if (v.typeId() == QMetaType::QString) {
            QJsonDocument doc = QJsonDocument::fromJson(v.toString().toUtf8());
            if (doc.isObject()) rules.append(NotificationRule::fromJson(doc.object()));
        } else if (v.canConvert<QJsonObject>()) {
            rules.append(NotificationRule::fromJson(v.toJsonObject()));
        }
    }
    return rules;
}

void NotificationRuleEngine::saveRules(const QList<NotificationRule>& rules) {
    QSettings settings("kgithub-notify", "NotificationRules");
    QVariantList list;
    for (const NotificationRule& rule : rules) {
        QJsonDocument doc(rule.toJson());
        list.append(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
    }
    settings.setValue("rules", list);
}

QString NotificationRuleEngine::evaluate(const Notification& n) {
    QList<NotificationRule> rules = loadRules();
    for (const NotificationRule& rule : rules) {
        if (rule.matches(n)) {
            if (rule.action != "Default") {
                return rule.action;
            }
        }
    }
    return "Default";
}

void NotificationRuleEngine::addRule(const NotificationRule& rule) {
    QList<NotificationRule> rules = loadRules();
    rules.append(rule);
    saveRules(rules);
}

void NotificationRuleEngine::prependRule(const NotificationRule& rule) {
    QList<NotificationRule> rules = loadRules();
    rules.prepend(rule);
    saveRules(rules);
}
