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

}  // namespace

MaterialPolicyEditor::MaterialPolicyEditor(ConfigDocument* document, QWidget* parent)
    : QWidget(parent), document_(document) {
    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout();
    enabled_ = new QCheckBox("启用 MaterialPolicy", this);
    rgb_enabled_ = new QCheckBox("RGB 启用", this);
    rgb_source_ = new QLineEdit(this);
    white_enabled_ = new QCheckBox("白墨启用", this);
    white_mode_ = new QComboBox(this);
    white_mode_->addItems({"underbase", "disabled", "all_model"});
    white_layers_ = new QLineEdit(this);
    white_value_ = makeByteSpin(this);
    varnish_enabled_ = new QCheckBox("光油启用", this);
    varnish_mode_ = new QComboBox(this);
    varnish_mode_->addItems({"top_n_layers", "all_model", "disabled"});
    varnish_top_layers_ = makeLayerSpin(this);
    varnish_value_ = makeByteSpin(this);
    conflict_policy_ = new QLineEdit(this);

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
    setString({"materialPolicy", "rgb", "source"}, rgb_source_);
    setBool({"materialPolicy", "white", "enabled"}, white_enabled_);
    white_mode_->setCurrentText(document_->value({"materialPolicy", "white", "mode"}).toString());
    setString({"materialPolicy", "white", "layers"}, white_layers_);
    setInt({"materialPolicy", "white", "value"}, white_value_);
    setBool({"materialPolicy", "varnish", "enabled"}, varnish_enabled_);
    varnish_mode_->setCurrentText(document_->value({"materialPolicy", "varnish", "mode"}).toString());
    setInt({"materialPolicy", "varnish", "topLayers"}, varnish_top_layers_);
    setInt({"materialPolicy", "varnish", "value"}, varnish_value_);
    setString({"materialPolicy", "conflictPolicy"}, conflict_policy_);
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
    bind_edit(rgb_source_, {"materialPolicy", "rgb", "source"});
    bind_check(white_enabled_, {"materialPolicy", "white", "enabled"});
    connect(white_mode_, &QComboBox::currentTextChanged, this, [this](const QString& value) {
        if (!loading_) {
            document_->setValue({"materialPolicy", "white", "mode"}, value);
        }
    });
    bind_edit(white_layers_, {"materialPolicy", "white", "layers"});
    bind_spin(white_value_, {"materialPolicy", "white", "value"});
    bind_check(varnish_enabled_, {"materialPolicy", "varnish", "enabled"});
    connect(varnish_mode_, &QComboBox::currentTextChanged, this, [this](const QString& value) {
        if (!loading_) {
            document_->setValue({"materialPolicy", "varnish", "mode"}, value);
        }
    });
    bind_spin(varnish_top_layers_, {"materialPolicy", "varnish", "topLayers"});
    bind_spin(varnish_value_, {"materialPolicy", "varnish", "value"});
    bind_edit(conflict_policy_, {"materialPolicy", "conflictPolicy"});
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
