#ifndef NOTIFICATIONRULEENGINE_H
#define NOTIFICATIONRULEENGINE_H

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QSettings>
#include <QString>

#include "Notification.h"

class NotificationRule {
   public:
    QString repoFilter;    // repo matches, supports * wildcard
    QString typeFilter;    // issue, pull request, etc
    QString reasonFilter;  // mention, review_requested, etc
    QString titleFilter;   // general text matching title

    QString action;  // "Mute", "AlwaysIndividual", "NeverIndividual", "AlwaysSummarize", "Default"

    QJsonObject toJson() const;
    static NotificationRule fromJson(const QJsonObject& obj);
    bool matches(const Notification& n) const;

    QString displayCondition() const;
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
