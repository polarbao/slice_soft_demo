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
    return HostWorkspaceState::Save(
        settings,
        this,
        m_workspaceSplitter,
        m_workspaceTabs,
        m_inspectorTabs,
        m_sliceSettingsPanel->Settings());
}
