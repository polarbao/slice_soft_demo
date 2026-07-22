#include "ModelPreflightController.h"

#include "slicer_core/preflight/ModelPreflightAdmissionPolicy.h"

#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>
#include <QPointer>
#include <QRunnable>
#include <QThreadPool>

#include <functional>
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

}  // namespace

struct ModelPreflightController::CallbackState
{
    QMutex mutex;
    QPointer<ModelPreflightController> controller;
};

ModelPreflightController::ModelPreflightController(QObject* parent)
    : QObject(parent),
      m_service(std::make_shared<slicer_core::ModelPreflightService>()),
      m_callbackState(std::make_shared<CallbackState>())
{
    m_callbackState->controller = this;
    connect(
        &m_capabilityProcess,
        qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
        this,
        &ModelPreflightController::OnCapabilityFinished);
    connect(
        &m_capabilityProcess,
        &QProcess::errorOccurred,
        this,
        &ModelPreflightController::OnCapabilityError);
    m_capabilityTimeout.setSingleShot(true);
    m_capabilityTimeout.setInterval(120000);
    connect(
        &m_capabilityTimeout,
        &QTimer::timeout,
        this,
        &ModelPreflightController::OnCapabilityTimeout);
}

ModelPreflightController::~ModelPreflightController()
{
    if (m_activeCancellation)
    {
        m_activeCancellation->store(true);
    }
    if (m_capabilityProcess.state() != QProcess::NotRunning)
    {
        m_capabilityProcess.kill();
    }
    m_capabilityTimeout.stop();
    QMutexLocker lock(&m_callbackState->mutex);
    m_callbackState->controller = nullptr;
}

void ModelPreflightController::RequestPreflight(
    const UiModelPreflightRequest& request)
{
    ++m_generation;
    if (m_activeCancellation)
    {
        m_activeCancellation->store(true);
    }

    m_lastRequest = request;
    m_pendingRequest = request;
    m_currentMode = request.mode;
    m_hasLastRequest = true;
    m_hasPendingRequest = true;
    SetLifecycleState(slicer_core::ModelPreflightStatus::Running);

    if (m_capabilityRunning)
    {
        m_capabilityRunning = false;
        m_capabilityTimeout.stop();
        m_capabilityProcess.kill();
    }
    TryStartPending();
}

void ModelPreflightController::Recheck()
{
    if (m_hasLastRequest)
    {
        RequestPreflight(m_lastRequest);
    }
}

void ModelPreflightController::Cancel()
{
    ++m_generation;
    m_hasPendingRequest = false;
    if (m_activeCancellation)
    {
        m_activeCancellation->store(true);
    }
    if (m_capabilityRunning)
    {
        m_capabilityRunning = false;
        m_capabilityTimeout.stop();
        m_capabilityProcess.kill();
    }
    SetLifecycleState(slicer_core::ModelPreflightStatus::Cancelled);
}

void ModelPreflightController::MarkStale()
{
    if (!m_hasLastRequest
        || m_currentExecution.result.status
            == slicer_core::ModelPreflightStatus::NotRun)
    {
        return;
    }

    ++m_generation;
    m_hasPendingRequest = false;
    if (m_activeCancellation)
    {
        m_activeCancellation->store(true);
    }
    if (m_capabilityRunning)
    {
        m_capabilityRunning = false;
        m_capabilityTimeout.stop();
        m_capabilityProcess.kill();
    }
    SetLifecycleState(slicer_core::ModelPreflightStatus::Stale);
}

void ModelPreflightController::SetMode(
    const slicer_core::ModelPreflightPipelineMode mode)
{
    m_currentMode = mode;
    emit SigStateChanged();
}

const slicer_core::ModelPreflightExecutionResult&
ModelPreflightController::CurrentExecution() const
{
    return m_currentExecution;
}

slicer_core::ModelPreflightPipelineMode
ModelPreflightController::CurrentMode() const
{
    return m_currentMode;
}

bool ModelPreflightController::IsRunning() const
{
    return m_currentExecution.result.status
        == slicer_core::ModelPreflightStatus::Running;
}

QString ModelPreflightController::LastCapabilityDiagnostic() const
{
    return m_lastCapabilityDiagnostic;
}

void ModelPreflightController::SetCapabilityOverrideForTests(
    const std::optional<bool> available)
{
    m_capabilityOverride = available;
}

void ModelPreflightController::OnCapabilityFinished(
    const int exitCode,
    const QProcess::ExitStatus exitStatus)
{
    const quint64 generation = m_capabilityGeneration;
    const QByteArray output = m_capabilityProcess.readAllStandardOutput();
    const QByteArray errorOutput = m_capabilityProcess.readAllStandardError();
    const bool wasCurrent = m_capabilityRunning
        && generation == m_generation;
    m_capabilityRunning = false;
    m_capabilityTimeout.stop();

    if (!wasCurrent)
    {
        TryStartPending();
        return;
    }

    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(output, &parseError);
    const QJsonObject root = document.object();
    const bool validSchema = parseError.error == QJsonParseError::NoError
        && root.value(QStringLiteral("schema")).toString()
            == QStringLiteral("slicesoft.openvdb_capability.12e_r4.1");
    const bool available = validSchema
        && root.value(QStringLiteral("runtimeAvailable")).toBool(false)
        && exitStatus == QProcess::NormalExit
        && exitCode == 0;
    m_lastCapabilityDiagnostic = QStringLiteral(
                                     "exit=%1 status=%2 schema=%3 runtime=%4 stderr=%5 stdout=%6")
                                     .arg(exitCode)
                                     .arg(exitStatus == QProcess::NormalExit
                                              ? QStringLiteral("normal")
                                              : QStringLiteral("crashed"))
                                     .arg(validSchema ? QStringLiteral("valid")
                                                      : QStringLiteral("invalid"))
                                     .arg(root.value(QStringLiteral("runtimeAvailable"))
                                              .toBool(false)
                                              ? QStringLiteral("true")
                                              : QStringLiteral("false"))
                                     .arg(QString::fromLocal8Bit(errorOutput).trimmed())
                                     .arg(QString::fromUtf8(output).trimmed());
    StartWorker(available);
}

