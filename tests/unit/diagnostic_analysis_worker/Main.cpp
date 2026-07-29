#include "DiagnosticAnalysisWorker.h"
#include "slicer_core/config.h"
#include "slicer_core/model.h"

#include "slicer_core/config.h"
#include "slicer_core/model.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>

namespace
{

DiagnosticAnalysisRequest MakeRequest(
    const QString& instanceId,
    const QString& configHash)
{
    DiagnosticAnalysisRequest request;
    request.identity.sessionid =
        QStringLiteral("diagnostic-session");
    request.identity.sceneid =
        QStringLiteral("scene-a");
    request.identity.instanceid = instanceId;
    request.identity.modelid =
        QStringLiteral("model-a");
    request.identity.confighash = configHash;
    request.identity.scenerevision = 4U;
    request.identity.transformrevision = 7U;
    request.texturesurfacewidthmm = 0.20;
    request.classificationresolutionmm = 0.10;
    request.modelfillmaterial =
        QStringLiteral("white");
    return request;
}

bool WaitUntil(
    const std::function<bool()>& predicate,
    const int timeoutMs = 2000)
{
    QEventLoop loop;
    QTimer poll;
    poll.setInterval(5);
    QObject::connect(
        &poll,
        &QTimer::timeout,
        &loop,
        [&]()
        {
            if (predicate())
            {
                loop.quit();
            }
        });
    QTimer::singleShot(timeoutMs, &loop, &QEventLoop::quit);
    poll.start();
    loop.exec();
    return predicate();
}

DiagnosticAnalysisResult SuccessfulResult(
    const DiagnosticAnalysisRequest& request)
{
    DiagnosticAnalysisResult result;
    result.identity = request.identity;
    result.state = DiagnosticAnalysisState::Succeeded;
    result.maximumwidthmm = 1.70;
    result.alltexturethresholdmm = 1.70;
    result.texturesurfacevoxels = 80U;
    result.modelfillvoxels = 20U;
    result.totalcorems = 12.5;
    return result;
}

bool TestDefaultCoreExecution()
{
    slicer_core::SliceConfig config;
    config.input.model_path =
        std::filesystem::path(SLICESOFT_SOURCE_DIR)
        / "samples/models/openvdb/surface_shell_cube.obj";
    config.input.format = "obj";
    config.auto_orient.enabled = false;
    auto source =
        std::make_shared<slicer_core::SceneModel>(
            slicer_core::load_model_report(
                config,
                std::filesystem::path(
                    SLICESOFT_SOURCE_DIR)));

    DiagnosticAnalysisRequest request = MakeRequest(
        QStringLiteral("instance-core"),
        QStringLiteral("hash-core"));
    request.sourcemodel = source;
    request.modelpath =
        QString::fromStdWString(
            source->model_path.wstring());
    request.classificationresolutionmm = 0.50;
    request.texturesurfacewidthmm = 1.00;
    request.instance.instanceid = "instance-core";
    request.instance.modelid = "model-a";
    request.instance.sourcetransformidentity =
        "source-transform-core";
    request.instance.sourcebboxmm = source->bbox_mm;
    request.instance.effectivebboxmm =
        source->bbox_mm;

    DiagnosticAnalysisWorker worker;
    bool completed{false};
    DiagnosticAnalysisResult observed;
    QObject::connect(
        &worker,
        &DiagnosticAnalysisWorker::SigFinished,
        &worker,
        [&](const DiagnosticAnalysisResult& result)
        {
            observed = result;
            completed = true;
        });
    if (!worker.Start(request)
        || !WaitUntil(
            [&]() { return completed; },
            10000))
    {
        return false;
    }
    return observed.state
            == DiagnosticAnalysisState::Succeeded
        && observed.modelvoxels.value_or(0U) > 0U
        && observed.texturesurfacevoxels.has_value()
        && observed.modelfillvoxels.has_value()
        && observed.evidence != nullptr
        && !observed.evidence->productionAdmitted;
}

bool TestSuccess()
{
    DiagnosticAnalysisWorker worker(
        [](const DiagnosticAnalysisRequest& request,
           const std::shared_ptr<std::atomic_bool>&)
        {
            return SuccessfulResult(request);
        });
    int completed{0};
    DiagnosticAnalysisResult observed;
    QObject::connect(
        &worker,
        &DiagnosticAnalysisWorker::SigFinished,
        &worker,
        [&](const DiagnosticAnalysisResult& result)
        {
            ++completed;
            observed = result;
        });

    const bool started = worker.Start(
        MakeRequest(
            QStringLiteral("instance-a"),
            QStringLiteral("hash-a")));
    return started
        && WaitUntil([&]() { return completed == 1; })
        && observed.state
            == DiagnosticAnalysisState::Succeeded
        && observed.identity.instanceid
            == QStringLiteral("instance-a")
        && observed.maximumwidthmm.has_value()
        && !worker.IsRunning();
}

bool TestFailure()
{
    DiagnosticAnalysisWorker worker(
        [](const DiagnosticAnalysisRequest&,
           const std::shared_ptr<std::atomic_bool>&)
            -> DiagnosticAnalysisResult
        {
            throw std::runtime_error("expected failure");
        });
    DiagnosticAnalysisResult observed;
    bool completed{false};
    QObject::connect(
        &worker,
        &DiagnosticAnalysisWorker::SigFinished,
        &worker,
        [&](const DiagnosticAnalysisResult& result)
        {
            observed = result;
            completed = true;
        });
    worker.Start(
        MakeRequest(
            QStringLiteral("instance-a"),
            QStringLiteral("hash-failure")));
    return WaitUntil([&]() { return completed; })
        && observed.state
            == DiagnosticAnalysisState::Failed
        && observed.error.contains(
            QStringLiteral("expected failure"));
}

bool TestCancel()
{
    DiagnosticAnalysisWorker worker(
        [](const DiagnosticAnalysisRequest& request,
           const std::shared_ptr<std::atomic_bool>& cancellation)
        {
            while (!cancellation->load())
            {
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(2));
            }
            DiagnosticAnalysisResult result;
            result.identity = request.identity;
            result.state =
                DiagnosticAnalysisState::Cancelled;
            return result;
        });
    bool completed{false};
    DiagnosticAnalysisResult observed;
    QObject::connect(
        &worker,
        &DiagnosticAnalysisWorker::SigFinished,
        &worker,
        [&](const DiagnosticAnalysisResult& result)
        {
            observed = result;
            completed = true;
        });
    worker.Start(
        MakeRequest(
            QStringLiteral("instance-a"),
            QStringLiteral("hash-cancel")));
    worker.Cancel();
    return WaitUntil([&]() { return completed; })
        && observed.state
            == DiagnosticAnalysisState::Cancelled
        && !worker.IsRunning();
}

