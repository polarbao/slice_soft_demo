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

QString roleLabel(const QString& role) {
    if (role == "rgb") {
        return "RGB 彩色";
    }
    if (role == "white") {
        return "白墨";
    }
    if (role == "varnish") {
        return "光油";
    }
    if (role == "ignore") {
        return "忽略";
    }
    if (role == "support_candidate") {
        return "支撑候选";
    }
    if (role == "support") {
        return "支撑";
    }
    return "未知值：" + role;
}

void addRoleItems(QComboBox* combo) {
    for (const QString& role : roles()) {
        combo->addItem(roleLabel(role), role);
    }
}

void setRoleValue(QComboBox* combo, const QString& role) {
    int index = combo->findData(role);
    if (index < 0) {
        index = combo->count();
        combo->addItem(roleLabel(role), role);
    }
    combo->setCurrentIndex(index);
}

}  // namespace

MaterialRoleMappingEditor::MaterialRoleMappingEditor(ConfigDocument* document, QWidget* parent)
    : QWidget(parent), document_(document) {
    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout();
    enabled_ = new QCheckBox("启用材料角色映射", this);
    default_role_ = new QComboBox(this);
    addRoleItems(default_role_);
    allow_input_support_ = new QCheckBox("允许输入支撑材料", this);
    enabled_->setToolTip("启用后按材料名规则把 OBJ/3MF 材料映射到 RGB、白墨、光油、支撑或忽略。");
    default_role_->setToolTip("没有命中规则时使用的默认材料角色。");
    allow_input_support_->setToolTip("允许输入模型自带材料直接作为支撑候选；通常仅用于专项验证。");
    form->addRow(enabled_);
    form->addRow("默认角色", default_role_);
    form->addRow(allow_input_support_);
    layout->addLayout(form);

    rules_ = new QTableWidget(this);
    rules_->setColumnCount(3);
    rules_->setHorizontalHeaderLabels({"匹配名称包含", "材料角色", "删除"});
    rules_->setToolTip("规则按材料名称包含关系匹配；例如 material 名包含 white 时可映射到白墨。");
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
    connect(default_role_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](const int index) {
        const QString role = default_role_->itemData(index).toString();
        if (!loading_ && !role.isEmpty()) {
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
    setRoleValue(default_role_, document_->value({"materialRoleMapping", "defaultRole"}).toString("rgb"));
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
        const QString role = combo ? combo->currentData().toString() : QString();
        rule.insert("matchNameContains", match ? match->text() : QString());
        rule.insert("role", role.isEmpty() ? "rgb" : role);
        array.push_back(rule);
    }
    document_->setValue({"materialRoleMapping", "rules"}, array);
}

QComboBox* MaterialRoleMappingEditor::createRoleCombo(const QString& role) {
    auto* combo = new QComboBox(rules_);
    addRoleItems(combo);
    setRoleValue(combo, role);
    combo->setToolTip("选择该规则命中的材料角色。");
    connect(combo, qOverload<int>(&QComboBox::currentIndexChanged), this, &MaterialRoleMappingEditor::writeRules);
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
