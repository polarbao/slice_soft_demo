#include "UiSmokeTestInternal.h"

using ui_smoke_test_support::BuildExperimentalReportFixture;
using ui_smoke_test_support::BuildMaterialClosureReportFixture;
using ui_smoke_test_support::BuildOpenVdbUtilityReportFixture;
using ui_smoke_test_support::ClosedBoxObjFixture;
using ui_smoke_test_support::ContainsAll;
using ui_smoke_test_support::GlobalRect;
using ui_smoke_test_support::OpenTriangleObjFixture;
using ui_smoke_test_support::ReadJsonObject;
using ui_smoke_test_support::WaitForCondition;
using ui_smoke_test_support::WriteJsonFixture;
using ui_smoke_test_support::WritePreflightFixture;

int UiSmokeTestRunner::WorkbenchProjectDiagnostics(
    const UiSmokeTestOptions& options)
{
    MainWindow window(options.repo_root);
    ProjectToolsDock* projectDock =
        window.findChild<ProjectToolsDock*>(
            QStringLiteral("projectToolsDock"));
    DiagnosticsDock* diagnosticsDock =
        window.findChild<DiagnosticsDock*>(
            QStringLiteral("diagnosticsDock"));
    QWidget* projectPanel =
        window.findChild<QWidget*>(
            QStringLiteral("projectPanel"));
    QSplitter* mainSplitter =
        window.findChild<QSplitter*>(
            QStringLiteral("mainSplitter"));
    ContextInspector* inspector =
        window.findChild<ContextInspector*>(
            QStringLiteral("contextInspector"));
    QAction* projectAction =
        window.findChild<QAction*>(
            QStringLiteral("projectToolsToggleAction"));
    QAction* diagnosticsAction =
        window.findChild<QAction*>(
            QStringLiteral("diagnosticsToggleAction"));
    QWidget* legacyRightTools =
        window.findChild<QWidget*>(
            QStringLiteral("legacyRightToolsTabs"));
    if (projectDock == nullptr
        || diagnosticsDock == nullptr
        || projectPanel == nullptr
        || mainSplitter == nullptr
        || inspector == nullptr
        || projectAction == nullptr
        || diagnosticsAction == nullptr
        || legacyRightTools != nullptr)
    {
        return fail(QStringLiteral(
            "13D-03 project or diagnostics shell missing"));
    }

    const QStringList expectedInspectorPages{
        QStringLiteral("场景"),
        QStringLiteral("变换"),
        QStringLiteral("排版"),
        QStringLiteral("切片设置"),
        QStringLiteral("预检与诊断"),
    };
    const QStringList expectedDiagnosticPages{
        QStringLiteral("报告"),
        QStringLiteral("材料闭环"),
        QStringLiteral("曲线"),
        QStringLiteral("材料参数"),
        QStringLiteral("工艺对比"),
        QStringLiteral("切片耗时"),
        QStringLiteral("日志"),
    };
    if (mainSplitter->count() != 2
        || mainSplitter->widget(0)
            != window.m_mainWorkspaceTabs
        || mainSplitter->widget(1) != inspector
        || inspector->PageTitles()
            != expectedInspectorPages
        || diagnosticsDock->TabTitles()
            != expectedDiagnosticPages
        || !projectDock->isAncestorOf(projectPanel)
        || !diagnosticsDock->isAncestorOf(
            window.material_process_panel_)
        || !inspector->isAncestorOf(
            window.warnings_view_)
        || !diagnosticsDock->isAncestorOf(
            window.compare_view_)
        || !diagnosticsDock->isAncestorOf(
            window.m_sliceTimingPanel))
    {
        return fail(QStringLiteral(
            "13D-03 capability migration mismatch"));
    }
    if (!projectDock->isAncestorOf(window.build_button_)
        || !projectDock->isAncestorOf(
            window.m_importSliceButton)
        || !projectDock->isAncestorOf(
            window.m_importOpenVdbButton)
        || !projectDock->isAncestorOf(
            window.regression_button_)
        || !projectDock->isAncestorOf(
            window.run_rip_button_))
    {
        return fail(QStringLiteral(
            "13D-03 compatibility actions are unreachable"));
    }

    window.show();
    QApplication::processEvents(
        QEventLoop::AllEvents,
        50);
    if (projectDock->IsExpanded()
        || diagnosticsDock->IsExpanded()
        || inspector->isHidden()
        || projectAction->isChecked()
        || diagnosticsAction->isChecked()
        || window.dockWidgetArea(diagnosticsDock)
            != Qt::RightDockWidgetArea)
    {
        return fail(QStringLiteral(
            "13D-03 auxiliary regions are not hidden by default "
            "or task details are not docked on the right"));
    }

    const quint64 sceneRevision =
        window.m_sceneDocument.SceneRevision();
    projectDock->SetExpanded(true);
    QApplication::processEvents(
        QEventLoop::AllEvents,
        50);
    if (!projectDock->IsExpanded()
        || !projectAction->isChecked()
        || !projectPanel->isVisibleTo(projectDock))
    {
        return fail(QStringLiteral(
            "13D-03 project tools cannot be expanded"));
    }
    projectDock->SetExpanded(false);
    diagnosticsDock->SetExpanded(true);
    QApplication::processEvents(
        QEventLoop::AllEvents,
        50);
    if (projectDock->IsExpanded()
        || !diagnosticsDock->IsExpanded()
        || !diagnosticsAction->isChecked()
        || inspector->isHidden()
        || window.m_sceneDocument.SceneRevision()
            != sceneRevision)
    {
        return fail(QStringLiteral(
            "13D-03 task details and the resident context inspector "
            "cannot be displayed together safely"));
    }
    diagnosticsDock->SetExpanded(false);
    QApplication::processEvents(
        QEventLoop::AllEvents,
        50);
    if (inspector->isHidden()
        || diagnosticsDock->IsExpanded())
    {
        return fail(QStringLiteral(
            "13D-03 closing task details changed the resident "
            "context inspector"));
    }

    diagnosticsDock->SetExpanded(true);
    QApplication::processEvents(
        QEventLoop::AllEvents,
        50);
    window.m_contextInspectorToggleAction->setChecked(false);
    QApplication::processEvents(
        QEventLoop::AllEvents,
        50);
    if (!inspector->isHidden()
        || !diagnosticsDock->IsExpanded())
    {
        return fail(QStringLiteral(
            "13D-03 explicitly hiding the context inspector changed "
            "task details"));
    }
    window.m_contextInspectorToggleAction->setChecked(true);
    QApplication::processEvents(
        QEventLoop::AllEvents,
        50);
    if (inspector->isHidden()
        || !diagnosticsDock->IsExpanded()
        || !diagnosticsAction->isChecked())
    {
        return fail(QStringLiteral(
            "13D-03 restoring the resident context inspector changed "
            "task details"));
    }
    diagnosticsDock->SetExpanded(false);
    QApplication::processEvents(
        QEventLoop::AllEvents,
        50);

    return pass(QStringLiteral(
        "workbench-project-diagnostics project=collapsed/"
        "advanced-actions diagnostics=parallel-right-side-detail "
        "contexts=five"));
}