bool TestReentryDropsStale()
{
    DiagnosticAnalysisWorker worker(
        [](const DiagnosticAnalysisRequest& request,
           const std::shared_ptr<std::atomic_bool>& cancellation)
        {
            if (request.identity.instanceid
                == QStringLiteral("instance-a"))
            {
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(60));
            }
            if (cancellation->load())
            {
                DiagnosticAnalysisResult cancelled;
                cancelled.identity = request.identity;
                cancelled.state =
                    DiagnosticAnalysisState::Cancelled;
                return cancelled;
            }
            return SuccessfulResult(request);
        });
    int completed{0};
    int stale{0};
    DiagnosticAnalysisResult observed;
    QObject::connect(
        &worker,
        &DiagnosticAnalysisWorker::SigFinished,
        &worker,
        [&](const DiagnosticAnalysisResult& result)
        {
            ++completed;
            observed = result;
        });
    QObject::connect(
        &worker,
        &DiagnosticAnalysisWorker::SigStaleDiscarded,
        &worker,
        [&](const DiagnosticAnalysisIdentity&)
        {
            ++stale;
        });

    worker.Start(
        MakeRequest(
            QStringLiteral("instance-a"),
            QStringLiteral("hash-a")));
    worker.Start(
        MakeRequest(
            QStringLiteral("instance-b"),
            QStringLiteral("hash-b")));
    return WaitUntil(
               [&]()
               {
                   return completed == 1
                       && stale == 1;
               })
        && observed.identity.instanceid
            == QStringLiteral("instance-b")
        && observed.identity.confighash
            == QStringLiteral("hash-b");
}

bool TestDestroyWhileRunning()
{
    auto finished = std::make_shared<std::atomic_bool>(false);
    auto* worker = new DiagnosticAnalysisWorker(
        [finished](
            const DiagnosticAnalysisRequest& request,
            const std::shared_ptr<std::atomic_bool>&)
        {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(30));
            finished->store(true);
            return SuccessfulResult(request);
        });
    worker->Start(
        MakeRequest(
            QStringLiteral("instance-a"),
            QStringLiteral("hash-destroy")));
    delete worker;
    return WaitUntil(
        [&]() { return finished->load(); });
}

bool TestRealDiagnosticExecutor()
{
    slicer_core::SliceConfig config;
    config.input.model_path =
        std::filesystem::path(SLICESOFT_SOURCE_DIR)
        / "samples/models/openvdb/surface_shell_cube.obj";
    config.input.format = "obj";
    config.auto_orient.enabled = false;
    auto source = std::make_shared<
        slicer_core::SceneModel>(
        slicer_core::load_model_report(
            config,
            std::filesystem::path(
                SLICESOFT_SOURCE_DIR)));

    DiagnosticAnalysisRequest request =
        MakeRequest(
            QStringLiteral("instance-real"),
            QStringLiteral("hash-real"));
    request.sourcemodel = source;
    request.instance.instanceid =
        request.identity.instanceid.toStdString();
    request.instance.modelid =
        request.identity.modelid.toStdString();
    request.instance.sourcetransformidentity =
        "source-transform-real";
    request.instance.sourcebboxmm = source->bbox_mm;
    request.instance.effectivebboxmm = source->bbox_mm;
    request.modelpath =
        QString::fromStdWString(
            source->model_path.wstring());
    request.classificationresolutionmm = 0.50;
    request.texturesurfacewidthmm = 1.00;

    DiagnosticAnalysisWorker worker;
    std::optional<DiagnosticAnalysisResult> observed;
    QObject::connect(
        &worker,
        &DiagnosticAnalysisWorker::SigFinished,
        &worker,
        [&](const DiagnosticAnalysisResult& result)
        {
            observed = result;
        });
    const bool started = worker.Start(request);
    return started
        && WaitUntil(
            [&]()
            {
                return observed.has_value();
            },
            10000)
        && observed->state
            == DiagnosticAnalysisState::Succeeded
        && observed->evidence != nullptr
        && observed->evidence->evidenceCollected
        && !observed->evidence->productionAdmitted;
}

}  // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    const bool success =
        TestDefaultCoreExecution()
        && TestSuccess()
        && TestFailure()
        && TestCancel()
        && TestReentryDropsStale()
        && TestDestroyWhileRunning()
        && TestRealDiagnosticExecutor();
    if (!success)
    {
        std::cerr
            << "diagnostic_analysis_worker_unit_tests failed\n";
        return 1;
    }
    std::cout
        << "diagnostic_analysis_worker_unit_tests passed\n";
    return 0;
}
