#include "DiagnosticAnalysisWorker.h"

#include "slicer_core/geometry/SceneModelTriangleMeshAdapter.h"
#include "slicer_core/geometry/TransformedModelAdapter.h"

#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>
#include <QPointer>
#include <QRunnable>
#include <QThreadPool>

#include <exception>
#include <functional>
#include <stdexcept>
#include <utility>

namespace
{

class FunctionRunnable final : public QRunnable
{
public:
    explicit FunctionRunnable(std::function<void()> task)
        : m_task(std::move(task))
    {
    }

    void run() override
    {
        m_task();
    }

private:
    std::function<void()> m_task;
};

DiagnosticAnalysisResult CancelledResult(
    const DiagnosticAnalysisRequest& request)
{
    DiagnosticAnalysisResult result;
    result.identity = request.identity;
    result.state = DiagnosticAnalysisState::Cancelled;
    result.message =
        QStringLiteral("诊断分析已取消。");
    result.error = result.message;
    return result;
}

QString FirstPartitionIssue(
    const slicer_core::
        TextureFillPartitionReleaseBenchmarkResult&
            benchmark)
{
    if (!benchmark.partition.issues.empty())
    {
        return QString::fromStdString(
            benchmark.partition.issues.front().message);
    }
    return QStringLiteral(
        "纹理/填充分区诊断未通过，未提供详细原因。");
}

}  // namespace

struct DiagnosticAnalysisWorker::CallbackState
{
    QMutex mutex;
    QPointer<DiagnosticAnalysisWorker> worker;
};

bool DiagnosticAnalysisIdentity::Matches(
    const DiagnosticAnalysisIdentity& other) const
{
    return sessionid == other.sessionid
        && sceneid == other.sceneid
        && modelid == other.modelid
        && instanceid == other.instanceid
        && confighash == other.confighash
        && scenerevision == other.scenerevision
        && transformrevision
            == other.transformrevision;
}

bool DiagnosticAnalysisIdentity::IsComplete() const
{
    return !sessionid.trimmed().isEmpty()
        && !sceneid.trimmed().isEmpty()
        && !modelid.trimmed().isEmpty()
        && !instanceid.trimmed().isEmpty()
        && !confighash.trimmed().isEmpty();
}

DiagnosticAnalysisWorker::DiagnosticAnalysisWorker(
    QObject* parent)
    : DiagnosticAnalysisWorker(
          &DiagnosticAnalysisWorker::ExecuteDefault,
          parent)
{
}

DiagnosticAnalysisWorker::DiagnosticAnalysisWorker(
    Executor executor,
    QObject* parent)
    : QObject(parent),
      m_executor(std::move(executor)),
      m_callbackState(
          std::make_shared<CallbackState>())
{
    if (!m_executor)
    {
        throw std::invalid_argument(
            "DiagnosticAnalysisWorker requires an executor");
    }
    m_callbackState->worker = this;
}

DiagnosticAnalysisWorker::~DiagnosticAnalysisWorker()
{
    if (m_activeCancellation)
    {
        m_activeCancellation->store(true);
    }
    QMutexLocker lock(&m_callbackState->mutex);
    m_callbackState->worker = nullptr;
}

bool DiagnosticAnalysisWorker::Start(
    const DiagnosticAnalysisRequest& request)
{
    if (!request.identity.IsComplete()
        || request.texturesurfacewidthmm <= 0.0
        || request.classificationresolutionmm <= 0.0
        || request.paddingvoxels < 0
        || request.modelfillmaterial.trimmed().isEmpty())
    {
        return false;
    }

    if (m_activeCancellation)
    {
        m_activeCancellation->store(true);
    }
    ++m_generation;
    m_activeCancellation =
        std::make_shared<std::atomic_bool>(false);
    const std::shared_ptr<std::atomic_bool>
        cancellation = m_activeCancellation;
    const std::shared_ptr<CallbackState>
        callbackState = m_callbackState;
    const Executor executor = m_executor;
    const quint64 generation = m_generation;
    m_running = true;
    m_state = DiagnosticAnalysisState::Running;
    emit SigStarted(request.identity);

    auto* runnable = new FunctionRunnable(
        [callbackState,
         cancellation,
         executor,
         generation,
         request]()
        {
            DiagnosticAnalysisResult result;
            result.identity = request.identity;
            try
            {
                result = executor(
                    request,
                    cancellation);
                if (!result.identity.Matches(
                        request.identity))
                {
                    result.identity =
                        request.identity;
                    result.state =
                        DiagnosticAnalysisState::Stale;
                    result.error =
                        QStringLiteral(
                            "诊断执行器返回了不匹配的身份。");
                    result.message = result.error;
                }
            }
            catch (const std::exception& error)
            {
                result.identity = request.identity;
                result.state =
                    DiagnosticAnalysisState::Failed;
                result.error =
                    QString::fromUtf8(error.what());
                result.message = result.error;
            }
            catch (...)
            {
                result.identity = request.identity;
                result.state =
                    DiagnosticAnalysisState::Failed;
                result.error =
                    QStringLiteral(
                        "诊断执行器发生未知异常。");
                result.message = result.error;
            }

            QMutexLocker lock(
                &callbackState->mutex);
            DiagnosticAnalysisWorker* worker =
                callbackState->worker.data();
            if (worker == nullptr)
            {
                return;
            }
            QMetaObject::invokeMethod(
                worker,
                [worker,
                 generation,
                 cancellation,
                 result = std::move(result)]() mutable
                {
                    worker->OnWorkerCompleted(
                        generation,
                        cancellation,
                        std::move(result));
                },
                Qt::QueuedConnection);
        });
    QThreadPool::globalInstance()->start(runnable);
    return true;
}

