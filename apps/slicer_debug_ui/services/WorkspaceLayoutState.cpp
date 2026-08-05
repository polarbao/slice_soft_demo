#include "WorkspaceLayoutState.h"

#include "../widgets/ContextInspector.h"
#include "../widgets/DiagnosticsDock.h"
#include "../widgets/ProjectToolsDock.h"

#include <QAction>
#include <QByteArray>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QMainWindow>
#include <QPoint>
#include <QRect>
#include <QScreen>
#include <QSettings>
#include <QSplitter>
#include <QVariantList>

namespace
{

const QString kSettingsGroup =
    QStringLiteral("ui/layout");
const QString kVersionKey =
    QStringLiteral("version");
const QString kGeometryKey =
    QStringLiteral("geometry");
const QString kWindowStateKey =
    QStringLiteral("windowState");
const QString kSplitterSizesKey =
    QStringLiteral("mainSplitterSizes");
const QString kInspectorPageKey =
    QStringLiteral("contextInspectorPage");
const QString kInspectorVisibleKey =
    QStringLiteral("contextInspectorVisible");

}  // namespace

int WorkspaceLayoutState::SchemaVersion()
{
    return 3;
}

QString WorkspaceLayoutState::OrganizationName()
{
    return QStringLiteral("SliceSoft");
}

QString WorkspaceLayoutState::ApplicationName()
{
    return QStringLiteral("SliceSoftDebugUi");
}

bool WorkspaceLayoutState::PersistenceEnabled()
{
    const QStringList arguments =
        QCoreApplication::arguments();
    return !arguments.contains(
               QStringLiteral("--ui-smoke-test"))
        && !arguments.contains(
               QStringLiteral("--self-test"));
}

void WorkspaceLayoutState::Save(
    QSettings& settings,
    QMainWindow* window,
    QSplitter* splitter,
    ContextInspector* inspector)
{
    if (window == nullptr
        || splitter == nullptr
        || inspector == nullptr)
    {
        return;
    }

    QVariantList splitterSizes;
    for (const int size : splitter->sizes())
    {
        splitterSizes.push_back(size);
    }

    settings.beginGroup(kSettingsGroup);
    settings.setValue(kVersionKey, SchemaVersion());
    settings.setValue(kGeometryKey, window->saveGeometry());
    settings.setValue(
        kWindowStateKey,
        window->saveState(SchemaVersion()));
    settings.setValue(
        kSplitterSizesKey,
        splitterSizes);
    settings.setValue(
        kInspectorPageKey,
        inspector->CurrentPageIndex());
    settings.setValue(
        kInspectorVisibleKey,
        !inspector->isHidden());
    settings.endGroup();
    settings.sync();
}

bool WorkspaceLayoutState::Restore(
    QSettings& settings,
    QMainWindow* window,
    QSplitter* splitter,
    ContextInspector* inspector)
{
    if (window == nullptr
        || splitter == nullptr
        || inspector == nullptr)
    {
        return false;
    }

    settings.beginGroup(kSettingsGroup);
    const int version =
        settings.value(kVersionKey, -1).toInt();
    const QByteArray geometry =
        settings.value(kGeometryKey).toByteArray();
    const QByteArray windowState =
        settings.value(kWindowStateKey).toByteArray();
    const QVariantList splitterValues =
        settings.value(kSplitterSizesKey).toList();
    const int inspectorPage =
        settings.value(kInspectorPageKey, -1).toInt();
    const bool inspectorVisible =
        settings.value(kInspectorVisibleKey, true).toBool();
    settings.endGroup();

    bool splitterValid =
        splitterValues.size() == splitter->count();
    QList<int> splitterSizes;
    int splitterTotal = 0;
    for (const QVariant& value : splitterValues)
    {
        const int size = value.toInt();
        splitterValid = splitterValid && size >= 0;
        splitterTotal += size;
        splitterSizes.push_back(size);
    }
    splitterValid = splitterValid && splitterTotal > 0;
    const bool restored =
        version == SchemaVersion()
        && !geometry.isEmpty()
        && !windowState.isEmpty()
        && splitterValid
        && inspectorPage >= 0
        && inspectorPage < inspector->PageCount()
        && window->restoreGeometry(geometry)
        && window->restoreState(
            windowState,
            SchemaVersion());
    if (!restored)
    {
        settings.remove(kSettingsGroup);
        Reset(window, splitter, inspector);
        return false;
    }

    splitter->setSizes(splitterSizes);
    inspector->SetCurrentPageIndex(inspectorPage);
    QAction* inspectorAction =
        window->findChild<QAction*>(
            QStringLiteral(
                "contextInspectorToggleAction"));
    if (inspectorAction != nullptr)
    {
        inspectorAction->setChecked(inspectorVisible);
    }
    inspector->setVisible(inspectorVisible);
    if (!IsOnAvailableScreen(window))
    {
        settings.remove(kSettingsGroup);
        Reset(window, splitter, inspector);
        return false;
    }
    return true;
}

void WorkspaceLayoutState::Reset(
    QMainWindow* window,
    QSplitter* splitter,
    ContextInspector* inspector)
{
    if (window == nullptr
        || splitter == nullptr
        || inspector == nullptr)
    {
        return;
    }

    window->resize(1440, 900);
    inspector->SetCurrentPageIndex(0);
    inspector->show();
    splitter->setSizes(QList<int>{1040, 320});

    ProjectToolsDock* projectDock =
        window->findChild<ProjectToolsDock*>(
            QStringLiteral("projectToolsDock"));
    if (projectDock != nullptr)
    {
        projectDock->SetExpanded(false);
    }
    DiagnosticsDock* diagnosticsDock =
        window->findChild<DiagnosticsDock*>(
            QStringLiteral("diagnosticsDock"));
    if (diagnosticsDock != nullptr)
    {
        diagnosticsDock->SetExpanded(false);
    }
    QAction* inspectorAction =
        window->findChild<QAction*>(
            QStringLiteral(
                "contextInspectorToggleAction"));
    if (inspectorAction != nullptr)
    {
        inspectorAction->setChecked(true);
    }

    QScreen* screen = QGuiApplication::primaryScreen();
    if (screen != nullptr)
    {
        const QRect available = screen->availableGeometry();
        window->move(
            available.center()
            - QPoint(
                window->width() / 2,
                window->height() / 2));
    }
}

bool WorkspaceLayoutState::IsOnAvailableScreen(
    const QMainWindow* window)
{
    if (window == nullptr)
    {
        return false;
    }
    const QRect frame = window->frameGeometry();
    for (QScreen* screen : QGuiApplication::screens())
    {
        if (screen != nullptr
            && screen->availableGeometry().intersects(frame))
        {
            return true;
        }
    }
    return false;
}
