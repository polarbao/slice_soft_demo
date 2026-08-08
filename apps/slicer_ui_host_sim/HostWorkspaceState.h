#pragma once

#include "HostSliceSettings.h"

#include <QString>

class QMainWindow;
class QSettings;
class QSplitter;
class QTabWidget;

/** @brief Host preferences restored before a new runtime scene is created. */
struct hostworkspacepreferences
{
    hostslicesettings slicesettings;
};

/** @brief Persists versioned host preferences and safe workspace layout. */
class HostWorkspaceState final
{
public:
    /**
     * @brief Returns the current host workspace schema version.
     * @return Stable integer schema version.
     */
    static int SchemaVersion();

    /**
     * @brief Returns the production QSettings organization name.
     * @return Stable organization identifier.
     */
    static QString OrganizationName();

    /**
     * @brief Returns the production QSettings application name.
     * @return Stable application identifier.
     */
    static QString ApplicationName();

    /**
     * @brief Reports whether this process may access real user settings.
     * @return False for all self-test processes.
     */
    static bool PersistenceEnabled();

    /**
     * @brief Saves validated host preferences and workspace geometry.
     * @param settings Destination settings store.
     * @param window Reference host main window.
     * @param workspaceSplitter Main work-area splitter.
     * @param workspaceTabs Top-level workspace tabs.
     * @param inspectorTabs Host business inspector tabs.
     * @param sliceSettings Current host-owned slice settings.
     * @return This function does not return a value.
     */
    static void Save(
        QSettings& settings,
        QMainWindow* window,
        QSplitter* workspaceSplitter,
        QTabWidget* workspaceTabs,
        QTabWidget* inspectorTabs,
        const hostslicesettings& sliceSettings);

    /**
     * @brief Restores a valid workspace or applies safe defaults.
     * @param settings Source settings store.
     * @param window Reference host main window.
     * @param workspaceSplitter Main work-area splitter.
     * @param workspaceTabs Top-level workspace tabs.
     * @param inspectorTabs Host business inspector tabs.
     * @param preferences Receives validated host-owned preferences.
     * @return True only when the complete saved state was restored.
     */
    static bool Restore(
        QSettings& settings,
        QMainWindow* window,
        QSplitter* workspaceSplitter,
        QTabWidget* workspaceTabs,
        QTabWidget* inspectorTabs,
        hostworkspacepreferences* preferences);

    /**
     * @brief Applies the safe reference-host workspace layout.
     * @param window Reference host main window.
     * @param workspaceSplitter Main work-area splitter.
     * @param workspaceTabs Top-level workspace tabs.
     * @param inspectorTabs Host business inspector tabs.
     * @return This function does not return a value.
     */
    static void Reset(
        QMainWindow* window,
        QSplitter* workspaceSplitter,
        QTabWidget* workspaceTabs,
        QTabWidget* inspectorTabs);

private:
    static bool IsOnAvailableScreen(const QMainWindow* window);
};