int UiSmokeTestRunner::WorkbenchLayoutRestore(
    const UiSmokeTestOptions& options)
{
    QTemporaryDir settingsDir;
    if (!settingsDir.isValid())
    {
        return fail(QStringLiteral(
            "13D-04 temporary settings directory unavailable"));
    }
    const QString settingsPath =
        QDir(settingsDir.path()).filePath(
            QStringLiteral("layout.ini"));
    QSettings settings(
        settingsPath,
        QSettings::IniFormat);

    MainWindow source(options.repo_root);
    source.resize(1280, 720);
    source.show();
    QApplication::processEvents(
        QEventLoop::AllEvents,
        50);
    source.m_projectToolsDock->SetExpanded(true);
    source.m_diagnosticsDock->SetExpanded(true);
    source.m_contextInspector->SetCurrentPageIndex(3);
    source.m_contextInspectorToggleAction->setChecked(
        false);
    source.m_mainSplitter->setSizes(
        QList<int>{760, 300});
    WorkspaceLayoutState::Save(
        settings,
        &source,
        source.m_mainSplitter,
        source.m_contextInspector);

    MainWindow restored(options.repo_root);
    const bool restoredSavedState =
        WorkspaceLayoutState::Restore(
            settings,
            &restored,
            restored.m_mainSplitter,
            restored.m_contextInspector);
    restored.show();
    QApplication::processEvents(
        QEventLoop::AllEvents,
        50);
    if (!restoredSavedState
        || !restored.m_projectToolsDock->IsExpanded()
        || !restored.m_diagnosticsDock->IsExpanded()
        || !restored.m_contextInspector->isHidden()
        || restored.m_contextInspectorToggleAction
               ->isChecked()
        || restored.m_contextInspector
               ->CurrentPageIndex()
            != 3)
    {
        return fail(
            QStringLiteral(
                "13D-04 valid layout state was not restored: "
                "restored=%1 project=%2 diagnostics=%3 "
                "inspectorHidden=%4 action=%5 page=%6")
                .arg(restoredSavedState)
                .arg(
                    restored.m_projectToolsDock
                        ->IsExpanded())
                .arg(
                    restored.m_diagnosticsDock
                        ->IsExpanded())
                .arg(
                    restored.m_contextInspector
                        ->isHidden())
                .arg(
                    restored
                        .m_contextInspectorToggleAction
                        ->isChecked())
                .arg(
                    restored.m_contextInspector
                        ->CurrentPageIndex()));
    }

    settings.beginGroup(
        QStringLiteral("ui/layout"));
    settings.setValue(
        QStringLiteral("version"),
        0);
    settings.setValue(
        QStringLiteral("geometry"),
        QByteArray("corrupt"));
    settings.endGroup();
    settings.sync();

    MainWindow fallback(options.repo_root);
    const bool restoredCorruptState =
        WorkspaceLayoutState::Restore(
            settings,
            &fallback,
            fallback.m_mainSplitter,
            fallback.m_contextInspector);
    fallback.show();
    QApplication::processEvents(
        QEventLoop::AllEvents,
        50);
    if (restoredCorruptState
        || fallback.m_projectToolsDock->IsExpanded()
        || fallback.m_diagnosticsDock->IsExpanded()
        || fallback.m_contextInspector->isHidden()
        || !fallback.m_contextInspectorToggleAction
                ->isChecked()
        || fallback.m_contextInspector
               ->CurrentPageIndex()
            != 0)
    {
        return fail(QStringLiteral(
            "13D-04 invalid layout did not fall back safely"));
    }

    return pass(QStringLiteral(
        "workbench-layout-restore schema=3/"
        "valid-state/corrupt-state-safe-default"));
}

