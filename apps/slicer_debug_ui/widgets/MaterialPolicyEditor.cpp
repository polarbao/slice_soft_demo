#include "MaterialPolicyEditor.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QVBoxLayout>

namespace {

QSpinBox* makeByteSpin(QWidget* parent) {
    auto* spin = new QSpinBox(parent);
    spin->setRange(0, 255);
    return spin;
}

QSpinBox* makeLayerSpin(QWidget* parent) {
    auto* spin = new QSpinBox(parent);
    spin->setRange(0, 100000);
    return spin;
}

void addComboOption(QComboBox* combo, const QString& label, const QString& value) {
    combo->addItem(label, value);
}

QString comboValue(const QComboBox* combo, const int index) {
    return combo->itemData(index).toString();
}

void setComboValue(QComboBox* combo, const QString& value) {
    int index = combo->findData(value);
    if (index < 0) {
        index = combo->count();
        combo->addItem("未知值：" + value, value);
    }
    combo->setCurrentIndex(index);
}

}  // namespace

MaterialPolicyEditor::MaterialPolicyEditor(ConfigDocument* document, QWidget* parent)
    : QWidget(parent), document_(document) {
    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout();
    enabled_ = new QCheckBox("启用材料策略", this);
    rgb_enabled_ = new QCheckBox("RGB 启用", this);
    rgb_source_ = new QComboBox(this);
    addComboOption(rgb_source_, "纹理或备用色", "texture_or_fallback");
    addComboOption(rgb_source_, "模型材料", "modelMaterial");
    white_enabled_ = new QCheckBox("白墨启用", this);
    white_mode_ = new QComboBox(this);
    addComboOption(white_mode_, "白墨底层", "underbase");
    addComboOption(white_mode_, "禁用", "disabled");
    addComboOption(white_mode_, "覆盖整个模型", "all_model");
    white_layers_ = new QLineEdit(this);
    white_value_ = makeByteSpin(this);
    varnish_enabled_ = new QCheckBox("光油启用", this);
    varnish_mode_ = new QComboBox(this);
    addComboOption(varnish_mode_, "顶部 N 层", "top_n_layers");
    addComboOption(varnish_mode_, "覆盖整个模型", "all_model");
    addComboOption(varnish_mode_, "禁用", "disabled");
    varnish_top_layers_ = makeLayerSpin(this);
    varnish_value_ = makeByteSpin(this);
    conflict_policy_ = new QComboBox(this);
    addComboOption(conflict_policy_, "模型材料优先于支撑", "model_material_over_support");

    enabled_->setToolTip("启用后按本页规则写入 RGB/W/V 材料通道。");
    rgb_enabled_->setToolTip("控制是否写入 RGB 模型材料或纹理颜色。");
    rgb_source_->setToolTip("纹理或备用色：优先贴图颜色；模型材料：使用配置中的 modelMaterial.rgb。");
    white_enabled_->setToolTip("控制是否写入 W 通道白墨。");
    white_mode_->setToolTip("白墨生成模式：底层、整个模型或禁用。");
    white_layers_->setToolTip("白墨层范围表达式，留空时由模式和工艺 Profile 决定。");
    white_value_->setToolTip("W 通道打印值；RGBWSV 协议中 0 表示打印，255 表示不打印。");
    varnish_enabled_->setToolTip("控制是否写入 V 通道光油。");
    varnish_mode_->setToolTip("光油生成模式：顶部 N 层、整个模型或禁用。");
    varnish_top_layers_->setToolTip("光油顶部层数，仅在“顶部 N 层”模式下生效。");
    varnish_value_->setToolTip("V 通道打印值；RGBWSV 协议中 0 表示打印，255 表示不打印。");
    conflict_policy_->setToolTip("当模型材料与支撑重叠时的优先级。当前长期策略是模型材料优先于支撑。");

    form->addRow(enabled_);
    form->addRow(rgb_enabled_);
    form->addRow("RGB 来源", rgb_source_);
    form->addRow(white_enabled_);
    form->addRow("白墨模式", white_mode_);
    form->addRow("白墨层", white_layers_);
    form->addRow("白墨值", white_value_);
    form->addRow(varnish_enabled_);
    form->addRow("光油模式", varnish_mode_);
    form->addRow("光油顶部层数", varnish_top_layers_);
    form->addRow("光油值", varnish_value_);
    form->addRow("冲突策略", conflict_policy_);
    layout->addLayout(form);
    layout->addStretch(1);

    bind();
}

