#include "NotificationRuleEngine.h"

#include <QRegularExpression>
#include <QStringList>

QJsonObject NotificationRule::toJson() const {
    QJsonObject obj;
    obj["condition"] = condition;
    obj["action"] = action;
    return obj;
}

NotificationRule NotificationRule::fromJson(const QJsonObject& obj) {
    NotificationRule rule;
    rule.condition = obj["condition"].toString();
    rule.action = obj["action"].toString();
    return rule;
}

bool NotificationRule::matches(const Notification& n) const {
    QString c = condition.trimmed();
    if (c.isEmpty() || c == "*") return true;

    QStringList parts = c.split(' ', Qt::SkipEmptyParts);
    for (const QString& part : parts) {
        if (part.startsWith("repo:", Qt::CaseInsensitive)) {
            QString v = part.mid(5);
            QRegularExpression rx(QRegularExpression::wildcardToRegularExpression(v),
                                  QRegularExpression::CaseInsensitiveOption);
            if (!rx.match(n.repository).hasMatch()) return false;
        } else if (part.startsWith("type:", Qt::CaseInsensitive)) {
            QString v = part.mid(5);
            if (n.type.compare(v, Qt::CaseInsensitive) != 0) return false;
        } else if (part.startsWith("reason:", Qt::CaseInsensitive)) {
            QString v = part.mid(7);
            if (n.reason.compare(v, Qt::CaseInsensitive) != 0) return false;
        } else {
            if (!n.repository.contains(part, Qt::CaseInsensitive) && !n.title.contains(part, Qt::CaseInsensitive)) {
                return false;
            }
        }
    }
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
