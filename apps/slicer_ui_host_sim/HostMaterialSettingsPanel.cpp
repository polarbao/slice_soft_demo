#include "HostMaterialSettingsPanel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

namespace
{
HostMaterialStrategy StrategyFromId(const QString& identifier)
{
    if (identifier == QStringLiteral("rgb_white"))
    {
        return HostMaterialStrategy::RgbWhite;
    }
    if (identifier == QStringLiteral("rgb_varnish"))
    {
        return HostMaterialStrategy::RgbVarnish;
    }
    if (identifier == QStringLiteral("rgb_white_varnish"))
    {
        return HostMaterialStrategy::RgbWhiteVarnish;
    }
    if (identifier == QStringLiteral("white_solid"))
    {
        return HostMaterialStrategy::WhiteSolid;
    }
    if (identifier == QStringLiteral("varnish_solid"))
    {
        return HostMaterialStrategy::VarnishSolid;
    }
    return HostMaterialStrategy::RgbSolid;
}

HostMaterialRole RoleFromId(const QString& identifier)
{
    if (identifier == QStringLiteral("white"))
    {
        return HostMaterialRole::White;
    }
    if (identifier == QStringLiteral("varnish"))
    {
        return HostMaterialRole::Varnish;
    }
    if (identifier == QStringLiteral("ignore"))
    {
        return HostMaterialRole::Ignore;
    }
    if (identifier == QStringLiteral("support_candidate"))
    {
        return HostMaterialRole::SupportCandidate;
    }
    return HostMaterialRole::Rgb;
}
}

HostMaterialSettingsPanel::HostMaterialSettingsPanel(QWidget* parent)
    : QWidget(parent)
{
    BuildInterface();
    RefreshEnabledState();
}

