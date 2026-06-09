#include "MaterialRoleMappingEditor.h"

#include <QFormLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

QStringList roles() {
    return {"rgb", "white", "varnish", "ignore", "support_candidate", "support"};
}

}  // namespace

MaterialRoleMappingEditor::MaterialRoleMappingEditor(ConfigDocument* document, QWidget* parent)
    : QWidget(parent), document_(document) {
    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout();
    enabled_ = new QCheckBox("启用 MaterialRoleMapping", this);
    default_role_ = new QComboBox(this);
    default_role_->addItems(roles());
    allow_input_support_ = new QCheckBox("允许输入支撑材料", this);
    form->addRow(enabled_);
    form->addRow("默认角色", default_role_);
    form->addRow(allow_input_support_);
    layout->addLayout(form);

    rules_ = new QTableWidget(this);
    rules_->setColumnCount(3);
    rules_->setHorizontalHeaderLabels({"matchNameContains", "role", "删除"});
    rules_->horizontalHeader()->setStretchLastSection(false);
    rules_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    rules_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    rules_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    layout->addWidget(rules_, 1);

    auto* add = new QPushButton("新增规则", this);
    layout->addWidget(add);

    connect(document_, &ConfigDocument::changed, this, &MaterialRoleMappingEditor::loadFromDocument);
    connect(enabled_, &QCheckBox::toggled, this, [this](const bool checked) {
        if (!loading_) {
            document_->setValue({"materialRoleMapping", "enabled"}, checked);
        }
    });
    connect(default_role_, &QComboBox::currentTextChanged, this, [this](const QString& role) {
        if (!loading_) {
            document_->setValue({"materialRoleMapping", "defaultRole"}, role);
        }
    });
    connect(allow_input_support_, &QCheckBox::toggled, this, [this](const bool checked) {
        if (!loading_) {
            document_->setValue({"materialRoleMapping", "allowInputSupportMaterial"}, checked);
        }
    });
    connect(add, &QPushButton::clicked, this, &MaterialRoleMappingEditor::addRule);
    connect(rules_, &QTableWidget::cellChanged, this, &MaterialRoleMappingEditor::writeRules);
}

void MaterialRoleMappingEditor::loadFromDocument() {
    loading_ = true;
    enabled_->setChecked(document_->value({"materialRoleMapping", "enabled"}).toBool());
    default_role_->setCurrentText(document_->value({"materialRoleMapping", "defaultRole"}).toString("rgb"));
    allow_input_support_->setChecked(document_->value({"materialRoleMapping", "allowInputSupportMaterial"}).toBool());
    rules_->setRowCount(0);
    const QJsonArray rules = document_->value({"materialRoleMapping", "rules"}).toArray();
    for (const QJsonValue& value : rules) {
        const QJsonObject rule = value.toObject();
        addRuleRow(rule.value("matchNameContains").toString(), rule.value("role").toString("rgb"));
    }
    loading_ = false;
}

void MaterialRoleMappingEditor::addRule() {
    addRuleRow(QString(), "rgb");
    writeRules();
}

void MaterialRoleMappingEditor::writeRules() {
    if (loading_) {
        return;
    }
    QJsonArray array;
    for (int row = 0; row < rules_->rowCount(); ++row) {
        QJsonObject rule;
        const QTableWidgetItem* match = rules_->item(row, 0);
        auto* combo = qobject_cast<QComboBox*>(rules_->cellWidget(row, 1));
        rule.insert("matchNameContains", match ? match->text() : QString());
        rule.insert("role", combo ? combo->currentText() : "rgb");
        array.push_back(rule);
    }
    document_->setValue({"materialRoleMapping", "rules"}, array);
}

QComboBox* MaterialRoleMappingEditor::createRoleCombo(const QString& role) {
    auto* combo = new QComboBox(rules_);
    combo->addItems(roles());
    combo->setCurrentText(role);
    connect(combo, &QComboBox::currentTextChanged, this, &MaterialRoleMappingEditor::writeRules);
    return combo;
}

void MaterialRoleMappingEditor::addRuleRow(const QString& match, const QString& role) {
    const int row = rules_->rowCount();
    rules_->insertRow(row);
    rules_->setItem(row, 0, new QTableWidgetItem(match));
    rules_->setCellWidget(row, 1, createRoleCombo(role));
    auto* remove = new QPushButton("删除", rules_);
    rules_->setCellWidget(row, 2, remove);
    connect(remove, &QPushButton::clicked, this, [this, remove]() {
        for (int row = 0; row < rules_->rowCount(); ++row) {
            if (rules_->cellWidget(row, 2) == remove) {
                rules_->removeRow(row);
                writeRules();
                return;
            }
        }
    });
}
