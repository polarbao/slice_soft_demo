#include "SliceSettingsModel.h"

#include <cmath>

namespace
{

SliceSettingsState MakeCommonDefaults()
{
    SliceSettingsState state;
    state.layerthicknessmm = 0.01;
    state.modelfillmaterial = ModelFillMaterial::White;
    state.support.enabled = true;
    state.support.placement = SupportPlacement::Lower;
    state.support.internalvoidenabled = true;
    state.support.internalvoidminareapx = 16;
    state.surfacevarnish.enabled = false;
    state.surfacevarnish.thicknesspx = 0;
    state.outervarnish.enabled = false;
    state.outervarnish.thicknessmm = 0.0;
    state.outervarnish.pixelpitchum = 42.3;
    state.preview.enabled = true;
    state.preview.interval = 10;
    state.enginerole = SliceEngineRole::LegacyProduction;
    return state;
}

}  // namespace

bool SliceSettingsValidationResult::IsValid() const
{
    return errors.isEmpty();
}

SliceSettingsModel::SliceSettingsModel()
    : m_state(MakeCommonDefaults())
{
}

bool SliceSettingsModel::ApplyProfileDefaults(const QString& profileId)
{
    SliceSettingsState state = MakeCommonDefaults();
    state.profileid = profileId;

    if (profileId == QStringLiteral("textured_nail_rgb_white_lower_support"))
    {
        state.modelfillmaterial = ModelFillMaterial::White;
    }
    else if (profileId == QStringLiteral("textured_nail_rgb_only_lower_support"))
    {
        state.modelfillmaterial = ModelFillMaterial::Rgb;
    }
    else if (profileId == QStringLiteral("textured_nail_rgb_varnish_lower_support"))
    {
        state.modelfillmaterial = ModelFillMaterial::Varnish;
    }
    else if (profileId == QStringLiteral("single_material_relief"))
    {
        state.modelfillmaterial = ModelFillMaterial::White;
    }
    else if (profileId == QStringLiteral("production_rgb_inspection"))
    {
        state.modelfillmaterial = ModelFillMaterial::White;
        state.preview.interval = 1;
    }
    else
    {
        return false;
    }

    m_state = state;
    return true;
}

void SliceSettingsModel::SetState(const SliceSettingsState& state)
{
    m_state = state;
}

const SliceSettingsState& SliceSettingsModel::State() const
{
    return m_state;
}

SliceSettingsValidationResult SliceSettingsModel::Validate() const
{
    SliceSettingsValidationResult result;
    if (m_state.profileid.trimmed().isEmpty())
    {
        result.errors.push_back(QStringLiteral("Profile 不能为空。"));
    }
    if (m_state.modelpath.trimmed().isEmpty())
    {
        result.errors.push_back(QStringLiteral("模型文件不能为空。"));
    }
    if (m_state.outputdirectory.trimmed().isEmpty())
    {
        result.errors.push_back(QStringLiteral("输出目录不能为空。"));
    }
    if (!std::isfinite(m_state.layerthicknessmm) || m_state.layerthicknessmm <= 0.0)
    {
        result.errors.push_back(QStringLiteral("层高必须是大于 0 的有限数值。"));
    }
    if (!m_state.support.enabled && m_state.support.internalvoidenabled)
    {
        result.errors.push_back(QStringLiteral("内部镂空支撑不能在支撑总开关关闭时启用。"));
    }
    if (m_state.support.internalvoidminareapx < 0)
    {
        result.errors.push_back(QStringLiteral("内部镂空最小面积不能为负数。"));
    }
    if (m_state.surfacevarnish.thicknesspx < 0
        || (m_state.surfacevarnish.enabled && m_state.surfacevarnish.thicknesspx <= 0))
    {
        result.errors.push_back(QStringLiteral("启用表面光油时厚度必须大于 0 px。"));
    }
    if (!std::isfinite(m_state.outervarnish.thicknessmm)
        || m_state.outervarnish.thicknessmm < 0.0
        || (m_state.outervarnish.enabled && m_state.outervarnish.thicknessmm <= 0.0))
    {
        result.errors.push_back(QStringLiteral("启用外侧光油时厚度必须大于 0 mm。"));
    }
    if (!std::isfinite(m_state.outervarnish.pixelpitchum) || m_state.outervarnish.pixelpitchum <= 0.0)
    {
        result.errors.push_back(QStringLiteral("像素物理尺寸必须大于 0 um。"));
    }
    if (m_state.preview.enabled && m_state.preview.interval <= 0)
    {
        result.errors.push_back(QStringLiteral("启用预览时预览间隔必须大于 0。"));
    }
    if (m_state.enginerole == SliceEngineRole::OpenVdbUtilityCandidate)
    {
        result.warnings.push_back(
            QStringLiteral("OpenVDB 当前仅为 utility/candidate，productionReplacementAllowed=false。"));
    }
    return result;
}
