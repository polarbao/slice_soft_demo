#include "HostSupportSettingsPanel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

namespace
{
HostSupportMode SupportModeFromId(const QString& identifier)
{
    if (identifier == QStringLiteral("unsupported_only"))
    {
        return HostSupportMode::UnsupportedOnly;
    }
    if (identifier == QStringLiteral("bottom_projection_plus_unsupported"))
    {
        return HostSupportMode::BottomProjectionPlusUnsupported;
    }
    if (identifier == QStringLiteral("full_vertical_projection"))
    {
        return HostSupportMode::FullVerticalProjection;
    }
    return HostSupportMode::BottomProjection;
}
}

HostSupportSettingsPanel::HostSupportSettingsPanel(QWidget* parent)
    : QWidget(parent)
{
    BuildInterface();
    RefreshEnabledState();
}

void HostSupportSettingsPanel::BuildInterface()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    auto* header = new QWidget(this);
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    m_enabledCheck = new QCheckBox(QStringLiteral("启用支撑"), header);
    m_enabledCheck->setObjectName(QStringLiteral("hostSupportEnabledCheck"));
    m_enabledCheck->setChecked(true);
    m_expandButton = new QToolButton(header);
    m_expandButton->setObjectName(QStringLiteral("hostSupportExpandButton"));
    m_expandButton->setText(QStringLiteral("收起"));
    m_expandButton->setCheckable(true);
    m_expandButton->setChecked(true);
    headerLayout->addWidget(m_enabledCheck);
    headerLayout->addStretch(1);
    headerLayout->addWidget(m_expandButton);
    layout->addWidget(header);

    m_content = new QWidget(this);
    m_content->setObjectName(QStringLiteral("hostSupportContent"));
    auto* form = new QFormLayout(m_content);
    form->setContentsMargins(12, 0, 0, 0);
    m_modeCombo = new QComboBox(m_content);
    m_modeCombo->setObjectName(QStringLiteral("hostSupportModeCombo"));
    m_modeCombo->addItem(
        QStringLiteral("下表面投影"), QStringLiteral("bottom_projection"));
    m_modeCombo->addItem(
        QStringLiteral("仅孤岛"), QStringLiteral("unsupported_only"));
    m_modeCombo->addItem(
        QStringLiteral("下表面投影 + 孤岛"),
        QStringLiteral("bottom_projection_plus_unsupported"));
    m_modeCombo->addItem(
        QStringLiteral("完整垂直投影"),
        QStringLiteral("full_vertical_projection"));

    m_offsetSpin = new QDoubleSpinBox(m_content);
    m_offsetSpin->setObjectName(QStringLiteral("hostSupportOffsetSpin"));
    m_offsetSpin->setRange(0.0, 10.0);
    m_offsetSpin->setDecimals(3);
    m_offsetSpin->setSingleStep(0.01);
    m_offsetSpin->setSuffix(QStringLiteral(" mm"));
    m_minAreaSpin = new QSpinBox(m_content);
    m_minAreaSpin->setObjectName(QStringLiteral("hostSupportMinAreaSpin"));
    m_minAreaSpin->setRange(0, 1000000);
    m_minAreaSpin->setSuffix(QStringLiteral(" px"));

    m_internalVoidCheck = new QCheckBox(
        QStringLiteral("填充内部闭合镂空"), m_content);
    m_internalVoidCheck->setObjectName(
        QStringLiteral("hostSupportInternalVoidCheck"));
    m_internalVoidCheck->setChecked(true);
    m_internalVoidMinAreaSpin = new QSpinBox(m_content);
    m_internalVoidMinAreaSpin->setObjectName(
        QStringLiteral("hostSupportInternalVoidMinAreaSpin"));
    m_internalVoidMinAreaSpin->setRange(0, 1000000);
    m_internalVoidMinAreaSpin->setValue(16);
    m_internalVoidMinAreaSpin->setSuffix(QStringLiteral(" px"));

    m_baseProjectionCheck = new QCheckBox(
        QStringLiteral("启用最大支撑投影铺底"), m_content);
    m_baseProjectionCheck->setObjectName(
        QStringLiteral("hostSupportBaseProjectionCheck"));
    m_baseProjectionLayersSpin = new QSpinBox(m_content);
    m_baseProjectionLayersSpin->setObjectName(
        QStringLiteral("hostSupportBaseProjectionLayersSpin"));
    m_baseProjectionLayersSpin->setRange(0, 1000);
    m_baseProjectionLayersSpin->setValue(30);
    m_baseProjectionLayersSpin->setSuffix(QStringLiteral(" 层"));

    form->addRow(QStringLiteral("生成模式"), m_modeCombo);
    form->addRow(QStringLiteral("XY 外扩"), m_offsetSpin);
    form->addRow(QStringLiteral("最小支撑面积"), m_minAreaSpin);
    form->addRow(m_internalVoidCheck);
    form->addRow(
        QStringLiteral("镂空最小面积"), m_internalVoidMinAreaSpin);
    form->addRow(m_baseProjectionCheck);
    form->addRow(
        QStringLiteral("铺底层数"), m_baseProjectionLayersSpin);
    layout->addWidget(m_content);

    connect(
        m_enabledCheck,
        &QCheckBox::toggled,
        this,
        &HostSupportSettingsPanel::OnSupportEnabledChanged);
    connect(
        m_expandButton,
        &QToolButton::toggled,
        this,
        &HostSupportSettingsPanel::OnExpandedChanged);
    connect(
        m_internalVoidCheck,
        &QCheckBox::toggled,
        this,
        &HostSupportSettingsPanel::OnInternalVoidChanged);
    connect(
        m_baseProjectionCheck,
        &QCheckBox::toggled,
        this,
        &HostSupportSettingsPanel::OnBaseProjectionChanged);
    connect(
        m_modeCombo,
        qOverload<int>(&QComboBox::currentIndexChanged),
        this,
        &HostSupportSettingsPanel::OnValueChanged);
    connect(
        m_offsetSpin,
        qOverload<double>(&QDoubleSpinBox::valueChanged),
        this,
        &HostSupportSettingsPanel::OnValueChanged);
    for (QSpinBox* spin : {
             m_minAreaSpin,
             m_internalVoidMinAreaSpin,
             m_baseProjectionLayersSpin})
    {
        connect(
            spin,
            qOverload<int>(&QSpinBox::valueChanged),
            this,
            &HostSupportSettingsPanel::OnValueChanged);
    }
}

