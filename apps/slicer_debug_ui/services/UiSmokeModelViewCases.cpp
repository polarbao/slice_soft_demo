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

int UiSmokeTestRunner::ModelTopView(
    const UiSmokeTestOptions& options)
{
    SceneDocument document;
    SceneSelectionModel selection;
    SceneModelRepository repository;
    ModelTopViewLoader loader(&document, &repository);
    ModelTopViewWidget widget(&document, &selection);

    if (document.State() != SceneDocumentState::Unloaded
        || widget.HasRenderableGeometry())
    {
        return fail(QStringLiteral("model top view initial state mismatch"));
    }

    ModelTopViewLoadRequest request;
    request.configpath = QDir(options.repo_root).filePath(
        QStringLiteral(
            "samples/configs/golden/material_process_top2_fixture.json"));
    request.modelpath = QDir(options.repo_root).filePath(
        QStringLiteral(
            "samples/models/openvdb/surface_shell_cube.obj"));
    request.sceneid = QStringLiteral("smoke-scene");
    request.modelid = QStringLiteral("smoke-model");
    request.instanceid = QStringLiteral("smoke-instance");
    request.scenerevision = 3U;
    request.transformrevision = 0U;
    loader.RequestLoad(request);
    if (document.State() != SceneDocumentState::Loading)
    {
        return fail(QStringLiteral("model top view did not enter loading"));
    }
    if (!WaitForCondition(
            [&document]()
            {
                return document.State() != SceneDocumentState::Loading;
            }))
    {
        return fail(QStringLiteral("model top view load timed out"));
    }
    if (document.State() != SceneDocumentState::Ready
        || !widget.HasRenderableGeometry()
        || !document.Geometry().has_value()
        || document.Geometry()->sceneid != "smoke-scene"
        || document.Geometry()->instanceid != "smoke-instance"
        || document.Geometry()->scenerevision != 3U)
    {
        return fail(QStringLiteral("model top view ready geometry mismatch"));
    }

    const QList<QSize> sizes{
        QSize(1280, 720),
        QSize(1440, 900),
        QSize(1920, 1080),
    };
    for (const QSize& size : sizes)
    {
        widget.resize(size);
        QImage image(size, QImage::Format_ARGB32);
        image.fill(Qt::transparent);
        widget.render(&image);
        bool foundGeometryPixel = false;
        for (int y = 0; y < image.height() && !foundGeometryPixel; y += 2)
        {
            for (int x = 0; x < image.width(); x += 2)
            {
                const QColor pixel = image.pixelColor(x, y);
                if (pixel.green() > 110
                    && pixel.blue() > 100
                    && pixel.red() < 120)
                {
                    foundGeometryPixel = true;
                    break;
                }
            }
        }
        if (!foundGeometryPixel)
        {
            return fail(
                QStringLiteral("model top view rendered blank at %1x%2")
                    .arg(size.width())
                    .arg(size.height()));
        }
    }

    selection.Clear();
    QMouseEvent selectEvent(
        QEvent::MouseButtonPress,
        QPointF(
            widget.width() * 0.5,
            (52.0 + widget.height() - 34.0) * 0.5),
        Qt::LeftButton,
        Qt::LeftButton,
        Qt::NoModifier);
    QApplication::sendEvent(&widget, &selectEvent);
    if (selection.SelectedInstance() != QStringLiteral("smoke-instance"))
    {
        return fail(QStringLiteral("model top view selection mismatch"));
    }

    ModelTopViewLoadRequest staleRequest = request;
    staleRequest.modelpath = QDir(options.repo_root).filePath(
        QStringLiteral("samples/models/missing-stale.obj"));
    staleRequest.sceneid = QStringLiteral("stale-scene");
    ModelTopViewLoadRequest latestRequest = request;
    latestRequest.sceneid = QStringLiteral("latest-scene");
    loader.RequestLoad(staleRequest);
    loader.RequestLoad(latestRequest);
    if (!WaitForCondition(
            [&document]()
            {
                return document.State() != SceneDocumentState::Loading;
            })
        || document.State() != SceneDocumentState::Ready
        || !document.Geometry().has_value()
        || document.Geometry()->sceneid != "latest-scene")
    {
        return fail(QStringLiteral("stale model top view result was published"));
    }

    slicer_core::SceneViewGeometry blockedGeometry =
        document.Geometry().value();
    const slicer_core::ModelInstance blockedInstance =
        document.Instance().value();
    const QString blockedCacheKey = document.SourceCacheKey();
    const QString blockedSourceHash = document.SourceHash();
    const QString blockedResourceHash = document.ResourceHash();
    blockedGeometry.admissionstatus =
        slicer_core::SceneViewAdmissionStatus::Blocked;
    const quint64 blockedGeneration = document.Generation() + 1U;
    document.SetLoading(
        blockedGeneration,
        QStringLiteral(
            "C:/很长的中文模型路径/用于验证界面不会遮挡/"
            "模型资产_版本_最终候选.obj"));
    document.SetSceneContext(
        blockedGeneration,
        QString::fromStdString(blockedGeometry.sceneid),
        blockedGeometry.scenerevision,
        blockedCacheKey,
        blockedSourceHash,
        blockedResourceHash,
        blockedInstance);
    document.SetGeometry(
        blockedGeneration,
        std::move(blockedGeometry));
    if (document.State() != SceneDocumentState::Blocked
        || !widget.HasRenderableGeometry())
    {
        return fail(QStringLiteral("blocked model was not viewable"));
    }

    const quint64 failedGeneration = document.Generation() + 1U;
    document.SetLoading(failedGeneration, QStringLiteral("missing.obj"));
    document.SetFailure(
        failedGeneration,
        QStringLiteral("fixture failure"));
    if (document.State() != SceneDocumentState::Failed
        || widget.HasRenderableGeometry())
    {
        return fail(QStringLiteral("model top view failure state mismatch"));
    }

    loader.RequestLoad(request);
    loader.Cancel();
    QApplication::processEvents(QEventLoop::AllEvents, 50);
    QThread::msleep(20);
    QApplication::processEvents(QEventLoop::AllEvents, 50);
    if (document.State() != SceneDocumentState::Cancelled
        || loader.IsRunning())
    {
        return fail(QStringLiteral("model top view cancellation mismatch"));
    }

    MainWindow window(options.repo_root);
    auto* importButton = window.findChild<QPushButton*>(
        QStringLiteral("importModelPreviewButton"));
    auto* cancelImportButton = window.findChild<QPushButton*>(
        QStringLiteral("cancelModelImportButton"));
    auto* sliceCurrentSceneButton =
        window.findChild<QPushButton*>(
            QStringLiteral("sliceCurrentSceneButton"));
    auto* workspace = window.findChild<QWidget*>(
        QStringLiteral("modelTopViewWorkspace"));
    auto* canvas = window.findChild<ModelTopViewWidget*>(
        QStringLiteral("modelTopViewWidget"));
    if (importButton == nullptr
        || cancelImportButton == nullptr
        || sliceCurrentSceneButton == nullptr
        || workspace == nullptr
        || canvas == nullptr
        || importButton->text()
            != QStringLiteral("导入模型（可多选）")
        || sliceCurrentSceneButton->text()
            != QStringLiteral("切片当前场景")
        || sliceCurrentSceneButton->isEnabled())
    {
        return fail(QStringLiteral(
            "model top view batch import/current-scene action integration missing"));
    }

    return pass(QStringLiteral(
        "model-top-view async/+Z/grid/identity/selection/blocked/"
        "cancel/batch-action"));
}

