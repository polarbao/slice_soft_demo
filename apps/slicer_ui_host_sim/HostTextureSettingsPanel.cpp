#include "HostTextureSettingsPanel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

#include <vector>

namespace
{
QSpinBox* BuildByteSpin(QWidget* parent, const QString& objectName)
{
    auto* spin = new QSpinBox(parent);
    spin->setObjectName(objectName);
    spin->setRange(0, 255);
    return spin;
}

void SelectData(QComboBox* combo, const QString& value)
{
    const int index = combo->findData(value);
    combo->setCurrentIndex(index >= 0 ? index : 0);
}
}

HostTextureSettingsPanel::HostTextureSettingsPanel(QWidget* parent)
    : QWidget(parent)
{
    BuildInterface();
    RefreshEnabledState();
}

void HostTextureSettingsPanel::BuildInterface()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    auto* header = new QWidget(this);
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    m_enabledCheck = new QCheckBox(QStringLiteral("启用生产纹理"), header);
    m_enabledCheck->setObjectName(QStringLiteral("hostTextureEnabledCheck"));
    m_expandButton = new QToolButton(header);
    m_expandButton->setObjectName(QStringLiteral("hostTextureExpandButton"));
    m_expandButton->setText(QStringLiteral("展开"));
    m_expandButton->setCheckable(true);
    headerLayout->addWidget(m_enabledCheck, 1);
    headerLayout->addWidget(m_expandButton);
    layout->addWidget(header);

    m_content = new QWidget(this);
    m_content->setObjectName(QStringLiteral("hostTextureContent"));
    m_content->setVisible(false);
    auto* form = new QFormLayout(m_content);
    form->setContentsMargins(12, 0, 0, 0);
    m_applyModeCombo = new QComboBox(m_content);
    m_applyModeCombo->setObjectName(QStringLiteral("hostTextureApplyModeCombo"));
    m_applyModeCombo->addItem(
        QStringLiteral("顶面纹理投影到实体"), QStringLiteral("solid_volume"));
    m_applyModeCombo->addItem(
        QStringLiteral("仅顶面一层"), QStringLiteral("top_only"));
    m_applyModeCombo->addItem(
        QStringLiteral("顶面纹理带"), QStringLiteral("top_band"));
    m_topLayersSpin = new QSpinBox(m_content);
    m_topLayersSpin->setObjectName(QStringLiteral("hostTextureTopLayersSpin"));
    m_topLayersSpin->setRange(1, 100000);
    m_topLayersSpin->setValue(1);
    m_topLayersSpin->setSuffix(QStringLiteral(" 层"));
    m_samplerCombo = new QComboBox(m_content);
    m_samplerCombo->setObjectName(QStringLiteral("hostTextureSamplerCombo"));
    m_samplerCombo->addItem(QStringLiteral("双线性"), QStringLiteral("bilinear"));
    m_samplerCombo->addItem(QStringLiteral("最近邻"), QStringLiteral("nearest"));
    m_uvAddressCombo = new QComboBox(m_content);
    m_uvAddressCombo->setObjectName(QStringLiteral("hostTextureUvAddressCombo"));
    m_uvAddressCombo->addItem(QStringLiteral("钳制"), QStringLiteral("clamp"));
    m_uvAddressCombo->addItem(QStringLiteral("重复"), QStringLiteral("repeat"));
    m_flipVCheck = new QCheckBox(QStringLiteral("翻转 V 坐标"), m_content);
    m_flipVCheck->setObjectName(QStringLiteral("hostTextureFlipVCheck"));
    m_flipVCheck->setChecked(true);
    m_missingPolicyCombo = new QComboBox(m_content);
    m_missingPolicyCombo->setObjectName(
        QStringLiteral("hostTextureMissingPolicyCombo"));
    m_missingPolicyCombo->addItem(
        QStringLiteral("警告并使用回退色"), QStringLiteral("fallback"));
    m_missingPolicyCombo->addItem(
        QStringLiteral("立即失败"), QStringLiteral("fail_fast"));
    m_nonSurfacePolicyCombo = new QComboBox(m_content);
    m_nonSurfacePolicyCombo->setObjectName(
        QStringLiteral("hostTextureNonSurfacePolicyCombo"));
    m_nonSurfacePolicyCombo->addItem(
        QStringLiteral("使用模型材料"), QStringLiteral("model_material"));
    m_nonSurfacePolicyCombo->addItem(
        QStringLiteral("保持空白"), QStringLiteral("empty"));

    auto* fallbackRow = new QWidget(m_content);
    auto* fallbackLayout = new QHBoxLayout(fallbackRow);
    fallbackLayout->setContentsMargins(0, 0, 0, 0);
    m_fallbackRedSpin = BuildByteSpin(
        fallbackRow, QStringLiteral("hostTextureFallbackRedSpin"));
    m_fallbackGreenSpin = BuildByteSpin(
        fallbackRow, QStringLiteral("hostTextureFallbackGreenSpin"));
    m_fallbackBlueSpin = BuildByteSpin(
        fallbackRow, QStringLiteral("hostTextureFallbackBlueSpin"));
    fallbackLayout->addWidget(m_fallbackRedSpin);
    fallbackLayout->addWidget(m_fallbackGreenSpin);
    fallbackLayout->addWidget(m_fallbackBlueSpin);

    m_whitePolicyCombo = new QComboBox(m_content);
    m_whitePolicyCombo->setObjectName(QStringLiteral("hostTextureWhitePolicyCombo"));
    m_whitePolicyCombo->addItem(
        QStringLiteral("纯白像素关闭材料闭环"), QStringLiteral("fail_closed"));
    m_whitePolicyCombo->addItem(
        QStringLiteral("纯白像素按需补白墨"), QStringLiteral("white_underbase"));
    m_whiteThresholdSpin = BuildByteSpin(
        m_content, QStringLiteral("hostTextureWhiteThresholdSpin"));
    m_whiteValueSpin = BuildByteSpin(
        m_content, QStringLiteral("hostTextureWhiteValueSpin"));
    m_hintLabel = new QLabel(m_content);
    m_hintLabel->setObjectName(QStringLiteral("hostTextureCompatibilityHint"));
    m_hintLabel->setWordWrap(true);

    form->addRow(QStringLiteral("纹理应用"), m_applyModeCombo);
    form->addRow(QStringLiteral("表面纹理层数"), m_topLayersSpin);
    form->addRow(QStringLiteral("采样过滤"), m_samplerCombo);
    form->addRow(QStringLiteral("UV 越界"), m_uvAddressCombo);
    form->addRow(m_flipVCheck);
    form->addRow(QStringLiteral("纹理缺失"), m_missingPolicyCombo);
    form->addRow(QStringLiteral("非表面 RGB"), m_nonSurfacePolicyCombo);
    form->addRow(QStringLiteral("回退 RGB"), fallbackRow);
    form->addRow(QStringLiteral("纯白像素"), m_whitePolicyCombo);
    form->addRow(QStringLiteral("补白阈值"), m_whiteThresholdSpin);
    form->addRow(QStringLiteral("白墨生产值"), m_whiteValueSpin);
    form->addRow(m_hintLabel);
    layout->addWidget(m_content);

    connect(m_expandButton, &QToolButton::toggled,
        this, &HostTextureSettingsPanel::OnExpandedChanged);
    connect(m_enabledCheck, &QCheckBox::toggled,
        this, &HostTextureSettingsPanel::OnSettingsEdited);
    for (QComboBox* combo : {m_applyModeCombo, m_samplerCombo,
             m_uvAddressCombo, m_missingPolicyCombo,
             m_nonSurfacePolicyCombo, m_whitePolicyCombo})
    {
        connect(combo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &HostTextureSettingsPanel::OnSettingsEdited);
    }
    connect(m_flipVCheck, &QCheckBox::toggled,
        this, &HostTextureSettingsPanel::OnSettingsEdited);
    for (QSpinBox* spin : {m_topLayersSpin, m_fallbackRedSpin,
             m_fallbackGreenSpin, m_fallbackBlueSpin,
             m_whiteThresholdSpin, m_whiteValueSpin})
    {
        connect(spin, qOverload<int>(&QSpinBox::valueChanged),
            this, &HostTextureSettingsPanel::OnSettingsEdited);
    }
}

