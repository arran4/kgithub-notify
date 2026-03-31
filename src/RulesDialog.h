#ifndef RULESDIALOG_H
#define RULESDIALOG_H

#include <QDialog>
#include <QPushButton>
#include <QTableWidget>

class RulesDialog : public QDialog {
    Q_OBJECT
   public:
    explicit RulesDialog(QWidget* parent = nullptr);

   private slots:
    void addRule();
    void editRule();
    void removeRule();
    void saveRules();

   private:
    void loadRules();
    QTableWidget* rulesTable;
};

#endif  // RULESDIALOG_H
