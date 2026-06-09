#include "SupportEditor.h"

#include <QFormLayout>
#include <QVBoxLayout>

namespace {

QSpinBox* makeSpin(QWidget* parent, const int max_value = 100000) {
    auto* spin = new QSpinBox(parent);
    spin->setRange(0, max_value);
    return spin;
}

}  // namespace

SupportEditor::SupportEditor(ConfigDocument* document, QWidget* parent) : QWidget(parent), document_(document) {
    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout();
    enabled_ = new QCheckBox("启用支撑", this);
    mode_ = new QComboBox(this);
    mode_->addItems({"none", "bottom_projection", "unsupported_only", "bottom_plus_unsupported", "island_filter"});
    min_island_area_ = makeSpin(this);
    xy_dilation_ = makeSpin(this);
    connectivity_ = makeSpin(this, 8);

    form->addRow(enabled_);
    form->addRow("模式", mode_);
    form->addRow("最小孤岛面积 px", min_island_area_);
    form->addRow("XY 膨胀 px", xy_dilation_);
    form->addRow("连通性", connectivity_);
    layout->addLayout(form);
    layout->addStretch(1);

    connect(document_, &ConfigDocument::changed, this, &SupportEditor::loadFromDocument);
    connect(enabled_, &QCheckBox::toggled, this, [this](const bool checked) {
        if (!loading_) {
            document_->setValue({"support", "enabled"}, checked);
        }
    });
    connect(mode_, &QComboBox::currentTextChanged, this, [this](const QString& mode) {
        if (!loading_) {
            document_->setValue({"support", "mode"}, mode);
        }
    });
    connect(min_island_area_, qOverload<int>(&QSpinBox::valueChanged), this, [this](const int value) {
        if (!loading_) {
            document_->setValue({"support", "minIslandAreaPx"}, value);
        }
    });
    connect(xy_dilation_, qOverload<int>(&QSpinBox::valueChanged), this, [this](const int value) {
        if (!loading_) {
            document_->setValue({"support", "xyDilationPx"}, value);
        }
    });
    connect(connectivity_, qOverload<int>(&QSpinBox::valueChanged), this, [this](const int value) {
        if (!loading_) {
            document_->setValue({"support", "connectivity"}, value);
        }
    });
}

void SupportEditor::loadFromDocument() {
    loading_ = true;
    enabled_->setChecked(document_->value({"support", "enabled"}).toBool());
    mode_->setCurrentText(document_->value({"support", "mode"}).toString("bottom_projection"));
    const QJsonValue min_island = document_->value({"support", "minIslandAreaPx"});
    min_island_area_->setValue(min_island.isDouble() ? min_island.toInt() : document_->value({"support", "minAreaPx"}).toInt());
    xy_dilation_->setValue(document_->value({"support", "xyDilationPx"}).toInt());
    connectivity_->setValue(document_->value({"support", "connectivity"}).toInt(4));
    loading_ = false;
}