void MaterialPolicyEditor::loadFromDocument() {
    loading_ = true;
    setBool({"materialPolicy", "enabled"}, enabled_);
    setBool({"materialPolicy", "rgb", "enabled"}, rgb_enabled_);
    setCombo({"materialPolicy", "rgb", "source"}, rgb_source_, "texture_or_fallback");
    setBool({"materialPolicy", "white", "enabled"}, white_enabled_);
    setCombo({"materialPolicy", "white", "mode"}, white_mode_, "disabled");
    setString({"materialPolicy", "white", "layers"}, white_layers_);
    setInt({"materialPolicy", "white", "value"}, white_value_);
    setBool({"materialPolicy", "varnish", "enabled"}, varnish_enabled_);
    setCombo({"materialPolicy", "varnish", "mode"}, varnish_mode_, "disabled");
    setInt({"materialPolicy", "varnish", "topLayers"}, varnish_top_layers_);
    setInt({"materialPolicy", "varnish", "value"}, varnish_value_);
    setCombo({"materialPolicy", "conflictPolicy"}, conflict_policy_, "model_material_over_support");
    loading_ = false;
}

void MaterialPolicyEditor::bind() {
    connect(document_, &ConfigDocument::changed, this, &MaterialPolicyEditor::loadFromDocument);
    auto bind_check = [this](QCheckBox* check, const QStringList& path) {
        connect(check, &QCheckBox::toggled, this, [this, path](const bool checked) {
            if (!loading_) {
                document_->setValue(path, checked);
            }
        });
    };
    auto bind_edit = [this](QLineEdit* edit, const QStringList& path) {
        connect(edit, &QLineEdit::editingFinished, this, [this, edit, path]() {
            if (!loading_) {
                document_->setValue(path, edit->text());
            }
        });
    };
    auto bind_spin = [this](QSpinBox* spin, const QStringList& path) {
        connect(spin, qOverload<int>(&QSpinBox::valueChanged), this, [this, path](const int value) {
            if (!loading_) {
                document_->setValue(path, value);
            }
        });
    };

    bind_check(enabled_, {"materialPolicy", "enabled"});
    bind_check(rgb_enabled_, {"materialPolicy", "rgb", "enabled"});
    connect(rgb_source_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](const int index) {
        const QString value = comboValue(rgb_source_, index);
        if (!loading_ && !value.isEmpty()) {
            document_->setValue({"materialPolicy", "rgb", "source"}, value);
        }
    });
    bind_check(white_enabled_, {"materialPolicy", "white", "enabled"});
    connect(white_mode_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](const int index) {
        const QString value = comboValue(white_mode_, index);
        if (!loading_ && !value.isEmpty()) {
            document_->setValue({"materialPolicy", "white", "mode"}, value);
        }
    });
    bind_edit(white_layers_, {"materialPolicy", "white", "layers"});
    bind_spin(white_value_, {"materialPolicy", "white", "value"});
    bind_check(varnish_enabled_, {"materialPolicy", "varnish", "enabled"});
    connect(varnish_mode_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](const int index) {
        const QString value = comboValue(varnish_mode_, index);
        if (!loading_ && !value.isEmpty()) {
            document_->setValue({"materialPolicy", "varnish", "mode"}, value);
        }
    });
    bind_spin(varnish_top_layers_, {"materialPolicy", "varnish", "topLayers"});
    bind_spin(varnish_value_, {"materialPolicy", "varnish", "value"});
    connect(conflict_policy_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](const int index) {
        const QString value = comboValue(conflict_policy_, index);
        if (!loading_ && !value.isEmpty()) {
            document_->setValue({"materialPolicy", "conflictPolicy"}, value);
        }
    });
}

void MaterialPolicyEditor::setString(const QStringList& path, QLineEdit* edit) {
    edit->setText(document_->value(path).toString());
}

void MaterialPolicyEditor::setBool(const QStringList& path, QCheckBox* check) {
    check->setChecked(document_->value(path).toBool());
}

void MaterialPolicyEditor::setInt(const QStringList& path, QSpinBox* spin) {
    spin->setValue(document_->value(path).toInt());
}

void MaterialPolicyEditor::setCombo(const QStringList& path, QComboBox* combo, const QString& fallback) {
    QString value = document_->value(path).toString();
    if (value.isEmpty()) {
        value = fallback;
    }
    setComboValue(combo, value);
}