void HostMaterialSettingsPanel::BuildInterface()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    auto* header = new QWidget(this);
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    m_strategyCombo = new QComboBox(header);
    m_strategyCombo->setObjectName(QStringLiteral("hostSliceMaterialCombo"));
    m_strategyCombo->addItem(
        QStringLiteral("RGB 实体"), QStringLiteral("rgb_solid"));
    m_strategyCombo->addItem(
        QStringLiteral("RGB + 白墨"), QStringLiteral("rgb_white"));
    m_strategyCombo->addItem(
        QStringLiteral("RGB + 光油"), QStringLiteral("rgb_varnish"));
    m_strategyCombo->addItem(
        QStringLiteral("RGB + 白墨 + 光油"),
        QStringLiteral("rgb_white_varnish"));
    m_strategyCombo->addItem(
        QStringLiteral("单材料白墨"), QStringLiteral("white_solid"));
    m_strategyCombo->addItem(
        QStringLiteral("单材料光油"), QStringLiteral("varnish_solid"));
    m_expandButton = new QToolButton(header);
    m_expandButton->setObjectName(QStringLiteral("hostMaterialExpandButton"));
    m_expandButton->setText(QStringLiteral("展开"));
    m_expandButton->setCheckable(true);
    headerLayout->addWidget(m_strategyCombo, 1);
    headerLayout->addWidget(m_expandButton);
    layout->addWidget(header);

    m_content = new QWidget(this);
    m_content->setObjectName(QStringLiteral("hostMaterialContent"));
    m_content->setVisible(false);
    auto* form = new QFormLayout(m_content);
    form->setContentsMargins(12, 0, 0, 0);

    m_roleMappingCheck = new QCheckBox(
        QStringLiteral("启用输入材料角色映射"), m_content);
    m_roleMappingCheck->setObjectName(
        QStringLiteral("hostMaterialRoleMappingCheck"));
    m_defaultRoleCombo = new QComboBox(m_content);
    m_defaultRoleCombo->setObjectName(
        QStringLiteral("hostMaterialDefaultRoleCombo"));
    m_defaultRoleCombo->addItem(QStringLiteral("RGB"), QStringLiteral("rgb"));
    m_defaultRoleCombo->addItem(
        QStringLiteral("白墨"), QStringLiteral("white"));
    m_defaultRoleCombo->addItem(
        QStringLiteral("光油"), QStringLiteral("varnish"));
    m_defaultRoleCombo->addItem(
        QStringLiteral("忽略"), QStringLiteral("ignore"));
    m_defaultRoleCombo->addItem(
        QStringLiteral("支撑候选"), QStringLiteral("support_candidate"));
    m_mapWhiteNamesCheck = new QCheckBox(
        QStringLiteral("名称含 white 映射为白墨"), m_content);
    m_mapWhiteNamesCheck->setObjectName(
        QStringLiteral("hostMaterialMapWhiteNamesCheck"));
    m_mapWhiteNamesCheck->setChecked(true);
    m_mapVarnishNamesCheck = new QCheckBox(
        QStringLiteral("名称含 varnish 映射为光油"), m_content);
    m_mapVarnishNamesCheck->setObjectName(
        QStringLiteral("hostMaterialMapVarnishNamesCheck"));
    m_mapVarnishNamesCheck->setChecked(true);
    m_allowInputSupportCheck = new QCheckBox(
        QStringLiteral("允许输入支撑材料"), m_content);
    m_allowInputSupportCheck->setObjectName(
        QStringLiteral("hostMaterialAllowInputSupportCheck"));

    m_whiteExpandSpin = new QSpinBox(m_content);
    m_whiteExpandSpin->setObjectName(
        QStringLiteral("hostMaterialWhiteExpandSpin"));
    m_whiteExpandSpin->setRange(0, 100000);
    m_whiteExpandSpin->setSuffix(QStringLiteral(" px"));
    m_whiteShrinkSpin = new QSpinBox(m_content);
    m_whiteShrinkSpin->setObjectName(
        QStringLiteral("hostMaterialWhiteShrinkSpin"));
    m_whiteShrinkSpin->setRange(0, 100000);
    m_whiteShrinkSpin->setSuffix(QStringLiteral(" px"));
    m_varnishTopLayersSpin = new QSpinBox(m_content);
    m_varnishTopLayersSpin->setObjectName(
        QStringLiteral("hostMaterialVarnishTopLayersSpin"));
    m_varnishTopLayersSpin->setRange(1, 100000);
    m_varnishTopLayersSpin->setValue(1);
    m_varnishTopLayersSpin->setSuffix(QStringLiteral(" 层"));
    m_maxOverlapSpin = new QSpinBox(m_content);
    m_maxOverlapSpin->setObjectName(
        QStringLiteral("hostMaterialMaxOverlapSpin"));
    m_maxOverlapSpin->setRange(0, 1000000);
    m_maxOverlapSpin->setSuffix(QStringLiteral(" px"));

    form->addRow(m_roleMappingCheck);
    form->addRow(QStringLiteral("默认材料角色"), m_defaultRoleCombo);
    form->addRow(m_mapWhiteNamesCheck);
    form->addRow(m_mapVarnishNamesCheck);
    form->addRow(m_allowInputSupportCheck);
    form->addRow(QStringLiteral("白墨外扩"), m_whiteExpandSpin);
    form->addRow(QStringLiteral("白墨内缩"), m_whiteShrinkSpin);
    form->addRow(QStringLiteral("光油顶层数"), m_varnishTopLayersSpin);
    form->addRow(QStringLiteral("允许重叠像素"), m_maxOverlapSpin);
    layout->addWidget(m_content);

    connect(
        m_expandButton,
        &QToolButton::toggled,
        this,
        &HostMaterialSettingsPanel::OnExpandedChanged);
    connect(
        m_roleMappingCheck,
        &QCheckBox::toggled,
        this,
        &HostMaterialSettingsPanel::OnRoleMappingChanged);
    for (QCheckBox* check : {
             m_mapWhiteNamesCheck,
             m_mapVarnishNamesCheck,
             m_allowInputSupportCheck})
    {
        connect(
            check,
            &QCheckBox::toggled,
            this,
            &HostMaterialSettingsPanel::OnValueChanged);
    }
    for (QComboBox* combo : {m_strategyCombo, m_defaultRoleCombo})
    {
        connect(
            combo,
            qOverload<int>(&QComboBox::currentIndexChanged),
            this,
            &HostMaterialSettingsPanel::OnValueChanged);
    }
    for (QSpinBox* spin : {
             m_whiteExpandSpin,
             m_whiteShrinkSpin,
             m_varnishTopLayersSpin,
             m_maxOverlapSpin})
    {
        connect(
            spin,
            qOverload<int>(&QSpinBox::valueChanged),
            this,
            &HostMaterialSettingsPanel::OnValueChanged);
    }
}