void ModelPreflightController::OnCapabilityError(
    const QProcess::ProcessError error)
{
    Q_UNUSED(error);
    if (!m_capabilityRunning)
    {
        return;
    }
    const quint64 generation = m_capabilityGeneration;
    m_lastCapabilityDiagnostic = QStringLiteral("process-error=%1 message=%2")
                                     .arg(static_cast<int>(error))
                                     .arg(m_capabilityProcess.errorString());
    m_capabilityRunning = false;
    m_capabilityTimeout.stop();
    if (generation == m_generation)
    {
        StartWorker(false);
    }
    else
    {
        TryStartPending();
    }
}

void ModelPreflightController::OnCapabilityTimeout()
{
    if (!m_capabilityRunning)
    {
        return;
    }
    const quint64 generation = m_capabilityGeneration;
    m_lastCapabilityDiagnostic = QStringLiteral("timeout=120000ms");
    m_capabilityRunning = false;
    m_capabilityProcess.kill();
    if (generation == m_generation)
    {
        StartWorker(false);
    }
}

void ModelPreflightController::TryStartPending()
{
    if (!m_hasPendingRequest || m_workerRunning || m_capabilityRunning
        || m_capabilityProcess.state() != QProcess::NotRunning)
    {
        return;
    }

    m_activeRequest = m_pendingRequest;
    m_hasPendingRequest = false;
    if (m_capabilityOverride.has_value())
    {
        m_lastCapabilityDiagnostic = QStringLiteral("test-override=%1")
                                         .arg(m_capabilityOverride.value()
                                                  ? QStringLiteral("true")
                                                  : QStringLiteral("false"));
        StartWorker(m_capabilityOverride.value());
        return;
    }
    if (m_activeRequest.capabilityprogram.isEmpty()
        || !QFileInfo::exists(m_activeRequest.capabilityprogram))
    {
        m_lastCapabilityDiagnostic = QStringLiteral("candidate-executable-missing=%1")
                                         .arg(m_activeRequest.capabilityprogram);
        StartWorker(false);
        return;
    }

    m_capabilityGeneration = m_generation;
    m_capabilityRunning = true;
    m_capabilityProcess.setProgram(m_activeRequest.capabilityprogram);
    m_capabilityProcess.setArguments(
        QStringList{QStringLiteral("--openvdb-capability-json")});
    m_capabilityProcess.start();
    m_capabilityTimeout.start();
}

void ModelPreflightController::StartWorker(
    const bool globalBackendAvailable)
{
    const quint64 generation = m_generation;
    const UiModelPreflightRequest request = m_activeRequest;
    m_activeCancellation = std::make_shared<std::atomic_bool>(false);
    const std::shared_ptr<std::atomic_bool> cancellation =
        m_activeCancellation;
    const std::shared_ptr<slicer_core::ModelPreflightService> service =
        m_service;
    const std::shared_ptr<CallbackState> callbackState = m_callbackState;
    m_workerRunning = true;

    auto* runnable = new FunctionRunnable(
        [service,
         callbackState,
         cancellation,
         request,
         generation,
         globalBackendAvailable]()
        {
            slicer_core::ModelPreflightRequest coreRequest;
            coreRequest.configPath = request.configpath.toStdWString();
            coreRequest.options = request.options;
            coreRequest.generation = generation;
            coreRequest.cancellationRequested = [cancellation]()
            {
                return cancellation->load();
            };
            slicer_core::ModelPreflightExecutionResult execution =
                service->Run(coreRequest);

            QMutexLocker lock(&callbackState->mutex);
            ModelPreflightController* controller = callbackState->controller.data();
            if (controller == nullptr)
            {
                return;
            }
            QMetaObject::invokeMethod(
                controller,
                [controller,
                 generation,
                 globalBackendAvailable,
                 execution = std::move(execution)]() mutable
                {
                    controller->OnWorkerCompleted(
                        generation,
                        globalBackendAvailable,
                        std::move(execution));
                },
                Qt::QueuedConnection);
        });
    QThreadPool::globalInstance()->start(runnable);
}

void ModelPreflightController::OnWorkerCompleted(
    const quint64 generation,
    const bool globalBackendAvailable,
    slicer_core::ModelPreflightExecutionResult execution)
{
    m_workerRunning = false;
    if (generation == m_generation && !m_hasPendingRequest)
    {
        slicer_core::ModelPreflightAdmissionContext context;
        context.global_backend_available = globalBackendAvailable;
        execution.result = slicer_core::EvaluateModelPreflightAdmissions(
            execution.result,
            context);
        m_currentExecution = std::move(execution);
        PublishCurrent();
    }
    TryStartPending();
}

void ModelPreflightController::SetLifecycleState(
    const slicer_core::ModelPreflightStatus status)
{
    m_currentExecution = {};
    m_currentExecution.generation = m_generation;
    m_currentExecution.result.status = status;
    slicer_core::ModelPreflightAdmissionContext context;
    context.global_backend_available = false;
    m_currentExecution.result = slicer_core::EvaluateModelPreflightAdmissions(
        m_currentExecution.result,
        context);
    PublishCurrent();
}

void ModelPreflightController::PublishCurrent()
{
    emit SigStateChanged();
}
