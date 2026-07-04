#include "SupportEditor.h"

#include <QFormLayout>
#include <QVBoxLayout>

namespace {

QSpinBox* makeSpin(QWidget* parent, const int max_value = 100000) {
    auto* spin = new QSpinBox(parent);
    spin->setRange(0, max_value);
    return spin;
}

void addMode(QComboBox* combo, const QString& label, const QString& value) {
    combo->addItem(label, value);
}

void setMode(QComboBox* combo, const QString& value) {
    int index = combo->findData(value);
    if (index < 0) {
        index = combo->count();
        combo->addItem("未知值：" + value, value);
    }
    combo->setCurrentIndex(index);
}

}  // namespace

SupportEditor::SupportEditor(ConfigDocument* document, QWidget* parent) : QWidget(parent), document_(document) {
    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout();
    enabled_ = new QCheckBox("启用支撑", this);
    mode_ = new QComboBox(this);
    addMode(mode_, "不生成支撑", "none");
    addMode(mode_, "底面投影支撑", "bottom_projection");
    addMode(mode_, "仅悬空支撑", "unsupported_only");
    addMode(mode_, "底面 + 悬空支撑", "bottom_projection_plus_unsupported");
    addMode(mode_, "全高度垂直填充", "full_vertical_projection");
    min_island_area_ = makeSpin(this);
    xy_dilation_ = makeSpin(this);
    connectivity_ = makeSpin(this, 8);
    enabled_->setToolTip("启用后生成 S 通道支撑材料。");
    mode_->setToolTip("支撑形态策略：甲片类模型通常使用全高度垂直填充或底面/悬空组合策略。");
    min_island_area_->setToolTip("小于该面积的支撑孤岛会被过滤；0 表示不过滤。");
    xy_dilation_->setToolTip("支撑区域 XY 方向膨胀像素数，用于补偿边缘支撑宽度。");
    connectivity_->setToolTip("孤岛检测连通性，通常使用 4 或 8。");

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
    connect(mode_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](const int index) {
        const QString mode = mode_->itemData(index).toString();
        if (!loading_ && !mode.isEmpty()) {
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
    setMode(mode_, document_->value({"support", "mode"}).toString("bottom_projection"));
    const QJsonValue min_island = document_->value({"support", "minIslandAreaPx"});
    min_island_area_->setValue(min_island.isDouble() ? min_island.toInt() : document_->value({"support", "minAreaPx"}).toInt());
    xy_dilation_->setValue(document_->value({"support", "xyDilationPx"}).toInt());
    connectivity_->setValue(document_->value({"support", "connectivity"}).toInt(4));
    loading_ = false;
}