void HostTextureSettingsPanel::SetSettings(
    const hosttexturesettings& settings)
{
    const QList<QObject*> blockers{
        m_enabledCheck, m_applyModeCombo, m_topLayersSpin, m_samplerCombo,
        m_uvAddressCombo, m_flipVCheck, m_missingPolicyCombo,
        m_nonSurfacePolicyCombo, m_fallbackRedSpin, m_fallbackGreenSpin,
        m_fallbackBlueSpin, m_whitePolicyCombo, m_whiteThresholdSpin,
        m_whiteValueSpin};
    std::vector<QSignalBlocker> signalBlockers;
    signalBlockers.reserve(blockers.size());
    for (QObject* object : blockers)
    {
        signalBlockers.emplace_back(object);
    }
    m_enabledCheck->setChecked(settings.enabled);
    SelectData(m_applyModeCombo,
        settings.applymode == HostTextureApplyMode::TopSurfaceOnly
            ? QStringLiteral("top_only")
            : settings.applymode == HostTextureApplyMode::TopSurfaceBand
                ? QStringLiteral("top_band") : QStringLiteral("solid_volume"));
    m_topLayersSpin->setValue(settings.topsurfacelayers);
    SelectData(m_samplerCombo,
        settings.sampler == HostTextureSampler::Nearest
            ? QStringLiteral("nearest") : QStringLiteral("bilinear"));
    SelectData(m_uvAddressCombo,
        settings.uvaddressmode == HostTextureUvAddressMode::Repeat
            ? QStringLiteral("repeat") : QStringLiteral("clamp"));
    m_flipVCheck->setChecked(settings.flipv);
    SelectData(m_missingPolicyCombo,
        settings.missingpolicy == HostTextureMissingPolicy::FailFast
            ? QStringLiteral("fail_fast") : QStringLiteral("fallback"));
    SelectData(m_nonSurfacePolicyCombo,
        settings.nonsurfacepolicy == HostTextureNonSurfacePolicy::Empty
            ? QStringLiteral("empty") : QStringLiteral("model_material"));
    m_fallbackRedSpin->setValue(settings.fallbackred);
    m_fallbackGreenSpin->setValue(settings.fallbackgreen);
    m_fallbackBlueSpin->setValue(settings.fallbackblue);
    SelectData(m_whitePolicyCombo,
        settings.whitepolicy == HostTextureWhitePolicy::WhiteUnderbase
            ? QStringLiteral("white_underbase") : QStringLiteral("fail_closed"));
    m_whiteThresholdSpin->setValue(settings.whiteinkthreshold);
    m_whiteValueSpin->setValue(settings.whitevalue);
    RefreshEnabledState();
}

