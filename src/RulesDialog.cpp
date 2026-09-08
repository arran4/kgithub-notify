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
    : QDialog(parent), m_prepopulateCondition(prepopulateCondition), m_filterRepo(preFilterRepo) {
    setWindowTitle(tr("Notification Rules"));
    resize(600, 400);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    rulesTable = new QTableWidget(0, 2, this);
    rulesTable->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    rulesTable->setHorizontalHeaderLabels({tr("Rule Matcher"), tr("Action")});
    rulesTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    rulesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    rulesTable->setSelectionMode(QAbstractItemView::SingleSelection);
    mainLayout->addWidget(rulesTable);

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* btnAdd = new QPushButton(tr("Add"), this);
    QPushButton* btnEdit = new QPushButton(tr("Edit"), this);
    btnUp = new QPushButton(tr("Move Up"), this);
    btnDown = new QPushButton(tr("Move Down"), this);
    QPushButton* btnRemove = new QPushButton(tr("Remove"), this);
    QPushButton* btnSave = new QPushButton(tr("Save"), this);
    QPushButton* btnClose = new QPushButton(tr("Close"), this);

    buttonLayout->addWidget(btnAdd);
    buttonLayout->addWidget(btnEdit);
    buttonLayout->addWidget(btnRemove);
    buttonLayout->addWidget(btnUp);
    buttonLayout->addWidget(btnDown);

    buttonLayout->addStretch();
    buttonLayout->addWidget(btnSave);
    buttonLayout->addWidget(btnClose);

    mainLayout->addLayout(buttonLayout);

    connect(btnAdd, &QPushButton::clicked, this, [this]() { addRule(m_prepopulateCondition); });
    connect(btnEdit, &QPushButton::clicked, this, &RulesDialog::editRule);
    connect(btnRemove, &QPushButton::clicked, this, &RulesDialog::removeRule);
    connect(btnUp, &QPushButton::clicked, this, &RulesDialog::moveUp);
    connect(btnDown, &QPushButton::clicked, this, &RulesDialog::moveDown);

    connect(btnSave, &QPushButton::clicked, this, &RulesDialog::saveRules);
    connect(btnClose, &QPushButton::clicked, this, &QDialog::accept);

    loadRules(preFilterRepo);
}

void RulesDialog::loadRules(const QString& filterRepo) {
    rulesTable->setRowCount(0);
    m_model.load();
    for (const NotificationRule& rule : m_model.allRules()) {
        if (!filterRepo.isEmpty() && !rule.repoFilter.contains(filterRepo)) continue;

        int row = rulesTable->rowCount();
        rulesTable->insertRow(row);
        rulesTable->setItem(row, 0, new QTableWidgetItem(rule.displayCondition()));
        rulesTable->item(row, 0)->setData(Qt::UserRole, QVariant::fromValue(rule.toJson()));
        rulesTable->setItem(row, 1, new QTableWidgetItem(rule.action));
    }

    if (!filterRepo.isEmpty()) {
        btnUp->setEnabled(false);
        btnDown->setEnabled(false);
        btnUp->setToolTip(tr("Reordering is disabled when viewing a filtered list of rules."));
        btnDown->setToolTip(tr("Reordering is disabled when viewing a filtered list of rules."));
    }
}

