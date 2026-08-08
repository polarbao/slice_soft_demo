#include "HostMainWindow.h"

#include <QLabel>
#include <QMessageBox>

void HostMainWindow::ConfigureProfiles()
{
    hostprofilecatalogresolution resolution;
    QString error;
    if (!HostProfileCapabilityResolver::Resolve(
            *m_profileCatalog,
            m_client.ModuleInfo(),
            &resolution,
            &error))
    {
        m_selectedProfileId.clear();
        m_profilePanel->ClearProfiles(error);
        m_statusLabel->setText(QStringLiteral("Profile 目录不可用"));
        QMessageBox::critical(
            this, QStringLiteral("Profile 目录错误"), error);
        return;
    }
    m_profilePanel->SetProfiles(resolution);
    if (!m_restoredProfileId.isEmpty())
    {
        m_profilePanel->SelectProfile(m_restoredProfileId);
        m_restoredProfileId.clear();
    }
    m_selectedProfileId = m_profilePanel->SelectedProfileId();
    m_sliceSettingsPanel->SetSelectedProfileId(
        m_selectedProfileId,
        ProfileSupportsSlice(m_selectedProfileId));
    QString contextError;
    if (!ApplyPendingSceneContext(&contextError))
    {
        m_statusLabel->setText(contextError);
    }
}

void HostMainWindow::OnProfileChanged(const QString& profileId)
{
    m_selectedProfileId = profileId;
    m_sliceSettingsPanel->SetSelectedProfileId(
        profileId,
        ProfileSupportsSlice(profileId));
    QString contextError;
    if (!ApplyPendingSceneContext(&contextError))
    {
        m_statusLabel->setText(contextError);
        RefreshSliceJobReadiness();
        return;
    }
    m_statusLabel->setText(
        QStringLiteral("Profile 已选择：%1 · 仅更新宿主会话草稿")
            .arg(profileId));
    RefreshSliceJobReadiness();
}

bool HostMainWindow::ProfileSupportsSlice(const QString& profileId) const
{
    for (const hostprofiledescriptor& profile : m_profileCatalog->Profiles())
    {
        if (profile.profileid == profileId)
        {
            return profile.requiredcapabilities.contains(
                QStringLiteral("slice.rgbwsv"));
        }
    }
    return false;
}
