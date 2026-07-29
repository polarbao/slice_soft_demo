#pragma once

#include "slicer_core/diagnostics/TextureFillPartitionReleaseBenchmark.h"
#include "slicer_core/scene/ModelInstance.h"
#include "slicer_core/scene/SceneModel.h"
#include "slicer_core/texture_image.h"

#include <QObject>
#include <QString>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>

/**
 * @brief Immutable identity attached to one diagnostic analysis generation.
 */
struct DiagnosticAnalysisIdentity
{
    QString sessionid;
    QString sceneid;
    QString modelid;
    QString instanceid;
    QString confighash;
    std::uint64_t scenerevision{0U};
    std::uint64_t transformrevision{0U};

    /**
     * @brief Compare every field used to reject stale diagnostic results.
     * @param other Identity to compare.
     * @return True only when all identity and revision fields match.
     */
    bool Matches(
        const DiagnosticAnalysisIdentity& other) const;

    /**
     * @brief Report whether the minimum scene-aware identity is complete.
     * @return True when stable ids and config hash are present.
     */
    bool IsComplete() const;
};

/**
 * @brief Stable lifecycle states for the asynchronous diagnostic analysis.
 */
enum class DiagnosticAnalysisState
{
    Idle,
    Running,
    Succeeded,
    Failed,
    Cancelled,
    Stale,
};

/**
 * @brief Immutable inputs consumed away from the Qt UI thread.
 */
struct DiagnosticAnalysisRequest
{
    DiagnosticAnalysisIdentity identity;
    std::shared_ptr<const slicer_core::SceneModel> sourcemodel;
    slicer_core::ModelInstance instance;
    slicer_core::TextureSampleOptions textureoptions;
    QString modelpath;
    QString modelfillmaterial{QStringLiteral("white")};
    double texturesurfacewidthmm{0.10};
    double classificationresolutionmm{0.10};
    int paddingvoxels{1};
};

/**
 * @brief Background diagnostic result projected for the UI and future preview.
 */
struct DiagnosticAnalysisResult
{
    DiagnosticAnalysisIdentity identity;
    DiagnosticAnalysisState state{
        DiagnosticAnalysisState::Idle};
    QString message;
    QString error;
    std::optional<double> maximumwidthmm;
    std::optional<double> alltexturethresholdmm;
    std::optional<bool> alltexture;
    std::optional<std::uint64_t> modelvoxels;
    std::optional<std::uint64_t> texturesurfacevoxels;
    std::optional<std::uint64_t> modelfillvoxels;
    std::optional<double> totalcorems;
    std::shared_ptr<const slicer_core::
        TextureFillPartitionReleaseBenchmarkResult>
        evidence;
};

/**
 * @brief Runs texture/fill diagnostics on QThreadPool with generation safety.
 */
class DiagnosticAnalysisWorker final : public QObject
{
    Q_OBJECT

public:
    using Executor = std::function<
        DiagnosticAnalysisResult(
            const DiagnosticAnalysisRequest&,
            const std::shared_ptr<std::atomic_bool>&)>;

    /**
     * @brief Create a worker using the production diagnostic executor.
     * @param parent QObject owner.
     */
    explicit DiagnosticAnalysisWorker(
        QObject* parent = nullptr);

    /**
     * @brief Create a worker with a deterministic executor for testing.
     * @param executor Function executed on QThreadPool.
     * @param parent QObject owner.
     */
    explicit DiagnosticAnalysisWorker(
        Executor executor,
        QObject* parent = nullptr);

    ~DiagnosticAnalysisWorker() override;

    /**
     * @brief Start a new generation and logically cancel an older generation.
     * @param request Immutable scene, config, and geometry inputs.
     * @return True when the request was accepted.
     */
    bool Start(const DiagnosticAnalysisRequest& request);

    /**
     * @brief Request cooperative cancellation of the current generation.
     */
    void Cancel();

    /**
     * @brief Report whether the newest generation is active.
     * @return True until the newest generation reaches one terminal state.
     */
    bool IsRunning() const;

    /**
     * @brief Return the current lifecycle state.
     * @return Idle, running, or the newest terminal state.
     */
    DiagnosticAnalysisState State() const;

signals:
    /**
     * @brief Emitted after a complete request starts.
     * @param identity Frozen request identity.
     */
    void SigStarted(
        const DiagnosticAnalysisIdentity& identity);

    /**
     * @brief Emitted once for the newest generation terminal result.
     * @param result Success, failure, or cancellation result.
     */
    void SigFinished(
        const DiagnosticAnalysisResult& result);

    /**
     * @brief Emitted when an older generation completes after replacement.
     * @param identity Discarded request identity.
     */
    void SigStaleDiscarded(
        const DiagnosticAnalysisIdentity& identity);

private:
    struct CallbackState;

    static DiagnosticAnalysisResult ExecuteDefault(
        const DiagnosticAnalysisRequest& request,
        const std::shared_ptr<std::atomic_bool>& cancellation);

    void OnWorkerCompleted(
        quint64 generation,
        const std::shared_ptr<std::atomic_bool>& cancellation,
        DiagnosticAnalysisResult result);

    Executor m_executor;
    std::shared_ptr<CallbackState> m_callbackState;
    std::shared_ptr<std::atomic_bool> m_activeCancellation;
    quint64 m_generation{0U};
    DiagnosticAnalysisState m_state{
        DiagnosticAnalysisState::Idle};
    bool m_running{false};
};
