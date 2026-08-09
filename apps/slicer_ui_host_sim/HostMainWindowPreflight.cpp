#include "HostMainWindow.h"

#include "HostTextureWhitePreflightService.h"

#include <QLabel>

void HostMainWindow::RefreshTextureWhitePreflight()
{
    m_textureWhiteWarning = false;
    if (!m_textureWhitePreflightService || !m_sliceSettingsPanel
        || !m_importWorkflow)
    {
        return;
    }

    const QStringList texturePaths = m_importWorkflow->TexturePaths();
    const hosteffectiveprofile effectiveProfile =
        m_sliceSettingsPanel->EffectiveProfile();
    if (m_importWorkflow->SceneHandle() == 0U
        || m_importWorkflow->InstanceCount() == 0
        || effectiveProfile.profilehash.isEmpty())
    {
        if (m_textureWhitePreflightService->IsRunning())
        {
            m_textureWhitePreflightService->Cancel();
        }
        m_sliceSettingsPanel->SetTextureWhitePreflightStatus(
            QStringLiteral("纹理白区预检：等待有效场景和 Profile。"),
            false);
        return;
    }
    if (texturePaths.isEmpty())
    {
        if (m_textureWhitePreflightService->IsRunning())
        {
            m_textureWhitePreflightService->Cancel();
        }
        m_sliceSettingsPanel->SetTextureWhitePreflightStatus(
            QStringLiteral("纹理白区预检：当前场景未发现源贴图。"),
            false);
        return;
    }

    hosttexturewhitepreflightrequest request;
    request.sceneid = QString::number(m_importWorkflow->SceneHandle());
    request.scenerevision = m_importWorkflow->SceneRevision();
    request.contenthash = effectiveProfile.profilehash;
    request.profileid = m_selectedProfileId;
    request.texturepaths = texturePaths;
    request.profilesupportswhitecarrier =
        m_sliceSettingsPanel->Settings().texture.whitepolicy
        == HostTextureWhitePolicy::WhiteUnderbase;
    m_textureWhitePreflightService->RequestScan(request);
    m_sliceSettingsPanel->SetTextureWhitePreflightStatus(
        QStringLiteral("纹理白区预检：正在扫描 %1 个源贴图…")
            .arg(texturePaths.size()),
        false);
}

void HostMainWindow::OnTextureWhitePreflightFinished(
    const hosttexturewhitepreflightresult& result)
{
    const hosteffectiveprofile effectiveProfile =
        m_sliceSettingsPanel->EffectiveProfile();
    if (result.sceneid != QString::number(m_importWorkflow->SceneHandle())
        || result.scenerevision != m_importWorkflow->SceneRevision()
        || result.contenthash != effectiveProfile.profilehash
        || result.profileid != m_selectedProfileId)
    {
        return;
    }

    int errorCount = 0;
    int cacheHitCount = 0;
    for (const hosttexturewhiteassetresult& asset : result.assets)
    {
        errorCount += asset.error.isEmpty() ? 0 : 1;
        cacheHitCount += asset.cachehit ? 1 : 0;
    }
    m_textureWhiteWarning = result.HasWarning() || errorCount > 0;
    if (result.HasWarning())
    {
        m_sliceSettingsPanel->SetTextureWhitePreflightStatus(
            QStringLiteral("纹理白区预检：%1").arg(result.warningmessage),
            true);
        return;
    }
    if (errorCount > 0)
    {
        m_sliceSettingsPanel->SetTextureWhitePreflightStatus(
            QStringLiteral(
                "纹理白区预检未完整：%1/%2 个贴图无法读取或解码；"
                "该提示不阻断切片，生产材料闭合校验仍为最终权威。")
                .arg(errorCount)
                .arg(result.assets.size()),
            true);
        return;
    }

    const QString carrierText = result.containsstrictwhite
        ? QStringLiteral("检测到纯白纹理，当前 Profile 已启用按需补白。")
        : QStringLiteral("未检测到严格纯白 RGB(255,255,255)。");
    m_sliceSettingsPanel->SetTextureWhitePreflightStatus(
        QStringLiteral("纹理白区预检：%1 已扫描 %2 个贴图，缓存命中 %3。")
            .arg(carrierText)
            .arg(result.assets.size())
            .arg(cacheHitCount),
        false);
}

void HostMainWindow::OnTextureWhitePreflightDiscarded(
    const quint64 generation)
{
    if (!m_textureWhitePreflightService->IsRunning())
    {
        m_sliceSettingsPanel->SetTextureWhitePreflightStatus(
            QStringLiteral("纹理白区预检：旧结果 %1 已因场景或 Profile 变化丢弃。")
                .arg(generation),
            false);
    }
}
