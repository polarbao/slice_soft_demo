#pragma once

#include "slicer_core/preflight/ModelPreflightService.h"

#include <QObject>
#include <QProcess>
#include <QTimer>

#include <atomic>
#include <memory>
#include <optional>

struct UiModelPreflightRequest
{
    QString configpath;
    QString capabilityprogram;
    slicer_core::ModelPreflightPipelineMode mode{
        slicer_core::ModelPreflightPipelineMode::Legacy};
    slicer_core::ModelPreflightOptions options;
};

class ModelPreflightController final : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Create an asynchronous Qt adapter around the synchronous core service.
     * @param parent QObject owner on the UI thread.
     */
    explicit ModelPreflightController(QObject* parent = nullptr);
    ~ModelPreflightController() override;

    /**
     * @brief Request a fresh preflight; only the newest generation may update state.
     * @param request Effective config, explicit mode, options and candidate probe path.
     */
    void RequestPreflight(const UiModelPreflightRequest& request);

    /**
     * @brief Re-run the latest request after a stale or completed result.
     */
    void Recheck();

    /**
     * @brief Cancel pending capability and geometry work without blocking the UI thread.
     */
    void Cancel();

    /**
     * @brief Mark the current result stale after config or resource selection changes.
     */
    void MarkStale();

    /**
     * @brief Change the presented mode without repeating shared geometry diagnostics.
     * @param mode Explicit mode selected by the caller.
     */
    void SetMode(slicer_core::ModelPreflightPipelineMode mode);

    /**
     * @brief Return the latest UI-thread execution snapshot.
     * @return Current model preflight result and execution metadata.
     */
    const slicer_core::ModelPreflightExecutionResult& CurrentExecution() const;

    /**
     * @brief Return the mode currently evaluated by the UI.
     * @return Legacy or global surface shell mode.
     */
    slicer_core::ModelPreflightPipelineMode CurrentMode() const;

    /**
     * @brief Report whether a capability probe or geometry worker is active.
     * @return True while the current logical request is running.
     */
    bool IsRunning() const;

    /**
     * @brief Return the last capability probe diagnostic for UI and smoke evidence.
     * @return Schema/exit/parser summary without changing admission behavior.
     */
    QString LastCapabilityDiagnostic() const;

    /**
     * @brief Override capability probing for deterministic UI smoke tests.
     * @param available Fixed backend availability, or nullopt to use the real CLI probe.
     */
    void SetCapabilityOverrideForTests(std::optional<bool> available);

signals:
    void SigStateChanged();

private slots:
    void OnCapabilityFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void OnCapabilityError(QProcess::ProcessError error);
    void OnCapabilityTimeout();

private:
    struct CallbackState;

    void TryStartPending();
    void StartWorker(bool globalBackendAvailable);
    void OnWorkerCompleted(
        quint64 generation,
        bool globalBackendAvailable,
        slicer_core::ModelPreflightExecutionResult execution);
    void SetLifecycleState(slicer_core::ModelPreflightStatus status);
    void PublishCurrent();

    std::shared_ptr<slicer_core::ModelPreflightService> m_service;
    std::shared_ptr<CallbackState> m_callbackState;
    std::shared_ptr<std::atomic_bool> m_activeCancellation;
    QProcess m_capabilityProcess;
    QTimer m_capabilityTimeout;
    UiModelPreflightRequest m_lastRequest;
    UiModelPreflightRequest m_pendingRequest;
    UiModelPreflightRequest m_activeRequest;
    slicer_core::ModelPreflightExecutionResult m_currentExecution;
    slicer_core::ModelPreflightPipelineMode m_currentMode{
        slicer_core::ModelPreflightPipelineMode::Legacy};
    std::optional<bool> m_capabilityOverride;
    QString m_lastCapabilityDiagnostic;
    quint64 m_generation{0U};
    quint64 m_capabilityGeneration{0U};
    bool m_hasLastRequest{false};
    bool m_hasPendingRequest{false};
    bool m_workerRunning{false};
    bool m_capabilityRunning{false};
};
