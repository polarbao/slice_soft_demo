#include "MaterialProcessProfileEditor.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QVBoxLayout>

namespace {

QSpinBox* makePxSpin(QWidget* parent) {
    auto* spin = new QSpinBox(parent);
    spin->setRange(0, 100000);
    return spin;
}

QGroupBox* group(const QString& title, QWidget* parent) {
    return new QGroupBox(title, parent);
}

}  // namespace

MaterialProcessProfileEditor::MaterialProcessProfileEditor(ConfigDocument* document, QWidget* parent)
    : QWidget(parent), document_(document) {
    auto* layout = new QVBoxLayout(this);

    auto* base = group("基础", this);
    auto* base_form = new QFormLayout(base);
    enabled_ = new QCheckBox("启用 MaterialProcessProfile", base);
    name_ = new QLineEdit(base);
    target_ = new QLineEdit(base);
    base_form->addRow(enabled_);
    base_form->addRow("名称", name_);
    base_form->addRow("目标", target_);
    layout->addWidget(base);

    auto* channel = group("通道策略", this);
    auto* channel_form = new QFormLayout(channel);
    rgb_enabled_ = new QCheckBox("RGB 启用", channel);
    white_enabled_ = new QCheckBox("白墨启用", channel);
    white_coverage_ = new QLineEdit(channel);
    white_expand_ = makePxSpin(channel);
    white_shrink_ = makePxSpin(channel);
    varnish_enabled_ = new QCheckBox("光油启用", channel);
    varnish_top_layers_ = makePxSpin(channel);
    support_expected_ = new QCheckBox("期望支撑存在", channel);
    channel_form->addRow(rgb_enabled_);
    channel_form->addRow(white_enabled_);
    channel_form->addRow("白墨覆盖", white_coverage_);
    channel_form->addRow("白墨扩展 px", white_expand_);
    channel_form->addRow("白墨收缩 px", white_shrink_);
    channel_form->addRow(varnish_enabled_);
    channel_form->addRow("光油顶部层数", varnish_top_layers_);
    channel_form->addRow(support_expected_);
    layout->addWidget(channel);

    auto* validation = group("校验要求", this);
    auto* validation_form = new QFormLayout(validation);
    require_rgb_ = new QCheckBox("要求 RGB 像素", validation);
    require_white_ = new QCheckBox("要求白墨像素", validation);
    require_varnish_ = new QCheckBox("要求光油像素", validation);
    require_support_ = new QCheckBox("要求支撑像素", validation);
    validation_form->addRow(require_rgb_);
    validation_form->addRow(require_white_);
    validation_form->addRow(require_varnish_);
    validation_form->addRow(require_support_);
    layout->addWidget(validation);
    layout->addStretch(1);

    bind();
}

void MaterialProcessProfileEditor::loadFromDocument() {
    loading_ = true;
    setBool({"materialProcessProfile", "enabled"}, enabled_);
    setString({"materialProcessProfile", "name"}, name_);
    setString({"materialProcessProfile", "target"}, target_);
    setBool({"materialProcessProfile", "rgb", "enabled"}, rgb_enabled_);
    setBool({"materialProcessProfile", "white", "enabled"}, white_enabled_);
    setString({"materialProcessProfile", "white", "coverage"}, white_coverage_);
    setInt({"materialProcessProfile", "white", "expandPx"}, white_expand_);
    setInt({"materialProcessProfile", "white", "shrinkPx"}, white_shrink_);
    setBool({"materialProcessProfile", "varnish", "enabled"}, varnish_enabled_);
    setInt({"materialProcessProfile", "varnish", "topLayers"}, varnish_top_layers_);
    setBool({"materialProcessProfile", "support", "expected"}, support_expected_);
    setBool({"materialProcessProfile", "validation", "requireRgbPixels"}, require_rgb_);
    setBool({"materialProcessProfile", "validation", "requireWhitePixels"}, require_white_);
    setBool({"materialProcessProfile", "validation", "requireVarnishPixels"}, require_varnish_);
    setBool({"materialProcessProfile", "validation", "requireSupportPixels"}, require_support_);
    loading_ = false;
}

void MaterialProcessProfileEditor::bind() {
    connect(document_, &ConfigDocument::changed, this, &MaterialProcessProfileEditor::loadFromDocument);
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

    bind_check(enabled_, {"materialProcessProfile", "enabled"});
    bind_edit(name_, {"materialProcessProfile", "name"});
    bind_edit(target_, {"materialProcessProfile", "target"});
    bind_check(rgb_enabled_, {"materialProcessProfile", "rgb", "enabled"});
    bind_check(white_enabled_, {"materialProcessProfile", "white", "enabled"});
    bind_edit(white_coverage_, {"materialProcessProfile", "white", "coverage"});
    bind_spin(white_expand_, {"materialProcessProfile", "white", "expandPx"});
    bind_spin(white_shrink_, {"materialProcessProfile", "white", "shrinkPx"});
    bind_check(varnish_enabled_, {"materialProcessProfile", "varnish", "enabled"});
    bind_spin(varnish_top_layers_, {"materialProcessProfile", "varnish", "topLayers"});
    bind_check(support_expected_, {"materialProcessProfile", "support", "expected"});
    bind_check(require_rgb_, {"materialProcessProfile", "validation", "requireRgbPixels"});
    bind_check(require_white_, {"materialProcessProfile", "validation", "requireWhitePixels"});
    bind_check(require_varnish_, {"materialProcessProfile", "validation", "requireVarnishPixels"});
    bind_check(require_support_, {"materialProcessProfile", "validation", "requireSupportPixels"});
}

void MaterialProcessProfileEditor::setString(const QStringList& path, QLineEdit* edit) {
    edit->setText(document_->value(path).toString());
}

void MaterialProcessProfileEditor::setBool(const QStringList& path, QCheckBox* check) {
    check->setChecked(document_->value(path).toBool());
}

void MaterialProcessProfileEditor::setInt(const QStringList& path, QSpinBox* spin) {
    spin->setValue(document_->value(path).toInt());
}

void MaterialProcessProfileEditor::setCombo(const QStringList& path, QComboBox* combo) {
    combo->setCurrentText(document_->value(path).toString());
}
