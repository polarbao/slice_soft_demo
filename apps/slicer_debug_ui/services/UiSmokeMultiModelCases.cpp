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

int UiSmokeTestRunner::MultiModelList(
    const UiSmokeTestOptions& options)
{
    QWidget workspace;
    SceneDocument document;
    SceneSelectionModel selection;
    ModelTopViewWidget canvas(&document, &selection, &workspace);
    ModelListPanel panel(&document, &selection, &workspace);

    slicer_core::ModelInstance first;
    first.instanceid = "multi-first";
    first.modelid = "multi-model-first";
    first.sourcetransformidentity = "first-source";
    first.sourcebboxmm = {{0.0, 0.0, 0.0}, {8.0, 4.0, 1.0}};
    first.effectivebboxmm = first.sourcebboxmm;
    document.SetLoading(1U, QStringLiteral("first.obj"));
    if (!document.SetSceneContext(
            1U,
            QStringLiteral("multi-scene"),
            1U,
            QStringLiteral("shared-cache"),
            QStringLiteral("first-source-hash"),
            QStringLiteral("first-resource-hash"),
            first))
    {
        return fail(QStringLiteral("multi-model-list first context failed"));
    }
    slicer_core::SceneViewGeometry firstGeometry;
    firstGeometry.sceneid = "multi-scene";
    firstGeometry.modelid = first.modelid;
    firstGeometry.instanceid = first.instanceid;
    firstGeometry.scenerevision = 1U;
    firstGeometry.worldboundsmm = {{0.0, 0.0}, {8.0, 4.0}};
    firstGeometry.surfacepreview.width = 8;
    firstGeometry.surfacepreview.height = 4;
    firstGeometry.surfacepreview.rgba.assign(
        8U * 4U * 4U,
        255U);
    for (std::size_t index = 0U;
         index < firstGeometry.surfacepreview.rgba.size();
         index += 4U)
    {
        firstGeometry.surfacepreview.rgba.at(index + 0U) = 24U;
        firstGeometry.surfacepreview.rgba.at(index + 1U) = 86U;
        firstGeometry.surfacepreview.rgba.at(index + 2U) = 214U;
    }
    firstGeometry.surfacepreview.texturedpixelcount = 8U * 4U;
    firstGeometry.surfacepreview.contenthash =
        "multi-model-list-blue-surface";
    firstGeometry.admissionstatus =
        slicer_core::SceneViewAdmissionStatus::Admitted;
    if (!document.SetGeometry(1U, firstGeometry))
    {
        return fail(QStringLiteral("multi-model-list first geometry failed"));
    }
    selection.SetSelectedInstance(QStringLiteral("multi-first"));
    workspace.resize(700, 420);
    canvas.setGeometry(0, 0, 700, 420);
    workspace.show();
    QApplication::processEvents(QEventLoop::AllEvents, 50);
    const QImage appearanceImage = canvas.grab().toImage();
    bool foundAppearanceColor{false};
    QPoint appearancePoint;
    for (int y = 0;
         y < appearanceImage.height() && !foundAppearanceColor;
         ++y)
    {
        for (int x = 0; x < appearanceImage.width(); ++x)
        {
            const QColor pixel = appearanceImage.pixelColor(x, y);
            if (pixel.red() == 24
                && pixel.green() == 86
                && pixel.blue() == 214)
            {
                foundAppearanceColor = true;
                appearancePoint = QPoint(x, y);
                break;
            }
        }
    }
    if (!foundAppearanceColor)
    {
        return fail(QStringLiteral(
            "multi-model-list top view ignores material appearance"));
    }
    selection.Clear();
    QMouseEvent selectSurface(
        QEvent::MouseButtonPress,
        QPointF(appearancePoint),
        Qt::LeftButton,
        Qt::LeftButton,
        Qt::NoModifier);
    QApplication::sendEvent(&canvas, &selectSurface);
    if (selection.SelectedInstance()
        != QStringLiteral("multi-first"))
    {
        return fail(QStringLiteral(
            "multi-model-list surface preview hit-test failed"));
    }

    const SceneDocumentOperationResult duplicated =
        document.DuplicateInstance(
            QStringLiteral("multi-first"),
            QStringLiteral("multi-copy"),
            document.SceneRevision());
    selection.SetSelectedInstance(document.CurrentInstanceId());
    if (!duplicated.IsValid()
        || document.InstanceCount() != 2U
        || document.Items().at(0U).sourcecachekey
            != document.Items().at(1U).sourcecachekey)
    {
        return fail(
            QStringLiteral("multi-model-list source sharing failed"));
    }

    QApplication::processEvents(QEventLoop::AllEvents, 50);
    auto* list = panel.findChild<QListWidget*>(
        QStringLiteral("modelInstanceList"));
    auto* visibility = panel.findChild<QToolButton*>(
        QStringLiteral("modelListVisibilityButton"));
    auto* selectAll = panel.findChild<QToolButton*>(
        QStringLiteral("modelListSelectAllButton"));
    auto* lock = panel.findChild<QToolButton*>(
        QStringLiteral("modelListLockButton"));
    auto* remove = panel.findChild<QToolButton*>(
        QStringLiteral("modelListDeleteButton"));
    auto* add = panel.findChild<QToolButton*>(
        QStringLiteral("modelListAddButton"));
    if (list == nullptr
        || visibility == nullptr
        || selectAll == nullptr
        || lock == nullptr
        || remove == nullptr
        || add == nullptr
        || list->count() != 2)
    {
        return fail(
            QStringLiteral("multi-model-list controls or rows missing"));
    }

    selectAll->click();
    if (list->selectionMode()
            != QAbstractItemView::ExtendedSelection
        || list->selectedItems().size() != 2)
    {
        return fail(QStringLiteral(
            "multi-model-list select-all did not select every row"));
    }

    list->setCurrentRow(1);
    visibility->click();
    lock->click();
    if (document.Items().at(1U).instance.visible
        || !document.Items().at(1U).instance.locked
        || remove->isEnabled())
    {
        return fail(
            QStringLiteral("multi-model-list visibility/lock mismatch"));
    }
    lock->click();
    remove->click();
    if (document.InstanceCount() != 1U
        || selection.SelectedInstance()
            != QStringLiteral("multi-first"))
    {
        return fail(
            QStringLiteral("multi-model-list delete/selection mismatch"));
    }

    bool addRequested{false};
    QObject::connect(
        &panel,
        &ModelListPanel::SigAddRequested,
        &workspace,
        [&addRequested]()
        {
            addRequested = true;
        });
    add->click();
    if (!addRequested)
    {
        return fail(QStringLiteral("multi-model-list add signal missing"));
    }

    SceneDocument importedDocument;
    SceneModelRepository repository;
    ModelTopViewLoader loader(&importedDocument, &repository);
    ModelTopViewLoadRequest firstRequest;
    firstRequest.configpath = QDir(options.repo_root).filePath(
        QStringLiteral(
            "samples/configs/golden/material_process_top2_fixture.json"));
    firstRequest.modelpath = QDir(options.repo_root).filePath(
        QStringLiteral(
            "samples/models/textured/fixtures/"
            "policy_textured_small.obj"));
    firstRequest.sceneid = QStringLiteral("import-scene");
    firstRequest.modelid = QStringLiteral("import-model-1");
    firstRequest.instanceid = QStringLiteral("import-instance-1");
    firstRequest.scenerevision = 1U;
    loader.RequestLoad(firstRequest);
    if (!WaitForCondition(
            [&importedDocument]()
            {
                return importedDocument.State()
                    != SceneDocumentState::Loading;
            })
        || importedDocument.State() != SceneDocumentState::Ready)
    {
        return fail(
            QStringLiteral("multi-model-list first async import failed"));
    }
    ModelTopViewLoadRequest secondRequest = firstRequest;
    secondRequest.modelid = QStringLiteral("import-model-2");
    secondRequest.instanceid = QStringLiteral("import-instance-2");
    secondRequest.scenerevision = 2U;
    secondRequest.appendtoscene = true;
    loader.RequestLoad(secondRequest);
    QSet<QRgb> importedSurfaceColors;
    if (!WaitForCondition(
            [&importedDocument]()
            {
                return importedDocument.State()
                    != SceneDocumentState::Loading;
            })
        || importedDocument.State() != SceneDocumentState::Ready
        || importedDocument.InstanceCount() != 2U
        || repository.Size() != 1U
        || importedDocument.Items().at(0U).layoutcolumn != 0
        || importedDocument.Items().at(1U).layoutcolumn != 1
        || !importedDocument.Items().at(0U)
                .geometry->surfacepreview.IsValid()
        || !importedDocument.Items().at(1U)
                .geometry->surfacepreview.IsValid()
        || importedDocument.Items().at(0U)
                .geometry->surfacepreview.texturedpixelcount
            == 0U
        || importedDocument.Items().at(1U)
                .geometry->surfacepreview.texturedpixelcount
            == 0U
        || importedDocument.Items().at(0U)
                .geometry->worldboundsmm.max.xmm
            >= importedDocument.Items().at(1U)
                .geometry->worldboundsmm.min.xmm)
    {
        return fail(
            QStringLiteral(
                "multi-model-list async append/source sharing/"
                "auto-layout failed"));
    }
    const auto& importedSurface =
        importedDocument.Items().at(0U).geometry->surfacepreview;
    for (std::size_t index = 0U;
         index + 3U < importedSurface.rgba.size();
         index += 4U)
    {
        if (importedSurface.rgba.at(index + 3U) == 0U)
        {
            continue;
        }
        importedSurfaceColors.insert(
            qRgb(
                importedSurface.rgba.at(index + 0U),
                importedSurface.rgba.at(index + 1U),
                importedSurface.rgba.at(index + 2U)));
        if (importedSurfaceColors.size() > 1)
        {
            break;
        }
    }
    if (importedSurfaceColors.size() <= 1)
    {
        return fail(
            QStringLiteral(
                "multi-model-list real UV texture sampling failed"));
    }

    MainWindow window(options.repo_root);
    auto* integratedPanel = window.findChild<ModelListPanel*>(
        QStringLiteral("modelListPanel"));
    auto* integratedCanvas = window.findChild<ModelTopViewWidget*>(
        QStringLiteral("modelTopViewWidget"));
    auto* transformPanel = window.findChild<ModelTransformPanel*>(
        QStringLiteral("modelTransformPanel"));
    auto* inspector = window.findChild<ContextInspector*>(
        QStringLiteral("contextInspector"));
    if (integratedPanel == nullptr
        || integratedCanvas == nullptr
        || transformPanel == nullptr
        || inspector == nullptr)
    {
        return fail(
            QStringLiteral("multi-model-list workspace integration missing"));
    }

    const QList<QSize> sizes{
        QSize(1280, 720),
        QSize(1440, 900),
        QSize(1920, 1080),
    };
    for (const QSize& size : sizes)
    {
        window.resize(size);
        window.show();
        QApplication::processEvents(QEventLoop::AllEvents, 50);
        const QRect canvasRect =
            GlobalRect(integratedCanvas).adjusted(1, 1, -1, -1);
        const QRect sideRect =
            GlobalRect(inspector).adjusted(1, 1, -1, -1);
        if (canvasRect.intersects(sideRect)
            || inspector->width() < inspector->minimumWidth()
            || integratedCanvas->width() < integratedCanvas->minimumWidth()
            || !inspector->PageTitles().contains(
                QStringLiteral("场景")))
        {
            return fail(
                QStringLiteral(
                    "multi-model-list overlap at %1x%2 "
                    "canvas=%3,%4,%5,%6 side=%7,%8,%9,%10")
                    .arg(size.width())
                    .arg(size.height())
                    .arg(canvasRect.x())
                    .arg(canvasRect.y())
                    .arg(canvasRect.width())
                    .arg(canvasRect.height())
                    .arg(sideRect.x())
                    .arg(sideRect.y())
                    .arg(sideRect.width())
                    .arg(sideRect.height()));
        }
    }

    return pass(QStringLiteral(
        "multi-model-list add/share/duplicate/visibility/lock/delete/"
        "selection/select-all/material-appearance/textured-import/auto-layout/"
        "three-window-sizes"));
}
