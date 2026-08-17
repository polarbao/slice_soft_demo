#include "HostMainWindow.h"

#include <QLabel>

bool HostMainWindow::ApplyPendingSceneContext(QString* error)
{
    const hostslicesettings settings = m_sliceSettingsPanel->Settings();
    return m_importWorkflow->SetPendingSceneContext(
        m_selectedProfileId,
        settings.buildvolume,
        error);
}

void HostMainWindow::RefreshSliceSettings()
{
    m_sliceSettingsPanel->SetModelPath(
        m_importWorkflow->ReferenceModelPath());
    m_sliceSettingsPanel->SetSingleMaterialRestriction(
        m_importWorkflow->RequiresSingleMaterialProcess(),
        m_importWorkflow->SingleMaterialRestrictionSummary());
    m_sliceSettingsPanel->SetSceneAuthority(
        m_importWorkflow->SceneHandle() != 0U,
        m_importWorkflow->SceneProfileId(),
        m_importWorkflow->SceneBuildVolume());
    const hostslicesettings settings = m_sliceSettingsPanel->Settings();
    m_sliceJobPanel->SetStage16Context(
        HostEffectiveProfileBuilder::GeometrySamplingStrategyId(
            settings.geometrysamplingstrategy));
    RefreshSliceJobReadiness();
}

void HostMainWindow::OnSliceSettingsChanged()
{
    const hostslicesettings settings = m_sliceSettingsPanel->Settings();
    m_sliceJobPanel->SetStage16Context(
        HostEffectiveProfileBuilder::GeometrySamplingStrategyId(
            settings.geometrysamplingstrategy));
    QString error;
    if (!ApplyPendingSceneContext(&error))
    {
        m_statusLabel->setText(
            QStringLiteral("切片设置未生效：%1").arg(error));
        RefreshSliceJobReadiness();
        RefreshTextureWhitePreflight();
        return;
    }
    const bool saved = SaveWorkspaceState();
    m_statusLabel->setText(
        m_sliceSettingsPanel->IsReady()
            ? saved
                ? QStringLiteral("有效 Profile 已更新并自动保存 · 可开始切片")
                : QStringLiteral("有效 Profile 已更新 · 自动保存失败")
            : saved
                ? QStringLiteral("切片参数草稿已自动保存 · 请检查有效 Profile")
                : QStringLiteral("切片参数草稿已更新 · 自动保存失败"));
    RefreshSliceJobReadiness();
    RefreshTextureWhitePreflight();
}