void HostMaterialSettingsPanel::SetSettings(
    const HostMaterialStrategy strategy,
    const hostmaterialprocesssettings& settings)
{
    const QSignalBlocker strategyBlocker(m_strategyCombo);
    const QSignalBlocker mappingBlocker(m_roleMappingCheck);
    const QSignalBlocker roleBlocker(m_defaultRoleCombo);
    const QSignalBlocker whiteRuleBlocker(m_mapWhiteNamesCheck);
    const QSignalBlocker varnishRuleBlocker(m_mapVarnishNamesCheck);
    const QSignalBlocker supportBlocker(m_allowInputSupportCheck);
    const QSignalBlocker expandBlocker(m_whiteExpandSpin);
    const QSignalBlocker shrinkBlocker(m_whiteShrinkSpin);
    const QSignalBlocker topBlocker(m_varnishTopLayersSpin);
    const QSignalBlocker overlapBlocker(m_maxOverlapSpin);
    const int strategyIndex = m_strategyCombo->findData(
        HostEffectiveProfileBuilder::MaterialStrategyId(strategy));
    if (strategyIndex >= 0)
    {
        m_strategyCombo->setCurrentIndex(strategyIndex);
    }
    m_roleMappingCheck->setChecked(settings.rolemappingenabled);
    const int roleIndex = m_defaultRoleCombo->findData(
        HostEffectiveProfileBuilder::MaterialRoleId(settings.defaultrole));
    if (roleIndex >= 0)
    {
        m_defaultRoleCombo->setCurrentIndex(roleIndex);
    }
    m_mapWhiteNamesCheck->setChecked(settings.mapwhitenames);
    m_mapVarnishNamesCheck->setChecked(settings.mapvarnishnames);
    m_allowInputSupportCheck->setChecked(
        settings.allowinputsupportmaterial);
    m_whiteExpandSpin->setValue(settings.whiteexpandpx);
    m_whiteShrinkSpin->setValue(settings.whiteshrinkpx);
    m_varnishTopLayersSpin->setValue(settings.varnishtoplayers);
    m_maxOverlapSpin->setValue(settings.maxunexpectedoverlappixels);
    RefreshEnabledState();
}

HostMaterialStrategy HostMaterialSettingsPanel::Strategy() const
{
    return StrategyFromId(m_strategyCombo->currentData().toString());
}

hostmaterialprocesssettings HostMaterialSettingsPanel::Settings() const
{
    hostmaterialprocesssettings settings;
    settings.rolemappingenabled = m_roleMappingCheck->isChecked();
    settings.defaultrole = RoleFromId(
        m_defaultRoleCombo->currentData().toString());
    settings.mapwhitenames = m_mapWhiteNamesCheck->isChecked();
    settings.mapvarnishnames = m_mapVarnishNamesCheck->isChecked();
    settings.allowinputsupportmaterial =
        m_allowInputSupportCheck->isChecked();
    settings.whiteexpandpx = m_whiteExpandSpin->value();
    settings.whiteshrinkpx = m_whiteShrinkSpin->value();
    settings.varnishtoplayers = m_varnishTopLayersSpin->value();
    settings.maxunexpectedoverlappixels = m_maxOverlapSpin->value();
    return settings;
}

void HostMaterialSettingsPanel::OnRoleMappingChanged()
{
    RefreshEnabledState();
    emit SigSettingsChanged();
}

void HostMaterialSettingsPanel::OnExpandedChanged(const bool expanded)
{
    m_content->setVisible(expanded);
    m_expandButton->setText(
        expanded ? QStringLiteral("收起") : QStringLiteral("展开"));
}

void HostMaterialSettingsPanel::OnValueChanged()
{
    emit SigSettingsChanged();
}

void HostMaterialSettingsPanel::RefreshEnabledState()
{
    const bool enabled = m_roleMappingCheck->isChecked();
    m_defaultRoleCombo->setEnabled(enabled);
    m_mapWhiteNamesCheck->setEnabled(enabled);
    m_mapVarnishNamesCheck->setEnabled(enabled);
    m_allowInputSupportCheck->setEnabled(enabled);
}
