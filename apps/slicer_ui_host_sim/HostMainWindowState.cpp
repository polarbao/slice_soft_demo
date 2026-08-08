#include "HostMainWindow.h"

#include "HostWorkspaceState.h"
#include "settings/ViewPresentationSettings.h"

#include <QSettings>
#include <QSplitter>
#include <QTabWidget>

HostMainWindow::~HostMainWindow()
{
    SaveWorkspaceState();
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

void HostMainWindow::SaveWorkspaceState()
{
    if (!HostWorkspaceState::PersistenceEnabled()
        || m_sliceSettingsPanel == nullptr)
    {
        return;
    }
    QSettings settings(
        HostWorkspaceState::OrganizationName(),
        HostWorkspaceState::ApplicationName());
    HostWorkspaceState::Save(
        settings,
        this,
        m_workspaceSplitter,
        m_workspaceTabs,
        m_inspectorTabs,
        m_sliceSettingsPanel->Settings());
}
