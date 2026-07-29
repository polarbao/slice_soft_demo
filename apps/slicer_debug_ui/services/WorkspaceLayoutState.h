#pragma once

#include <QString>

class ContextInspector;
class QMainWindow;
class QSettings;
class QSplitter;

/**
 * @brief Persists and validates the versioned Qt workbench layout.
 */
class WorkspaceLayoutState final
{
public:
    /**
     * @brief Return the current layout-state schema version.
     * @return Stable integer schema version.
     */
    static int SchemaVersion();

    /**
     * @brief Return the QSettings organization name.
     * @return Stable organization name.
     */
    static QString OrganizationName();

    /**
     * @brief Return the QSettings application name.
     * @return Stable application name.
     */
    static QString ApplicationName();

    /**
     * @brief Return whether persistent settings are enabled for this process.
     * @return false for self-test and UI smoke processes.
     */
    static bool PersistenceEnabled();

    /**
     * @brief Save the current workbench layout.
     * @param settings Target settings store.
     * @param window Main workbench window.
     * @param splitter Main workspace splitter.
     * @param inspector Context inspector.
     */
    static void Save(
        QSettings& settings,
        QMainWindow* window,
        QSplitter* splitter,
        ContextInspector* inspector);

    /**
     * @brief Restore a valid layout or apply safe defaults.
     * @param settings Source settings store.
     * @param window Main workbench window.
     * @param splitter Main workspace splitter.
     * @param inspector Context inspector.
     * @return true when a valid saved layout was restored.
     */
    static bool Restore(
        QSettings& settings,
        QMainWindow* window,
        QSplitter* splitter,
        ContextInspector* inspector);

    /**
     * @brief Apply the safe 13D default layout.
     * @param window Main workbench window.
     * @param splitter Main workspace splitter.
     * @param inspector Context inspector.
     */
    static void Reset(
        QMainWindow* window,
        QSplitter* splitter,
        ContextInspector* inspector);

private:
    static bool IsOnAvailableScreen(
        const QMainWindow* window);
};
