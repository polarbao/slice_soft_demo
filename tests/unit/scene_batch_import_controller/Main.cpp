#include "SceneBatchImportController.h"

#include <QCoreApplication>

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>

namespace
{

void Require(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

slicer_core::ModelInstance MakeInstance(
    const ModelTopViewLoadRequest& request)
{
    slicer_core::ModelInstance instance;
    instance.modelid = request.modelid.toStdString();
    instance.instanceid = request.instanceid.toStdString();
    instance.sourcetransformidentity =
        request.modelpath.toStdString();
    instance.transformrevision =
        request.transformrevision;
    instance.transform = request.transform;
    instance.sourcebboxmm = {
        {0.0, 0.0, 0.0},
        {10.0, 5.0, 1.0},
    };
    instance.effectivebboxmm = instance.sourcebboxmm;
    return instance;
}

slicer_core::SceneViewGeometry MakeGeometry(
    const ModelTopViewLoadRequest& request)
{
    slicer_core::SceneViewGeometry geometry;
    geometry.sceneid = request.sceneid.toStdString();
    geometry.instanceid = request.instanceid.toStdString();
    geometry.scenerevision = request.scenerevision;
    geometry.transformrevision =
        request.transformrevision;
    geometry.sourcebboxmm = {
        {0.0, 0.0, 0.0},
        {10.0, 5.0, 1.0},
    };
    geometry.effectivebboxmm = geometry.sourcebboxmm;
    geometry.worldboundsmm = {
        {0.0, 0.0},
        {10.0, 5.0},
    };
    geometry.triangles.push_back(
        {{0.0, 0.0}, {10.0, 0.0}, {0.0, 5.0}});
    geometry.admissionstatus =
        slicer_core::SceneViewAdmissionStatus::Admitted;
    slicer_core::RefreshSceneViewGeometryHash(geometry);
    return geometry;
}

class FakeLoader final
{
public:
    explicit FakeLoader(SceneDocument* document)
        : m_document(document)
    {
    }

    quint64 Request(const ModelTopViewLoadRequest& request)
    {
        ++m_generation;
        m_pending = request;
        if (request.appendtoscene)
        {
            m_document->SetAdding(
                m_generation,
                request.modelpath);
        }
        else
        {
            m_document->SetLoading(
                m_generation,
                request.modelpath);
        }
        return m_generation;
    }

    void Cancel()
    {
        ++m_cancelCalls;
        m_document->SetCancelled(m_generation);
    }

    void CompleteSuccess(
        SceneBatchImportController* controller)
    {
        Require(
            m_pending.has_value(),
            "a fake load request should be pending");
        const ModelTopViewLoadRequest request =
            m_pending.value();
        const slicer_core::ModelInstance instance =
            MakeInstance(request);
        const bool contextAccepted = request.appendtoscene
            ? m_document->AddSceneContext(
                  m_generation,
                  request.sceneid,
                  request.scenerevision,
                  QStringLiteral("cache-")
                      + request.instanceid,
                  QStringLiteral("source-")
                      + request.instanceid,
                  QStringLiteral("resource-")
                      + request.instanceid,
                  instance)
            : m_document->SetSceneContext(
                  m_generation,
                  request.sceneid,
                  request.scenerevision,
                  QStringLiteral("cache-")
                      + request.instanceid,
                  QStringLiteral("source-")
                      + request.instanceid,
                  QStringLiteral("resource-")
                      + request.instanceid,
                  instance);
        Require(
            contextAccepted,
            "fake scene context should be accepted");
        Require(
            m_document->SetGeometry(
                m_generation,
                MakeGeometry(request)),
            "fake geometry should be accepted");
        m_pending.reset();
        controller->OnLoadFinished(m_generation);
    }

    void CompleteFailure(
        SceneBatchImportController* controller,
        const QString& message)
    {
        Require(
            m_pending.has_value(),
            "a fake failure request should be pending");
        Require(
            m_document->SetFailure(m_generation, message),
            "fake load failure should be accepted");
        m_pending.reset();
        controller->OnLoadFinished(m_generation);
    }

    const std::optional<ModelTopViewLoadRequest>& Pending() const
    {
        return m_pending;
    }

    quint64 Generation() const
    {
        return m_generation;
    }

    int CancelCalls() const
    {
        return m_cancelCalls;
    }

private:
    SceneDocument* m_document{nullptr};
    std::optional<ModelTopViewLoadRequest> m_pending;
    quint64 m_generation{0U};
    int m_cancelCalls{0};
};

void Bind(
    SceneBatchImportController* controller,
    FakeLoader* loader)
{
    controller->SetLoadHandlers(
        [loader](const ModelTopViewLoadRequest& request)
        {
            return loader->Request(request);
        },
        [loader]()
        {
            loader->Cancel();
        });
}

SceneBatchImportRequest MakeRequest(
    const QStringList& files)
{
    SceneBatchImportRequest request;
    request.batchid = QStringLiteral("batch-001");
    request.configpath =
        QStringLiteral("fixture/slice_config.json");
    request.files = files;
    request.autolayout = true;
    return request;
}

void OrderedThreeItemBatchAppliesOneLayout()
{
    SceneDocument document;
    SceneBatchImportController controller(&document);
    FakeLoader loader(&document);
    Bind(&controller, &loader);

    const SceneBatchImportStartResult started =
        controller.Start(MakeRequest({
            QStringLiteral("first.obj"),
            QStringLiteral("second.obj"),
            QStringLiteral("third.obj"),
        }));
    Require(started.IsValid(), "three-item batch should start");
    Require(
        loader.Pending()->modelpath.endsWith(
            QStringLiteral("first.obj")),
        "first selected path should dispatch first");
    Require(
        !loader.Pending()->autolayoutoncompletion,
        "single-item loader auto-layout must be disabled");
    Require(
        loader.Pending()->admissionstatus
            == slicer_core::SceneViewAdmissionStatus::Admitted,
        "successful batch imports must enter the Legacy scene gate "
        "as admitted");

    loader.CompleteSuccess(&controller);
    Require(
        loader.Pending()->modelpath.endsWith(
            QStringLiteral("second.obj")),
        "second selected path should dispatch second");
    loader.CompleteSuccess(&controller);
    loader.CompleteSuccess(&controller);

    const SceneBatchImportSummary& summary =
        controller.Summary();
    Require(!controller.IsRunning(), "batch should finish");
    Require(
        summary.selected == 3
            && summary.imported == 3
            && summary.failed == 0
            && summary.cancelled == 0,
        "three-item summary should be exact");
    Require(
        summary.autolayoutapplied,
        "completed batch should apply the grid once");
    Require(
        summary.finalscenerevision == 4U,
        "three commits plus one layout should produce revision four");
    Require(
        document.Items().at(0U).layoutcolumn == 0
            && document.Items().at(1U).layoutcolumn == 1
            && document.Items().at(2U).layoutcolumn == 2,
        "layout should preserve stable row-major order");
}

void PartialFailureContinuesAndPreservesSuccess()
{
    SceneDocument document;
    SceneBatchImportController controller(&document);
    FakeLoader loader(&document);
    Bind(&controller, &loader);

    Require(
        controller.Start(MakeRequest({
            QStringLiteral("good-a.obj"),
            QStringLiteral("bad.obj"),
            QStringLiteral("good-b.obj"),
        })).IsValid(),
        "partial-failure batch should start");
    loader.CompleteSuccess(&controller);
    loader.CompleteFailure(
        &controller,
        QStringLiteral("fixture model is invalid"));
    Require(
        loader.Pending()->modelpath.endsWith(
            QStringLiteral("good-b.obj")),
        "item after a failure must still dispatch");
    loader.CompleteSuccess(&controller);

    const SceneBatchImportSummary& summary =
        controller.Summary();
    Require(
        summary.imported == 2 && summary.failed == 1,
        "partial failure should be explicit");
    Require(
        document.InstanceCount() == 2U,
        "successful models must remain in the scene");
    Require(
        summary.items.at(1U).errorcode
            == QStringLiteral(
                "SCENE_BATCH_IMPORT_ITEM_FAILED"),
        "failed item should expose a stable error code");
    Require(
        summary.autolayoutapplied,
        "successful items should still receive one final layout");
}

void CapacityFailureDoesNotDispatch()
{
    SceneDocument document;
    SceneBatchImportController controller(&document);
    FakeLoader loader(&document);
    Bind(&controller, &loader);

    QStringList admittedFiles;
    for (int index = 0; index < 21; ++index)
    {
        admittedFiles.push_back(
            QStringLiteral("model-%1.obj").arg(index));
    }
    Require(
        controller.Start(MakeRequest(admittedFiles)).IsValid(),
        "twenty-one item fixture should start");
    for (int index = 0; index < 21; ++index)
    {
        loader.CompleteSuccess(&controller);
    }
    Require(
        document.InstanceCount() == 21U,
        "capacity fixture should retain twenty-one instances");

    const SceneBatchImportStartResult result =
        controller.Start(MakeRequest({
            QStringLiteral("overflow-a.obj"),
            QStringLiteral("overflow-b.obj"),
        }));
    Require(
        !result.IsValid()
            && result.error->code
                == SceneBatchImportStartErrorCode::
                    CapacityExceeded,
        "twenty-one plus two models must fail before dispatch");
    Require(
        !loader.Pending().has_value()
            && document.InstanceCount() == 21U,
        "capacity failure must leave the scene unchanged");
}

void CancellationKeepsCommittedItemsAndRejectsLateCompletion()
{
    SceneDocument document;
    SceneBatchImportController controller(&document);
    FakeLoader loader(&document);
    Bind(&controller, &loader);

    Require(
        controller.Start(MakeRequest({
            QStringLiteral("first.obj"),
            QStringLiteral("second.obj"),
            QStringLiteral("third.obj"),
        })).IsValid(),
        "cancellation fixture should start");
    loader.CompleteSuccess(&controller);
    const quint64 cancelledGeneration = loader.Generation();
    controller.Cancel();

    const SceneBatchImportSummary& summary =
        controller.Summary();
    Require(
        summary.imported == 1 && summary.cancelled == 2,
        "current and queued items should be counted as cancelled");
    Require(
        document.InstanceCount() == 1U,
        "committed item must survive batch cancellation");
    Require(
        loader.CancelCalls() == 1,
        "loader cancellation should be requested once");

    controller.OnLoadFinished(cancelledGeneration);
    Require(
        controller.Summary().items.size() == 3U,
        "late completion must not alter the terminal summary");
}

void UnsupportedFileFailsBeforeDispatch()
{
    SceneDocument document;
    SceneBatchImportController controller(&document);
    FakeLoader loader(&document);
    Bind(&controller, &loader);

    const SceneBatchImportStartResult result =
        controller.Start(MakeRequest({
            QStringLiteral("model.obj"),
            QStringLiteral("notes.txt"),
        }));
    Require(
        !result.IsValid()
            && result.error->code
                == SceneBatchImportStartErrorCode::
                    UnsupportedFile,
        "unsupported extension must fail the whole selection");
    Require(
        !loader.Pending().has_value(),
        "unsupported selection must not dispatch a partial batch");
}

}  // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    OrderedThreeItemBatchAppliesOneLayout();
    PartialFailureContinuesAndPreservesSuccess();
    CapacityFailureDoesNotDispatch();
    CancellationKeepsCommittedItemsAndRejectsLateCompletion();
    UnsupportedFileFailsBeforeDispatch();
    std::cout
        << "scene_batch_import_controller_unit_tests: PASS\n";
    return 0;
}
