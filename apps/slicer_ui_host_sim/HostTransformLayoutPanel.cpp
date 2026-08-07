#include "HostTransformLayoutPanel.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace
{
QDoubleSpinBox* CreateDistanceSpin(const QString& objectName, QWidget* parent)
{
    auto* spin = new QDoubleSpinBox(parent);
    spin->setObjectName(objectName);
    spin->setRange(-1000.0, 1000.0);
    spin->setDecimals(2);
    spin->setSingleStep(0.1);
    spin->setSuffix(QStringLiteral(" mm"));
    spin->setKeyboardTracking(false);
    return spin;
}

QDoubleSpinBox* CreateGapSpin(const QString& objectName, QWidget* parent)
{
    auto* spin = new QDoubleSpinBox(parent);
    spin->setObjectName(objectName);
    spin->setRange(0.0, 1000.0);
    spin->setDecimals(2);
    spin->setSingleStep(0.1);
    spin->setValue(10.0);
    spin->setSuffix(QStringLiteral(" mm"));
    spin->setKeyboardTracking(false);
    return spin;
}
}

HostTransformLayoutPanel::HostTransformLayoutPanel(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("hostTransformLayoutPanel"));
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(6, 6, 6, 6);
    root->setSpacing(8);

    m_selectionLabel = new QLabel(QStringLiteral("未选择模型实例。"), this);
    m_selectionLabel->setObjectName(
        QStringLiteral("hostTransformSelectionLabel"));
    m_selectionLabel->setWordWrap(true);
    root->addWidget(m_selectionLabel);

    auto* transformGroup = new QGroupBox(QStringLiteral("实例变换"), this);
    auto* transformForm = new QFormLayout(transformGroup);
    m_deltaXSpin = CreateDistanceSpin(
        QStringLiteral("hostTransformDeltaXSpin"), transformGroup);
    m_deltaYSpin = CreateDistanceSpin(
        QStringLiteral("hostTransformDeltaYSpin"), transformGroup);
    m_deltaZSpin = CreateDistanceSpin(
        QStringLiteral("hostTransformDeltaZSpin"), transformGroup);
    m_rotateZSpin = new QDoubleSpinBox(transformGroup);
    m_rotateZSpin->setObjectName(QStringLiteral("hostTransformRotateZSpin"));
    m_rotateZSpin->setRange(-360.0, 360.0);
    m_rotateZSpin->setDecimals(2);
    m_rotateZSpin->setSingleStep(1.0);
    m_rotateZSpin->setSuffix(QStringLiteral(" deg"));
    m_rotateZSpin->setKeyboardTracking(false);
    m_scaleSpin = new QDoubleSpinBox(transformGroup);
    m_scaleSpin->setObjectName(QStringLiteral("hostTransformScaleSpin"));
    m_scaleSpin->setRange(0.01, 100.0);
    m_scaleSpin->setDecimals(3);
    m_scaleSpin->setSingleStep(0.01);
    m_scaleSpin->setValue(1.0);
    m_scaleSpin->setKeyboardTracking(false);
    m_mirrorXCheck = new QCheckBox(QStringLiteral("翻转 X"), transformGroup);
    m_mirrorXCheck->setObjectName(QStringLiteral("hostTransformMirrorXCheck"));
    m_mirrorYCheck = new QCheckBox(QStringLiteral("翻转 Y"), transformGroup);
    m_mirrorYCheck->setObjectName(QStringLiteral("hostTransformMirrorYCheck"));
    auto* mirrorLayout = new QVBoxLayout();
    mirrorLayout->addWidget(m_mirrorXCheck);
    mirrorLayout->addWidget(m_mirrorYCheck);
    transformForm->addRow(QStringLiteral("X 增量"), m_deltaXSpin);
    transformForm->addRow(QStringLiteral("Y 增量"), m_deltaYSpin);
    transformForm->addRow(QStringLiteral("Z 增量"), m_deltaZSpin);
    transformForm->addRow(QStringLiteral("绕 Z 旋转"), m_rotateZSpin);
    transformForm->addRow(QStringLiteral("等比缩放因子"), m_scaleSpin);
    transformForm->addRow(QStringLiteral("镜像"), mirrorLayout);
    m_applyTransformButton = new QPushButton(
        QStringLiteral("提交选中实例变换"), transformGroup);
    m_applyTransformButton->setObjectName(
        QStringLiteral("hostTransformApplyButton"));
    m_applyTransformButton->setToolTip(QStringLiteral(
        "输入仅在宿主本地编辑；点击后经一次原子 Commit 应用于全部选中实例"));
    transformForm->addRow(m_applyTransformButton);
    root->addWidget(transformGroup);

    auto* layoutGroup = new QGroupBox(QStringLiteral("规则排版"), this);
    auto* layoutForm = new QFormLayout(layoutGroup);
    m_columnsSpin = new QSpinBox(layoutGroup);
    m_columnsSpin->setObjectName(QStringLiteral("hostLayoutColumnsSpin"));
    m_columnsSpin->setRange(1, 11);
    m_columnsSpin->setValue(11);
    m_columnsSpin->setKeyboardTracking(false);
    m_rowsSpin = new QSpinBox(layoutGroup);
    m_rowsSpin->setObjectName(QStringLiteral("hostLayoutRowsSpin"));
    m_rowsSpin->setRange(1, 2);
    m_rowsSpin->setValue(2);
    m_rowsSpin->setKeyboardTracking(false);
    m_columnGapSpin = CreateGapSpin(
        QStringLiteral("hostLayoutColumnGapSpin"), layoutGroup);
    m_rowGapSpin = CreateGapSpin(
        QStringLiteral("hostLayoutRowGapSpin"), layoutGroup);
    layoutForm->addRow(QStringLiteral("每行模型数"), m_columnsSpin);
    layoutForm->addRow(QStringLiteral("最大行数"), m_rowsSpin);
    layoutForm->addRow(QStringLiteral("列间净距"), m_columnGapSpin);
    layoutForm->addRow(QStringLiteral("行间净距"), m_rowGapSpin);
    m_applyLayoutButton = new QPushButton(
        QStringLiteral("执行规则排版"), layoutGroup);
    m_applyLayoutButton->setObjectName(QStringLiteral("hostLayoutApplyButton"));
    m_applyLayoutButton->setToolTip(QStringLiteral(
        "排版算法由切片能力模块执行；宿主不自行计算实例落位"));
    layoutForm->addRow(m_applyLayoutButton);
    root->addWidget(layoutGroup);

    m_sceneLabel = new QLabel(QStringLiteral("场景为空。"), this);
    m_sceneLabel->setObjectName(QStringLiteral("hostSceneEditSummaryLabel"));
    m_sceneLabel->setWordWrap(true);
    root->addWidget(m_sceneLabel);
    root->addStretch(1);

    connect(
        m_applyTransformButton,
        &QPushButton::clicked,
        this,
        &HostTransformLayoutPanel::OnApplyTransform);
    connect(
        m_applyLayoutButton,
        &QPushButton::clicked,
        this,
        &HostTransformLayoutPanel::OnApplyLayout);
    UpdateControls();
}