hosttexturesettings HostTextureSettingsPanel::Settings() const
{
    hosttexturesettings settings;
    settings.enabled = m_enabledCheck->isChecked();
    const QString applyMode = m_applyModeCombo->currentData().toString();
    settings.applymode = applyMode == QStringLiteral("top_only")
        ? HostTextureApplyMode::TopSurfaceOnly
        : applyMode == QStringLiteral("top_band")
            ? HostTextureApplyMode::TopSurfaceBand
            : HostTextureApplyMode::SolidVolumeFromTopSurface;
    settings.topsurfacelayers = m_topLayersSpin->value();
    settings.sampler = m_samplerCombo->currentData().toString()
            == QStringLiteral("nearest")
        ? HostTextureSampler::Nearest : HostTextureSampler::Bilinear;
    settings.uvaddressmode = m_uvAddressCombo->currentData().toString()
            == QStringLiteral("repeat")
        ? HostTextureUvAddressMode::Repeat : HostTextureUvAddressMode::Clamp;
    settings.flipv = m_flipVCheck->isChecked();
    settings.missingpolicy = m_missingPolicyCombo->currentData().toString()
            == QStringLiteral("fail_fast")
        ? HostTextureMissingPolicy::FailFast
        : HostTextureMissingPolicy::WarnAndFallback;
    settings.nonsurfacepolicy =
        m_nonSurfacePolicyCombo->currentData().toString()
                == QStringLiteral("empty")
            ? HostTextureNonSurfacePolicy::Empty
            : HostTextureNonSurfacePolicy::ModelMaterial;
    settings.fallbackred = m_fallbackRedSpin->value();
    settings.fallbackgreen = m_fallbackGreenSpin->value();
    settings.fallbackblue = m_fallbackBlueSpin->value();
    settings.whitepolicy = m_whitePolicyCombo->currentData().toString()
            == QStringLiteral("white_underbase")
        ? HostTextureWhitePolicy::WhiteUnderbase
        : HostTextureWhitePolicy::FailClosed;
    settings.whiteinkthreshold = m_whiteThresholdSpin->value();
    settings.whitevalue = m_whiteValueSpin->value();
    return settings;
}

void HostTextureSettingsPanel::OnExpandedChanged(const bool expanded)
{
    m_content->setVisible(expanded);
    m_expandButton->setText(
        expanded ? QStringLiteral("收起") : QStringLiteral("展开"));
}

void HostTextureSettingsPanel::OnSettingsEdited()
{
    RefreshEnabledState();
    emit SigSettingsChanged();
}

void HostTextureSettingsPanel::RefreshEnabledState()
{
    const bool enabled = m_enabledCheck->isChecked();
    m_content->setEnabled(enabled);
    m_topLayersSpin->setEnabled(
        enabled && m_applyModeCombo->currentData().toString()
            == QStringLiteral("top_band"));
    const bool whiteCarrier = enabled
        && m_whitePolicyCombo->currentData().toString()
            == QStringLiteral("white_underbase");
    m_whiteThresholdSpin->setEnabled(whiteCarrier);
    m_whiteValueSpin->setEnabled(whiteCarrier);
    m_hintLabel->setText(whiteCarrier
        ? QStringLiteral(
            "按需补白仅允许 Legacy 全实体 RGB、RGB 实体材料且禁用材料角色映射。")
        : QStringLiteral(
            "纹理缺失、解码失败或 UV/材质绑定无效由生产配置显式关闭。"));
}