void DiagnosticAnalysisWorker::Cancel()
{
    if (!m_running)
    {
        return;
    }
    if (m_activeCancellation)
    {
        m_activeCancellation->store(true);
    }
}

bool DiagnosticAnalysisWorker::IsRunning() const
{
    return m_running;
}

DiagnosticAnalysisState
DiagnosticAnalysisWorker::State() const
{
    return m_state;
}

DiagnosticAnalysisResult
DiagnosticAnalysisWorker::ExecuteDefault(
    const DiagnosticAnalysisRequest& request,
    const std::shared_ptr<std::atomic_bool>& cancellation)
{
    if (request.sourcemodel == nullptr)
    {
        throw std::invalid_argument(
            "diagnostic source model is required");
    }
    if (cancellation->load())
    {
        return CancelledResult(request);
    }

    slicer_core::TransformedModelResult transformed =
        slicer_core::AdaptTransformedModel(
            *request.sourcemodel,
            request.instance);
    if (!transformed.IsValid())
    {
        DiagnosticAnalysisResult result;
        result.identity = request.identity;
        result.state = DiagnosticAnalysisState::Failed;
        result.error = QString::fromStdString(
            transformed.error->message);
        result.message = result.error;
        return result;
    }
    if (cancellation->load())
    {
        return CancelledResult(request);
    }

    slicer_core::SceneModel diagnosticModel;
    diagnosticModel.model_path =
        request.sourcemodel->model_path;
    diagnosticModel.format =
        request.sourcemodel->format;
    diagnosticModel.material_infos =
        request.sourcemodel->material_infos;
    diagnosticModel.materials =
        request.sourcemodel->materials;
    diagnosticModel.bbox_mm =
        transformed.geometry.bboxmm;
    diagnosticModel.triangles =
        std::move(transformed.geometry.triangles);
    diagnosticModel.triangle_textures =
        std::move(
            transformed.geometry.triangletextures);
    diagnosticModel.triangle_count =
        diagnosticModel.triangles.size();

    const slicer_core::AdaptedTriangleMesh adapted =
        slicer_core::AdaptSceneModelToTriangleMesh(
            diagnosticModel);
    if (cancellation->load())
    {
        return CancelledResult(request);
    }

    slicer_core::
        TextureFillPartitionReleaseBenchmarkRequest
            benchmarkRequest;
    benchmarkRequest.mesh = &adapted.mesh;
    benchmarkRequest.adaptedMesh = &adapted;
    benchmarkRequest.caseName =
        request.identity.instanceid.toStdString();
    benchmarkRequest.modelPath =
        request.modelpath.toStdString();
    benchmarkRequest.configPath =
        request.identity.sessionid.toStdString();
    benchmarkRequest.buildType = "qt_diagnostic";
    benchmarkRequest.voxelMm =
        request.classificationresolutionmm;
    benchmarkRequest.widthMm =
        request.texturesurfacewidthmm;
    benchmarkRequest.paddingVoxels =
        request.paddingvoxels;
    benchmarkRequest.sourceTriangles =
        adapted.topology.source_triangles;
    benchmarkRequest.acceptedTriangles =
        adapted.topology.accepted_triangles;
    benchmarkRequest.degenerateTriangles =
        adapted.topology.degenerate_triangles;
    benchmarkRequest.boundaryEdges =
        adapted.topology.boundary_edges;
    benchmarkRequest.nonManifoldEdges =
        adapted.topology.non_manifold_edges;
    benchmarkRequest.textureSample =
        request.textureoptions;

    auto benchmark = std::make_shared<
        slicer_core::
            TextureFillPartitionReleaseBenchmarkResult>(
        slicer_core::
            RunTextureFillPartitionReleaseBenchmark(
                benchmarkRequest));
    if (cancellation->load())
    {
        return CancelledResult(request);
    }

    DiagnosticAnalysisResult result;
    result.identity = request.identity;
    result.evidence = benchmark;
    result.modelvoxels =
        benchmark->partition.stats.modelVoxels;
    result.texturesurfacevoxels =
        benchmark->partition.stats
            .textureSurfaceVoxels;
    result.modelfillvoxels =
        benchmark->partition.stats.modelFillVoxels;
    result.totalcorems =
        benchmark->partition.performance.totalCoreMs;
    if (!benchmark->partition.partitionPass)
    {
        result.state = DiagnosticAnalysisState::Failed;
        result.error = FirstPartitionIssue(*benchmark);
        result.message =
            QStringLiteral("诊断失败：") + result.error;
        return result;
    }
    if (benchmark->partition.available)
    {
        result.maximumwidthmm =
            benchmark->partition.widthMetrics
                .allTextureThresholdMm;
        result.alltexturethresholdmm =
            benchmark->partition.widthMetrics
                .allTextureThresholdMm;
        result.alltexture =
            benchmark->partition.widthMetrics
                .allTexture;
    }
    result.state = DiagnosticAnalysisState::Succeeded;
    result.message =
        QStringLiteral("诊断完成（diagnostic，不代表生产准入）。");
    return result;
}

void DiagnosticAnalysisWorker::OnWorkerCompleted(
    const quint64 generation,
    const std::shared_ptr<std::atomic_bool>& cancellation,
    DiagnosticAnalysisResult result)
{
    if (generation != m_generation)
    {
        result.state = DiagnosticAnalysisState::Stale;
        emit SigStaleDiscarded(result.identity);
        return;
    }
    if (cancellation->load()
        && result.state
            != DiagnosticAnalysisState::Failed)
    {
        result.state =
            DiagnosticAnalysisState::Cancelled;
        if (result.error.isEmpty())
        {
            result.error =
                QStringLiteral("诊断分析已取消。");
        }
        result.message = result.error;
    }
    m_running = false;
    m_state = result.state;
    emit SigFinished(result);
}