void HostTransformLayoutPanel::SetSelectedInstances(
    const QStringList& instanceIds)
{
    m_selectedInstanceIds = instanceIds;
    m_selectionLabel->setText(
        instanceIds.isEmpty()
            ? QStringLiteral("未选择模型实例。")
            : QStringLiteral("将变换 %1 个选中实例。")
                  .arg(instanceIds.size()));
    UpdateControls();
}

void HostTransformLayoutPanel::SetSceneState(
    const int instanceCount,
    const quint64 sceneRevision)
{
    m_instanceCount = instanceCount;
    m_sceneRevision = sceneRevision;
    m_sceneLabel->setText(
        instanceCount > 0
            ? QStringLiteral("场景实例 %1 / 22 · revision=%2")
                  .arg(instanceCount)
                  .arg(sceneRevision)
            : QStringLiteral("场景为空。"));
    UpdateControls();
}

void HostTransformLayoutPanel::SetCommandsEnabled(const bool enabled)
{
    m_commandsEnabled = enabled;
    UpdateControls();
}

void HostTransformLayoutPanel::ResetTransformInputs()
{
    m_deltaXSpin->setValue(0.0);
    m_deltaYSpin->setValue(0.0);
    m_deltaZSpin->setValue(0.0);
    m_rotateZSpin->setValue(0.0);
    m_scaleSpin->setValue(1.0);
    m_mirrorXCheck->setChecked(false);
    m_mirrorYCheck->setChecked(false);
}

void HostTransformLayoutPanel::OnApplyTransform()
{
    emit SigTransformRequested(
        m_selectedInstanceIds,
        m_deltaXSpin->value(),
        m_deltaYSpin->value(),
        m_deltaZSpin->value(),
        m_rotateZSpin->value(),
        m_scaleSpin->value(),
        m_mirrorXCheck->isChecked(),
        m_mirrorYCheck->isChecked());
}

void HostTransformLayoutPanel::OnApplyLayout()
{
    emit SigLayoutRequested(
        m_columnsSpin->value(),
        m_rowsSpin->value(),
        m_columnGapSpin->value(),
        m_rowGapSpin->value());
}

void HostTransformLayoutPanel::UpdateControls()
{
    const bool hasSelection = !m_selectedInstanceIds.isEmpty();
    m_applyTransformButton->setEnabled(m_commandsEnabled && hasSelection);
    m_applyLayoutButton->setEnabled(
        m_commandsEnabled && m_instanceCount > 0);
}
