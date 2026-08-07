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

int UiSmokeTestRunner::SceneBatchImportThree(
    const UiSmokeTestOptions& options)
{
    MainWindow window(options.repo_root);
    SceneDocument& document = window.m_sceneDocument;
    SceneBatchImportController& controller =
        window.m_sceneBatchImportController;
    ModelTopViewWidget* topView = window.m_modelTopViewWidget;

    const QString configPath = QDir(options.repo_root).filePath(
        QStringLiteral(
            "samples/configs/golden/"
            "material_process_top2_fixture.json"));
    if (topView == nullptr
        || !window.config_editor_panel_->loadConfig(configPath))
    {
        return fail(QStringLiteral(
            "scene batch three-item MainWindow fixture unavailable"));
    }

    bool secondItemWasPresented{false};
    QObject::connect(
        &controller,
        &SceneBatchImportController::SigStateChanged,
        &window,
        [&controller,
         &document,
         topView,
         &secondItemWasPresented]()
         {
             if (controller.IsRunning()
                 && controller.Summary().imported == 2)
             {
                 secondItemWasPresented =
                     document.InstanceCount() == 2U
                     && topView->PresentationItemCount() == 2U;
             }
         });

    const QString modelPath = QDir(options.repo_root).filePath(
        QStringLiteral(
            "samples/models/openvdb/surface_shell_cube.obj"));
    SceneBatchImportRequest request;
    request.batchid = QStringLiteral("smoke-three");
    request.configpath = configPath;
    request.files = QStringList{
        modelPath,
        modelPath,
        modelPath,
    };
    request.autolayout = true;
    if (!controller.Start(request).IsValid()
        || !WaitForCondition(
            [&controller]()
            {
                return !controller.IsRunning();
            }))
    {
        return fail(QStringLiteral(
            "scene batch three-item import did not complete"));
    }

    const SceneBatchImportSummary& summary =
        controller.Summary();
    if (summary.selected != 3
        || summary.imported != 3
        || summary.failed != 0
        || summary.cancelled != 0
        || !summary.autolayoutapplied
        || document.InstanceCount() != 3U
        || !secondItemWasPresented
        || topView->PresentationItemCount() != 3U
        || document.Items().at(0U).layoutcolumn != 0
        || document.Items().at(1U).layoutcolumn != 1
        || document.Items().at(2U).layoutcolumn != 2)
    {
        return fail(QStringLiteral(
            "scene batch three-item summary/layout mismatch"));
    }

    int configTabIndex{-1};
    for (int index = 0;
         index < window.m_mainWorkspaceTabs->count();
         ++index)
    {
        if (window.m_mainWorkspaceTabs->tabText(index)
            == QStringLiteral("配置"))
        {
            configTabIndex = index;
            break;
        }
    }
    if (configTabIndex < 0)
    {
        return fail(QStringLiteral(
            "scene batch configuration workspace missing"));
    }

    window.m_mainWorkspaceTabs->setCurrentIndex(configTabIndex);
    QApplication::processEvents(QEventLoop::AllEvents, 50);
    if (document.InstanceCount() != 3U
        || !topView->HasRenderableGeometry())
    {
        return fail(QStringLiteral(
            "scene disappeared when configuration workspace opened"));
    }
    const int previewInterval =
        window.config_document_
            .value({QStringLiteral("preview"),
                    QStringLiteral("interval")})
            .toInt(1);
    window.config_document_.setValue(
        {QStringLiteral("preview"), QStringLiteral("interval")},
        previewInterval + 1);
    window.m_mainWorkspaceTabs->setCurrentWidget(
        window.m_modelTopViewWorkspace);
    QApplication::processEvents(QEventLoop::AllEvents, 50);
    if (document.InstanceCount() != 3U
        || !document.Geometry().has_value()
        || !topView->HasRenderableGeometry())
    {
        return fail(QStringLiteral(
            "scene disappeared after configuration workspace round-trip"));
    }

    return pass(QStringLiteral(
        "scene-batch-import-three ordered/one-layout/"
        "incremental-presentation/config-round-trip"));
}

