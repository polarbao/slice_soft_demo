// 缩裹材料 T 通道的拓扑放宽控件。
//
// 为什么需要它：该上限原本只存在于部署的 JSON 工艺文件里，用户在 UI 上既看不到也改不了。
// 而工作区会把它持久化——在该键尚不存在的旧版本上，恢复时取默认 0 并在退出时写回，
// 于是这个「从未被谁选过的 0」变成了持久值，此后每次启动都压过工艺文件里的 8；
// 又因为下拉框重选同一个预设不触发变更信号，用户无从把正确值放回去，
// 表现为切片始终报「3 boundary edges exceed the configured limit of 0」。
//
// 把它提到设置层解决的是这个结构性问题：值可见、可改、且由用户的显式选择持久化，
// 而不是由一个缺失键的默认值悄悄决定。

#include "HostSliceSettingsPanel.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>

void HostSliceSettingsPanel::AttachTransferTopologyControl(QVBoxLayout* layout)
{
    auto* group = new QGroupBox(
        QStringLiteral("宿主 Profile 缩裹 T 通道｜拓扑放宽"), this);
    group->setObjectName(QStringLiteral("hostTransferTopologyGroup"));
    auto* row = new QHBoxLayout(group);
    row->setContentsMargins(8, 8, 8, 8);
    auto* label = new QLabel(QStringLiteral("真开边上限"), group);
    label->setToolTip(QStringLiteral(
        "允许缩裹材质带有的真开边数上限。0 表示不放行任何开边（保守默认）。\n"
        "08/09/08-03/08-04 等资产的材质 02 各有 3 条真开边，需设为 3 或更大才能切片。\n"
        "该放宽仅在逐列射线奇偶检查通过时生效，不会绕过区间求解的正确性门。"));
    m_transferBoundaryEdgeSpin = new QSpinBox(group);
    m_transferBoundaryEdgeSpin->setObjectName(
        QStringLiteral("hostTransferBoundaryEdgeSpin"));
    m_transferBoundaryEdgeSpin->setRange(0, 4096);
    m_transferBoundaryEdgeSpin->setToolTip(label->toolTip());
    row->addWidget(label);
    row->addWidget(m_transferBoundaryEdgeSpin, 1);
    connect(
        m_transferBoundaryEdgeSpin,
        qOverload<int>(&QSpinBox::valueChanged),
        this,
        &HostSliceSettingsPanel::OnSettingsEdited);
    layout->addWidget(group);
}

void HostSliceSettingsPanel::SyncTransferTopologyControl()
{
    if (m_transferBoundaryEdgeSpin == nullptr)
    {
        return;
    }
    const QSignalBlocker blocker(m_transferBoundaryEdgeSpin);
    m_transferBoundaryEdgeSpin->setValue(m_transferChannel.maxboundaryedges);
}

int HostSliceSettingsPanel::ReadTransferBoundaryEdgeLimit() const
{
    return m_transferBoundaryEdgeSpin == nullptr
        ? m_transferChannel.maxboundaryedges
        : m_transferBoundaryEdgeSpin->value();
}
