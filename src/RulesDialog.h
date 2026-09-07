#ifndef RULESDIALOG_H
#define RULESDIALOG_H

#include <QDialog>
#include <QPushButton>
#include <QTableWidget>

#include "NotificationRuleEngine.h"

class RulesDialog : public QDialog {
    Q_OBJECT
   public:
    explicit RulesDialog(QWidget* parent = nullptr, const QString& preFilterRepo = QString(),
                         const QString& prepopulateCondition = QString());

   private slots:
    void addRule(const QString& prepopulateCondition = QString());

    void editRule();
    void removeRule();
    void moveUp();
    void moveDown();

    void saveRules();

   private:
    void loadRules(const QString& filterRepo = QString());
    QString m_prepopulateCondition;
    QString m_filterRepo;
    QList<NotificationRule> m_allRules;

    QTableWidget* rulesTable;
    QPushButton* btnUp;
    QPushButton* btnDown;
};

#endif  // RULESDIALOG_H