void HostSupportSettingsPanel::SetSettings(
    const hostsupportsettings& settings)
{
    const QSignalBlocker enabledBlocker(m_enabledCheck);
    const QSignalBlocker modeBlocker(m_modeCombo);
    const QSignalBlocker offsetBlocker(m_offsetSpin);
    const QSignalBlocker areaBlocker(m_minAreaSpin);
    const QSignalBlocker voidBlocker(m_internalVoidCheck);
    const QSignalBlocker voidAreaBlocker(m_internalVoidMinAreaSpin);
    const QSignalBlocker baseBlocker(m_baseProjectionCheck);
    const QSignalBlocker baseLayersBlocker(m_baseProjectionLayersSpin);
    m_enabledCheck->setChecked(settings.enabled);
    const QString modeId = HostEffectiveProfileBuilder::SupportModeId(
        settings.mode);
    const int modeIndex = m_modeCombo->findData(modeId);
    if (modeIndex >= 0)
    {
        m_modeCombo->setCurrentIndex(modeIndex);
    }
    m_offsetSpin->setValue(settings.offsetmm);
    m_minAreaSpin->setValue(settings.minareapx);
    m_internalVoidCheck->setChecked(settings.internalvoid.enabled);
    m_internalVoidMinAreaSpin->setValue(settings.internalvoid.minareapx);
    m_baseProjectionCheck->setChecked(settings.baseprojection.enabled);
    m_baseProjectionLayersSpin->setValue(settings.baseprojection.layercount);
    RefreshEnabledState();
}

hostsupportsettings HostSupportSettingsPanel::Settings() const
{
    hostsupportsettings settings;
    settings.enabled = m_enabledCheck->isChecked();
    settings.mode = settings.enabled
        ? SupportModeFromId(m_modeCombo->currentData().toString())
        : HostSupportMode::None;
    settings.offsetmm = m_offsetSpin->value();
    settings.minareapx = m_minAreaSpin->value();
    settings.internalvoid.enabled = settings.enabled
        && m_internalVoidCheck->isChecked();
    settings.internalvoid.minareapx = m_internalVoidMinAreaSpin->value();
    settings.baseprojection.enabled = settings.enabled
        && m_baseProjectionCheck->isChecked();
    settings.baseprojection.layercount =
        m_baseProjectionLayersSpin->value();
    return settings;
}

void HostSupportSettingsPanel::OnSupportEnabledChanged()
{
    RefreshEnabledState();
    emit SigSettingsChanged();
}

void HostSupportSettingsPanel::OnInternalVoidChanged()
{
    RefreshEnabledState();
    emit SigSettingsChanged();
}

void HostSupportSettingsPanel::OnBaseProjectionChanged()
{
    RefreshEnabledState();
    emit SigSettingsChanged();
}

void HostSupportSettingsPanel::OnExpandedChanged(const bool expanded)
{
    m_content->setVisible(expanded);
    m_expandButton->setText(
        expanded ? QStringLiteral("收起") : QStringLiteral("展开"));
}

void HostSupportSettingsPanel::OnValueChanged()
{
    emit SigSettingsChanged();
}

void HostSupportSettingsPanel::RefreshEnabledState()
{
    const bool enabled = m_enabledCheck->isChecked();
    m_modeCombo->setEnabled(enabled);
    m_offsetSpin->setEnabled(enabled);
    m_minAreaSpin->setEnabled(enabled);
    m_internalVoidCheck->setEnabled(enabled);
    m_internalVoidMinAreaSpin->setEnabled(
        enabled && m_internalVoidCheck->isChecked());
    m_baseProjectionCheck->setEnabled(enabled);
    m_baseProjectionLayersSpin->setEnabled(
        enabled && m_baseProjectionCheck->isChecked());
}