int UiSmokeTestRunner::SceneBatchImportRealMeigui(
    const UiSmokeTestOptions& options)
{
    SceneDocument document;
    SceneModelRepository repository;
    ModelTopViewLoader loader(&document, &repository);
    SceneBatchImportController controller(&document);
    controller.SetLoadHandlers(
        [&loader](const ModelTopViewLoadRequest& request)
        {
            loader.RequestLoad(request);
            return loader.Generation();
        },
        [&loader]()
        {
            loader.Cancel();
        });
    QObject::connect(
        &loader,
        &ModelTopViewLoader::SigLoadingFinished,
        &controller,
        [&controller, &loader]()
        {
            controller.OnLoadFinished(loader.Generation());
        });

    const QDir root(options.repo_root);
    const QString modelRoot =
        root.filePath(QStringLiteral("model/obj/meigui_fudiao"));
    SceneBatchImportRequest request;
    request.batchid = QStringLiteral("smoke-real-meigui");
    request.configpath = root.filePath(
        QStringLiteral(
            "samples/configs/golden/"
            "material_process_top2_fixture.json"));
    request.files = QStringList{
        QDir(modelRoot).filePath(QStringLiteral("02.obj")),
        QDir(modelRoot).filePath(QStringLiteral("03.obj")),
        QDir(modelRoot).filePath(QStringLiteral("04.obj")),
        QDir(modelRoot).filePath(
            QStringLiteral("MF_Mei_gui_wumingzhi_fx04.obj")),
    };
    request.autolayout = true;
    for (const QString& modelPath : request.files)
    {
        if (!QFileInfo::exists(modelPath))
        {
            return fail(
                QStringLiteral(
                    "scene batch real meigui asset missing: ")
                + modelPath);
        }
    }
    if (!controller.Start(request).IsValid()
        || !WaitForCondition(
            [&controller]()
            {
                return !controller.IsRunning();
            },
            120000))
    {
        return fail(QStringLiteral(
            "scene batch real meigui import did not complete"));
    }

    const SceneBatchImportSummary& summary =
        controller.Summary();
    if (summary.selected != 4
        || summary.imported != 4
        || summary.failed != 0
        || summary.cancelled != 0
        || !summary.autolayoutapplied
        || document.InstanceCount() != 4U)
    {
        return fail(
            QStringLiteral(
                "scene batch real meigui summary mismatch: "
                "selected=%1 imported=%2 failed=%3 cancelled=%4 "
                "instances=%5 layout=%6")
                .arg(summary.selected)
                .arg(summary.imported)
                .arg(summary.failed)
                .arg(summary.cancelled)
                .arg(document.InstanceCount())
                .arg(summary.autolayoutapplied));
    }

    const auto TipFacesPositiveY =
        [](const slicer_core::SceneModel& model)
    {
        constexpr double kEndBandFraction{0.12};
        constexpr double kMinimumDifferenceMm{0.05};
        const double width =
            model.bbox_mm.max.x - model.bbox_mm.min.x;
        const double depth =
            model.bbox_mm.max.y - model.bbox_mm.min.y;
        if (depth <= width)
        {
            return false;
        }
        const double lowBoundary =
            model.bbox_mm.min.y
            + depth * kEndBandFraction;
        const double highBoundary =
            model.bbox_mm.max.y
            - depth * kEndBandFraction;
        double lowMinimumX{
            std::numeric_limits<double>::max()};
        double lowMaximumX{
            std::numeric_limits<double>::lowest()};
        double highMinimumX{
            std::numeric_limits<double>::max()};
        double highMaximumX{
            std::numeric_limits<double>::lowest()};
        for (const slicer_core::Triangle& triangle : model.triangles)
        {
            for (const slicer_core::Vec3* point :
                 {&triangle.a, &triangle.b, &triangle.c})
            {
                if (point->y <= lowBoundary)
                {
                    lowMinimumX = std::min(
                        lowMinimumX,
                        point->x);
                    lowMaximumX = std::max(
                        lowMaximumX,
                        point->x);
                }
                if (point->y >= highBoundary)
                {
                    highMinimumX = std::min(
                        highMinimumX,
                        point->x);
                    highMaximumX = std::max(
                        highMaximumX,
                        point->x);
                }
            }
        }
        const double lowSpan = lowMaximumX - lowMinimumX;
        const double highSpan = highMaximumX - highMinimumX;
        return std::isfinite(lowSpan)
            && std::isfinite(highSpan)
            && highSpan + kMinimumDifferenceMm < lowSpan;
    };
    for (const SceneDocumentItem& item : document.Items())
    {
        const auto source = repository.Find(item.sourcecachekey);
        if (!source.has_value()
            || source->model == nullptr
            || !TipFacesPositiveY(*source->model))
        {
            return fail(
                QStringLiteral(
                    "scene batch real meigui tip is not facing +Y: ")
                + item.modelpath);
        }
    }
    return pass(QStringLiteral(
        "scene-batch-import-real-meigui 02/03/04/MF +Y-tip"));
}

int UiSmokeTestRunner::SceneBatchImportPartialFailure(
    const UiSmokeTestOptions& options)
{
    SceneDocument document;
    SceneModelRepository repository;
    ModelTopViewLoader loader(&document, &repository);
    SceneBatchImportController controller(&document);
    controller.SetLoadHandlers(
        [&loader](const ModelTopViewLoadRequest& request)
        {
            loader.RequestLoad(request);
            return loader.Generation();
        },
        [&loader]()
        {
            loader.Cancel();
        });
    QObject::connect(
        &loader,
        &ModelTopViewLoader::SigLoadingFinished,
        &controller,
        [&controller, &loader]()
        {
            controller.OnLoadFinished(loader.Generation());
        });

    const QString modelPath = QDir(options.repo_root).filePath(
        QStringLiteral(
            "samples/models/openvdb/surface_shell_cube.obj"));
    SceneBatchImportRequest request;
    request.batchid = QStringLiteral("smoke-partial");
    request.configpath = QDir(options.repo_root).filePath(
        QStringLiteral(
            "samples/configs/golden/"
            "material_process_top2_fixture.json"));
    request.files = QStringList{
        modelPath,
        QDir(options.repo_root).filePath(
            QStringLiteral(
                "samples/models/missing-model.obj")),
        modelPath,
    };
    request.autolayout = true;
    if (!controller.Start(request).IsValid()
        || !WaitForCondition(
            [&controller]()
            {
                return !controller.IsRunning();
            }))
    {
        return fail(QStringLiteral(
            "scene batch partial-failure import did not complete"));
    }

    const SceneBatchImportSummary& summary =
        controller.Summary();
    if (summary.imported != 2
        || summary.failed != 1
        || summary.items.size() != 3U
        || summary.items.at(1U).errorcode
            != QStringLiteral(
                "SCENE_BATCH_IMPORT_ITEM_FAILED")
        || document.InstanceCount() != 2U
        || !summary.autolayoutapplied)
    {
        return fail(QStringLiteral(
            "scene batch partial-failure contract mismatch"));
    }
    return pass(QStringLiteral(
        "scene-batch-import-partial-failure continue/summary"));
}