void RulesDialog::addRule(const QString& prepopulateRepo) {
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Add Rule"));
    QVBoxLayout layout(&dialog);

    QLabel* docLabel =
        new QLabel(tr("All filters support negative matching by prefixing the condition with '!'.\nRepository filter "
                      "supports '*' wildcards. For example: `arran4/kgithub-notif*`"));
    docLabel->setWordWrap(true);
    layout.addWidget(docLabel);
    layout.addWidget(new QLabel(tr("Repository Filter (* wildcards supported):")));
    QLineEdit repoEdit(prepopulateRepo);
    layout.addWidget(&repoEdit);

    layout.addWidget(new QLabel(tr("Type Filter (e.g. PullRequest, Issue):")));
    QLineEdit typeEdit;
    layout.addWidget(&typeEdit);

    layout.addWidget(new QLabel(tr("Reason Filter (e.g. mention, review_requested):")));
    QLineEdit reasonEdit;
    layout.addWidget(&reasonEdit);

    layout.addWidget(new QLabel(tr("Title Contains Filter:")));
    QLineEdit titleEdit;
    layout.addWidget(&titleEdit);

    layout.addWidget(new QLabel(tr("Action:")));
    QComboBox actionCombo;
    actionCombo.addItems({"Mute", "AlwaysIndividual", "NeverIndividual", "AlwaysSummarize", "Default"});
    layout.addWidget(&actionCombo);

    QDialogButtonBox buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &dialog);
    layout.addWidget(&buttonBox);
    connect(&buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        NotificationRule rule;
        rule.repoFilter = repoEdit.text().trimmed();
        rule.typeFilter = typeEdit.text().trimmed();
        rule.reasonFilter = reasonEdit.text().trimmed();
        rule.titleFilter = titleEdit.text().trimmed();
        rule.action = actionCombo.currentText();

        m_model.addRule(rule);

        if (m_filterRepo.isEmpty() || rule.repoFilter.contains(m_filterRepo)) {
            int row = rulesTable->rowCount();
            rulesTable->insertRow(row);
            rulesTable->setItem(row, 0, new QTableWidgetItem(rule.displayCondition()));
            rulesTable->item(row, 0)->setData(Qt::UserRole, QVariant::fromValue(rule.toJson()));
            rulesTable->setItem(row, 1, new QTableWidgetItem(rule.action));
        }
    }
}
void RulesDialog::editRule() {
    int row = rulesTable->currentRow();
    if (row < 0) return;

    QJsonObject obj = rulesTable->item(row, 0)->data(Qt::UserRole).toJsonObject();
    NotificationRule rule = NotificationRule::fromJson(obj);

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Edit Rule"));
    QVBoxLayout layout(&dialog);

    QLabel* docLabel =
        new QLabel(tr("All filters support negative matching by prefixing the condition with '!'.\nRepository filter "
                      "supports '*' wildcards. For example: `arran4/kgithub-notif*`"));
    docLabel->setWordWrap(true);
    layout.addWidget(docLabel);
    layout.addWidget(new QLabel(tr("Repository Filter (* wildcards supported):")));
    QLineEdit repoEdit(rule.repoFilter);
    layout.addWidget(&repoEdit);

    layout.addWidget(new QLabel(tr("Type Filter (e.g. PullRequest, Issue):")));
    QLineEdit typeEdit(rule.typeFilter);
    layout.addWidget(&typeEdit);

    layout.addWidget(new QLabel(tr("Reason Filter (e.g. mention, review_requested):")));
    QLineEdit reasonEdit(rule.reasonFilter);
    layout.addWidget(&reasonEdit);

    layout.addWidget(new QLabel(tr("Title Contains Filter:")));
    QLineEdit titleEdit(rule.titleFilter);
    layout.addWidget(&titleEdit);

    layout.addWidget(new QLabel(tr("Action:")));
    QComboBox actionCombo;
    actionCombo.addItems({"Mute", "AlwaysIndividual", "NeverIndividual", "AlwaysSummarize", "Default"});
    actionCombo.setCurrentText(rule.action);
    layout.addWidget(&actionCombo);

    QDialogButtonBox buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &dialog);
    layout.addWidget(&buttonBox);
    connect(&buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        rule.repoFilter = repoEdit.text().trimmed();
        rule.typeFilter = typeEdit.text().trimmed();
        rule.reasonFilter = reasonEdit.text().trimmed();
        rule.titleFilter = titleEdit.text().trimmed();
        rule.action = actionCombo.currentText();

        m_model.updateRule(rule);

        rulesTable->item(row, 0)->setText(rule.displayCondition());
        rulesTable->item(row, 0)->setData(Qt::UserRole, QVariant::fromValue(rule.toJson()));
        rulesTable->item(row, 1)->setText(rule.action);
    }
}
void RulesDialog::removeRule() {
    int row = rulesTable->currentRow();
    if (row >= 0) {
        QJsonObject obj = rulesTable->item(row, 0)->data(Qt::UserRole).toJsonObject();
        QString id = obj["id"].toString();
        m_model.removeRule(id);
        rulesTable->removeRow(row);
    }
}

void RulesDialog::saveRules() { m_model.save(); }
void RulesDialog::moveUp() {
    int row = rulesTable->currentRow();
    if (row > 0) {
        QJsonObject obj = rulesTable->item(row, 0)->data(Qt::UserRole).toJsonObject();
        QString id = obj["id"].toString();

        m_model.moveUp(id);

        QTableWidgetItem* conditionItem = rulesTable->takeItem(row, 0);
        QTableWidgetItem* actionItem = rulesTable->takeItem(row, 1);
        rulesTable->removeRow(row);
        rulesTable->insertRow(row - 1);
        rulesTable->setItem(row - 1, 0, conditionItem);
        rulesTable->setItem(row - 1, 1, actionItem);
        rulesTable->selectRow(row - 1);
    }
}

void RulesDialog::moveDown() {
    int row = rulesTable->currentRow();
    if (row >= 0 && row < rulesTable->rowCount() - 1) {
        QJsonObject obj = rulesTable->item(row, 0)->data(Qt::UserRole).toJsonObject();
        QString id = obj["id"].toString();

        m_model.moveDown(id);

        QTableWidgetItem* conditionItem = rulesTable->takeItem(row, 0);
        QTableWidgetItem* actionItem = rulesTable->takeItem(row, 1);
        rulesTable->removeRow(row);
        rulesTable->insertRow(row + 1);
        rulesTable->setItem(row + 1, 0, conditionItem);
        rulesTable->setItem(row + 1, 1, actionItem);
        rulesTable->selectRow(row + 1);
    }
}
