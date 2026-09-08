#ifndef NOTIFICATIONRULEENGINE_H
#define NOTIFICATIONRULEENGINE_H

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QSettings>
#include <QString>
#include <QUuid>

#include "Notification.h"

class NotificationRule {
   public:
    QString repoFilter;    // repo matches, supports * wildcard
    QString typeFilter;    // issue, pull request, etc
    QString reasonFilter;  // mention, review_requested, etc
    QString titleFilter;   // general text matching title

    NotificationRule();
    QString id;
    QString action;  // "Mute", "AlwaysIndividual", "NeverIndividual", "AlwaysSummarize", "Default"

    QJsonObject toJson() const;
    static NotificationRule fromJson(const QJsonObject& obj);
    bool matches(const Notification& n) const;

    QString displayCondition() const;
};

class NotificationRuleModel {
public:
    NotificationRuleModel();
    void load();
    void save();

    QList<NotificationRule> allRules() const { return m_rules; }
    void addRule(const NotificationRule& rule);
    void updateRule(const NotificationRule& rule);
    void removeRule(const QString& id);
    void moveUp(const QString& id);
    void moveDown(const QString& id);

private:
    QList<NotificationRule> m_rules;
};

class NotificationRuleEngine {
   public:
    static QList<NotificationRule> loadRules();
    static void saveRules(const QList<NotificationRule>& rules);

    static QString evaluate(const Notification& n);
    static void addRule(const NotificationRule& rule);
    static void prependRule(const NotificationRule& rule);
};

#endif  // NOTIFICATIONRULEENGINE_H