int UiSmokeTestRunner::ModelTopViewTransform(
    const UiSmokeTestOptions& options)
{
    SceneDocument document;
    SceneSelectionModel selection;
    SceneModelRepository repository;
    ModelTopViewLoader loader(&document, &repository);
    SceneTransformController controller(
        &document,
        &selection,
        &repository);
    controller.SetProjectionRequester(
        [&loader](const SceneProjectionRequest& request)
        {
            loader.RequestProjection(request);
        });

    QWidget workspace;
    workspace.setObjectName(
        QStringLiteral("modelTransformSmokeWorkspace"));
    ModelTopViewWidget canvas(&document, &selection, &workspace);
    ModelTransformPanel panel(
        &document,
        &selection,
        &controller,
        &workspace);

    ModelTopViewLoadRequest request;
    request.configpath = QDir(options.repo_root).filePath(
        QStringLiteral(
            "samples/configs/golden/material_process_top2_fixture.json"));
    request.modelpath = QDir(options.repo_root).filePath(
        QStringLiteral(
            "samples/models/openvdb/surface_shell_cube.obj"));
    request.sceneid = QStringLiteral("transform-smoke-scene");
    request.modelid = QStringLiteral("transform-smoke-model");
    request.instanceid = QStringLiteral("transform-smoke-instance");
    request.scenerevision = 1U;
    loader.RequestLoad(request);
    if (!WaitForCondition(
            [&document]()
            {
                return document.State() != SceneDocumentState::Loading;
            })
        || document.State() != SceneDocumentState::Ready)
    {
        return fail(QStringLiteral(
            "model transform fixture did not become ready"));
    }
    selection.SetSelectedInstance(request.instanceid);

    auto* translateX = panel.findChild<QDoubleSpinBox*>(
        QStringLiteral("modelTransformTranslateX"));
    auto* translateY = panel.findChild<QDoubleSpinBox*>(
        QStringLiteral("modelTransformTranslateY"));
    auto* rotateZ = panel.findChild<QDoubleSpinBox*>(
        QStringLiteral("modelTransformRotateZ"));
    auto* uniformScale = panel.findChild<QDoubleSpinBox*>(
        QStringLiteral("modelTransformUniformScale"));
    auto* applyButton = panel.findChild<QPushButton*>(
        QStringLiteral("modelTransformApplyButton"));
    auto* centerButton = panel.findChild<QPushButton*>(
        QStringLiteral("modelTransformCenterButton"));
    auto* resetButton = panel.findChild<QPushButton*>(
        QStringLiteral("modelTransformResetButton"));
    auto* saveButton = panel.findChild<QPushButton*>(
        QStringLiteral("modelTransformSaveButton"));
    auto* axisPolicy = panel.findChild<QLabel*>(
        QStringLiteral("modelTransformAxisPolicy"));
    if (translateX == nullptr
        || translateY == nullptr
        || rotateZ == nullptr
        || uniformScale == nullptr
        || applyButton == nullptr
        || centerButton == nullptr
        || resetButton == nullptr
        || saveButton == nullptr
        || axisPolicy == nullptr
        || !axisPolicy->text().contains(
            QStringLiteral("Z 高度由自动定向")))
    {
        return fail(QStringLiteral(
            "model transform controls are incomplete"));
    }

    translateX->setValue(12.34);
    translateY->setValue(-5.67);
    rotateZ->setValue(45.0);
    uniformScale->setValue(1.25);
    applyButton->click();
    translateX->setValue(13.34);
    applyButton->click();
    if (!WaitForCondition(
            [&document]()
            {
                return document.State() != SceneDocumentState::Loading;
            })
        || document.State() != SceneDocumentState::Ready
        || !document.Instance().has_value()
        || document.SceneRevision() != 3U
        || document.Instance()->transformrevision != 2U
        || std::abs(
               document.Instance()->transform.translatexmm - 13.34)
            > 1.0e-9
        || std::abs(
               document.Instance()->transform.translateymm + 5.67)
            > 1.0e-9
        || !document.IsDirty()
        || document.IsGeometryStale())
    {
        return fail(QStringLiteral(
            "precise transform did not publish latest geometry"));
    }

    centerButton->click();
    if (!WaitForCondition(
            [&document]()
            {
                return document.State() != SceneDocumentState::Loading;
            })
        || document.State() != SceneDocumentState::Ready
        || document.SceneRevision() != 4U)
    {
        return fail(QStringLiteral(
            "scene-origin center command failed"));
    }
    resetButton->click();
    if (!WaitForCondition(
            [&document]()
            {
                return document.State() != SceneDocumentState::Loading;
            })
        || document.State() != SceneDocumentState::Ready
        || document.SceneRevision() != 5U
        || !slicer_core::ModelTransformsEquivalent(
            document.Instance()->transform,
            slicer_core::ModelTransform{}))
    {
        return fail(QStringLiteral("transform reset failed"));
    }

    const QList<QSize> sizes{
        QSize(1280, 720),
        QSize(1440, 900),
        QSize(1920, 1080),
    };
    for (const QSize& size : sizes)
    {
        workspace.resize(size);
        panel.setGeometry(
            size.width() - 300,
            0,
            300,
            size.height());
        canvas.setGeometry(
            0,
            0,
            size.width() - 300,
            size.height());
        if (panel.geometry().right() >= size.width()
            || canvas.geometry().right() >= panel.geometry().left())
        {
            return fail(QStringLiteral(
                "model transform panel overlaps canvas at %1x%2")
                    .arg(size.width())
                    .arg(size.height()));
        }
    }

    ModelTopViewLoadRequest lockedRequest = request;
    lockedRequest.sceneid = QStringLiteral("locked-transform-scene");
    lockedRequest.locked = true;
    loader.RequestLoad(lockedRequest);
    if (!WaitForCondition(
            [&document]()
            {
                return document.State() != SceneDocumentState::Loading;
            })
        || document.State() != SceneDocumentState::Ready)
    {
        return fail(QStringLiteral(
            "locked transform fixture did not load"));
    }
    selection.SetSelectedInstance(lockedRequest.instanceid);
    if (applyButton->isEnabled()
        || centerButton->isEnabled()
        || resetButton->isEnabled()
        || saveButton->isEnabled())
    {
        return fail(QStringLiteral(
            "locked instance transform controls remain enabled"));
    }

    MainWindow window(options.repo_root);
    if (window.findChild<ModelTransformPanel*>(
            QStringLiteral("modelTransformPanel"))
            == nullptr
        || window.findChild<ContextInspector*>(
            QStringLiteral("contextInspector"))
            == nullptr)
    {
        return fail(QStringLiteral(
            "model transform context-inspector integration missing"));
    }

    return pass(QStringLiteral(
        "model-top-view-transform x/y/rotate/scale/center/reset/"
        "locked/dirty/latest-generation"));
}
