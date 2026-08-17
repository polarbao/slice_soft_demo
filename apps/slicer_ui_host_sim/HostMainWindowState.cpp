#include "HostMainWindow.h"

#include "HostTextureWhitePreflightService.h"
#include "HostWorkspaceState.h"
#include "MoveOptimizationPolicy.h"
#include "SceneInteractionController.h"
#include "render/CpuRasterBackend.h"
#include "render/SceneRenderPolicy.h"
#include "render/TopViewRenderPolicy.h"
#include "settings/ViewPresentationSettings.h"

#include <QSettings>
#include <QSplitter>
#include <QTabWidget>

HostMainWindow::~HostMainWindow()
{
    (void)SaveWorkspaceState();
}

void HostMainWindow::RestoreWorkspaceState()
{
    hostripsettings ripSettings = HostRipSettingsStore::Defaults();
    QString ripSettingsError;
    if (HostWorkspaceState::PersistenceEnabled())
    {
        QSettings settings(
            HostWorkspaceState::OrganizationName(),
            HostWorkspaceState::ApplicationName());
        (void)HostRipSettingsStore::Load(
            settings, &ripSettings, &ripSettingsError);
    }
    m_ripSettingsPanel->SetSettings(ripSettings);
    if (!ripSettingsError.isEmpty())
    {
        m_ripSettingsPanel->ShowJobState(
            QStringLiteral("配置无效"), ripSettingsError);
    }

    if (!HostWorkspaceState::PersistenceEnabled())
    {
        HostWorkspaceState::Reset(
            this, m_workspaceSplitter, m_workspaceTabs, m_inspectorTabs);
        return;
    }
    QSettings settings(
        HostWorkspaceState::OrganizationName(),
        HostWorkspaceState::ApplicationName());
    hostworkspacepreferences preferences;
    if (HostWorkspaceState::Restore(
            settings,
            this,
            m_workspaceSplitter,
            m_workspaceTabs,
            m_inspectorTabs,
            &preferences))
    {
        m_restoredProfileId = preferences.slicesettings.profileid;
        m_sliceSettingsPanel->SetPersistentSettings(
            preferences.slicesettings);
    }
}

bool HostMainWindow::SaveWorkspaceState()
{
    if (!HostWorkspaceState::PersistenceEnabled()
        || m_sliceSettingsPanel == nullptr)
    {
        return true;
    }
    QSettings settings(
        HostWorkspaceState::OrganizationName(),
        HostWorkspaceState::ApplicationName());
    const bool workspaceSaved = HostWorkspaceState::Save(
        settings,
        this,
        m_workspaceSplitter,
        m_workspaceTabs,
        m_inspectorTabs,
        m_sliceSettingsPanel->Settings());
    QString ignored;
    const bool ripSaved = m_ripSettingsPanel == nullptr
        || HostRipSettingsStore::Save(
            settings, m_ripSettingsPanel->Settings(), &ignored);
    return workspaceSaved && ripSaved;
}
