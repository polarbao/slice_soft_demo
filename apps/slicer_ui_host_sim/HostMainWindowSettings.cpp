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
    m_sliceSettingsPanel->SetSceneAuthority(
        m_importWorkflow->SceneHandle() != 0U,
        m_importWorkflow->SceneProfileId(),
        m_importWorkflow->SceneBuildVolume());
}

void HostMainWindow::OnSliceSettingsChanged()
{
    QString error;
    if (!ApplyPendingSceneContext(&error))
    {
        m_statusLabel->setText(
            QStringLiteral("切片设置未生效：%1").arg(error));
        return;
    }
    m_statusLabel->setText(
        m_sliceSettingsPanel->IsReady()
            ? QStringLiteral("有效 Profile 已更新 · 未调用切片模块")
            : QStringLiteral("切片参数草稿已更新 · 等待导入模型"));
}