int UiSmokeTestRunner::Workbench1280x720(
    const UiSmokeTestOptions& options)
{
    MainWindow window(options.repo_root);
    QWidget* actionBar =
        window.findChild<QWidget*>(
            QStringLiteral("sceneActionBar"));
    QWidget* workspace =
        window.findChild<QWidget*>(
            QStringLiteral("mainWorkspaceTabs"));
    ContextInspector* inspector =
        window.findChild<ContextInspector*>(
            QStringLiteral("contextInspector"));
    const QList<QPushButton*> jobButtons{
        window.findChild<QPushButton*>(
            QStringLiteral("jobImportModelsButton")),
        window.findChild<QPushButton*>(
            QStringLiteral("jobSaveSceneButton")),
        window.findChild<QPushButton*>(
            QStringLiteral("sliceCurrentSceneButton")),
        window.findChild<QPushButton*>(
            QStringLiteral(
                "cancelCurrentSceneSliceButton")),
    };
    if (actionBar == nullptr
        || workspace == nullptr
        || inspector == nullptr
        || jobButtons.contains(nullptr))
    {
        return fail(QStringLiteral(
            "13D-04 responsive workbench controls missing"));
    }
    for (QPushButton* button : jobButtons)
    {
        button->setEnabled(true);
    }
    const auto nextTabFocus =
        [](QWidget* current)
        {
            QWidget* candidate =
                current->nextInFocusChain();
            while (candidate != current)
            {
                if ((candidate->focusPolicy()
                        & Qt::TabFocus)
                    && candidate->isEnabled()
                    && !candidate->isHidden())
                {
                    return candidate;
                }
                candidate =
                    candidate->nextInFocusChain();
            }
            return current;
        };
    if (nextTabFocus(jobButtons.at(0))
            != jobButtons.at(1)
        || nextTabFocus(jobButtons.at(1))
            != jobButtons.at(2)
        || nextTabFocus(jobButtons.at(2))
            != jobButtons.at(3))
    {
        return fail(QStringLiteral(
            "13D-04 primary action keyboard order mismatch"));
    }

    const auto verifyLayout =
        [&window,
         actionBar,
         workspace,
         inspector,
         &jobButtons](const QString& scaleLabel)
        {
            window.resize(1280, 720);
            QApplication::processEvents(
                QEventLoop::AllEvents,
                50);
            if (window.width() > 1280
                || window.height() > 720
                || window.m_projectToolsDock->IsExpanded()
                || window.m_diagnosticsDock->IsExpanded())
            {
                return QStringLiteral(
                    "%1 default size or dock state mismatch")
                    .arg(scaleLabel);
            }

            const QRect actionRect =
                GlobalRect(actionBar);
            const QRect workspaceRect =
                GlobalRect(workspace);
            const QRect inspectorRect =
                GlobalRect(inspector);
            if (!actionBar->isVisibleTo(&window)
                || !workspace->isVisibleTo(&window)
                || !inspector->isVisibleTo(&window)
                || workspaceRect.intersects(inspectorRect)
                || actionRect.intersects(workspaceRect)
                || actionRect.intersects(inspectorRect)
                || workspaceRect.width() < 400
                || inspectorRect.width() < 240)
            {
                return QStringLiteral(
                    "%1 workbench regions overlap or collapse")
                    .arg(scaleLabel);
            }

            QList<QRect> buttonRects;
            for (QPushButton* button : jobButtons)
            {
                const QRect buttonRect =
                    GlobalRect(button);
                if (!button->isVisibleTo(&window)
                    || !actionRect.contains(buttonRect)
                    || (button->text().isEmpty()
                        && button->toolTip().isEmpty()))
                {
                    return QStringLiteral(
                        "%1 primary action is clipped")
                        .arg(scaleLabel);
                }
                for (const QRect& priorRect : buttonRects)
                {
                    if (priorRect.intersects(buttonRect))
                    {
                        return QStringLiteral(
                            "%1 primary actions overlap")
                            .arg(scaleLabel);
                    }
                }
                buttonRects.push_back(buttonRect);
            }
            return QString{};
        };

    window.show();
    QApplication::processEvents(
        QEventLoop::AllEvents,
        50);
    QString layoutError =
        verifyLayout(QStringLiteral("100%"));
    if (!layoutError.isEmpty())
    {
        return fail(layoutError);
    }

    QFont highDpiFont = window.font();
    highDpiFont.setPointSizeF(
        highDpiFont.pointSizeF() * 1.5);
    window.setFont(highDpiFont);
    layoutError =
        verifyLayout(QStringLiteral("150%"));
    if (!layoutError.isEmpty())
    {
        return fail(layoutError);
    }

    return pass(QStringLiteral(
        "workbench-1280x720 100%-150%/"
        "actions-visible/no-overlap/docks-collapsed"));
}
