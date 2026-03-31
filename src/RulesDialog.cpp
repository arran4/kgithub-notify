#include "RulesDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QVBoxLayout>

#include "NotificationRuleEngine.h"

RulesDialog::RulesDialog(QWidget* parent, const QString& preFilterRepo, const QString& prepopulateCondition)
    : QDialog(parent), m_prepopulateCondition(prepopulateCondition) {
    setWindowTitle(tr("Notification Rules"));
    resize(600, 400);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    rulesTable = new QTableWidget(0, 2, this);
    rulesTable->setHorizontalHeaderLabels({tr("Condition"), tr("Action")});
    rulesTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    rulesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    rulesTable->setSelectionMode(QAbstractItemView::SingleSelection);
    mainLayout->addWidget(rulesTable);

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* btnAdd = new QPushButton(tr("Add"), this);
    QPushButton* btnEdit = new QPushButton(tr("Edit"), this);
    QPushButton* btnRemove = new QPushButton(tr("Remove"), this);
    QPushButton* btnSave = new QPushButton(tr("Save"), this);
    QPushButton* btnClose = new QPushButton(tr("Close"), this);

    buttonLayout->addWidget(btnAdd);
    buttonLayout->addWidget(btnEdit);
    buttonLayout->addWidget(btnRemove);
    buttonLayout->addStretch();
    buttonLayout->addWidget(btnSave);
    buttonLayout->addWidget(btnClose);

    mainLayout->addLayout(buttonLayout);

    connect(btnAdd, &QPushButton::clicked, this, [this]() { addRule(m_prepopulateCondition); });
    connect(btnEdit, &QPushButton::clicked, this, &RulesDialog::editRule);
    connect(btnRemove, &QPushButton::clicked, this, &RulesDialog::removeRule);
    connect(btnSave, &QPushButton::clicked, this, &RulesDialog::saveRules);
    connect(btnClose, &QPushButton::clicked, this, &QDialog::accept);

    loadRules(preFilterRepo);
}

void RulesDialog::loadRules(const QString& filterRepo) {
    rulesTable->setRowCount(0);
    QList<NotificationRule> rules = NotificationRuleEngine::loadRules();
    for (const NotificationRule& rule : rules) {
        if (!filterRepo.isEmpty() && !rule.condition.contains(filterRepo)) continue;

        int row = rulesTable->rowCount();
        rulesTable->insertRow(row);
        rulesTable->setItem(row, 0, new QTableWidgetItem(rule.condition));
        rulesTable->setItem(row, 1, new QTableWidgetItem(rule.action));
    }
}

void RulesDialog::addRule(const QString& prepopulateCondition) {
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Add Rule"));
    QVBoxLayout layout(&dialog);

    layout.addWidget(new QLabel(tr("Condition (e.g. repo:arran4/kgithub-notify):")));
    QLineEdit conditionEdit(prepopulateCondition);
    layout.addWidget(&conditionEdit);

    layout.addWidget(new QLabel(tr("Action:")));
    QComboBox actionCombo;
    actionCombo.addItems({"Mute", "AlwaysIndividual", "NeverIndividual", "AlwaysSummarize", "Default"});
    layout.addWidget(&actionCombo);

    QDialogButtonBox buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &dialog);
    layout.addWidget(&buttonBox);
    connect(&buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        int row = rulesTable->rowCount();
        rulesTable->insertRow(row);
        rulesTable->setItem(row, 0, new QTableWidgetItem(conditionEdit.text()));
        rulesTable->setItem(row, 1, new QTableWidgetItem(actionCombo.currentText()));
    }
}

void RulesDialog::editRule() {
    int row = rulesTable->currentRow();
    if (row < 0) return;

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Edit Rule"));
    QVBoxLayout layout(&dialog);

    layout.addWidget(new QLabel(tr("Condition:")));
    QLineEdit conditionEdit(rulesTable->item(row, 0)->text());
    layout.addWidget(&conditionEdit);

    layout.addWidget(new QLabel(tr("Action:")));
    QComboBox actionCombo;
    actionCombo.addItems({"Mute", "AlwaysIndividual", "NeverIndividual", "AlwaysSummarize", "Default"});
    actionCombo.setCurrentText(rulesTable->item(row, 1)->text());
    layout.addWidget(&actionCombo);

    QDialogButtonBox buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &dialog);
    layout.addWidget(&buttonBox);
    connect(&buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        rulesTable->item(row, 0)->setText(conditionEdit.text());
        rulesTable->item(row, 1)->setText(actionCombo.currentText());
    }
}

void RulesDialog::removeRule() {
    int row = rulesTable->currentRow();
    if (row >= 0) {
        rulesTable->removeRow(row);
    }
}

void RulesDialog::saveRules() {
    QList<NotificationRule> rules;
    for (int i = 0; i < rulesTable->rowCount(); ++i) {
        NotificationRule rule;
        rule.condition = rulesTable->item(i, 0)->text();
        rule.action = rulesTable->item(i, 1)->text();
        rules.append(rule);
    }
    NotificationRuleEngine::saveRules(rules);
}
