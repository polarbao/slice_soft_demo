#include "HostMainWindow.h"
#include "HostProfileTransition.h"

#include <QLabel>
#include <QMessageBox>
#include <QSignalBlocker>

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
    RefreshTextureWhitePreflight();
}

void HostMainWindow::OnProfileChanged(const QString& profileId)
{
    const auto previousProfile=m_selectedProfileId;
    const auto previousSettings=m_sliceSettingsPanel->Settings();
    auto candidate=previousSettings;
    QString contextError;
    if(!AlignHostProfileProtocol(profileId,&candidate,&contextError))
    {
        const QSignalBlocker blocker(m_profilePanel);
        m_profilePanel->SelectProfile(previousProfile);
        m_statusLabel->setText(contextError);
        return;
    }
    const QSignalBlocker settingsBlocker(m_sliceSettingsPanel);
    m_sliceSettingsPanel->SetPersistentSettings(candidate);
    m_selectedProfileId = profileId;
    m_sliceSettingsPanel->SetSelectedProfileId(
        profileId,
        ProfileSupportsSlice(profileId));
    if (!ApplyPendingSceneContext(&contextError))
    {
        m_selectedProfileId=previousProfile;
        const QSignalBlocker profileBlocker(m_profilePanel);
        m_profilePanel->SelectProfile(previousProfile);
        m_sliceSettingsPanel->SetPersistentSettings(previousSettings);
        m_sliceSettingsPanel->SetSelectedProfileId(previousProfile,ProfileSupportsSlice(previousProfile));
        m_statusLabel->setText(contextError);
        RefreshSliceJobReadiness();
        RefreshTextureWhitePreflight();
        return;
    }
    m_statusLabel->setText(
        QStringLiteral("Profile 已生效：%1 · 保留模型与摆放位置")
            .arg(profileId));
    (void)SaveWorkspaceState();
    RefreshSliceJobReadiness();
    RefreshTextureWhitePreflight();
}

bool HostMainWindow::ProfileSupportsSlice(const QString& profileId) const
{
    for (const hostprofiledescriptor& profile : m_profileCatalog->Profiles())
    {
        if (profile.profileid == profileId)
        {
            return profile.requiredcapabilities.contains(
                       QStringLiteral("slice.rgbwsv"))
                || profile.requiredcapabilities.contains(
                       QStringLiteral("slice.rgbwsvt"));
        }
    }
    return false;
}
