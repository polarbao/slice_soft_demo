#pragma once

#include "slicer_core/config/SlicePipelineConfig.h"

#include <QObject>
#include <QString>
#include <QStringList>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>

/**
 * @brief Stable states of the current-scene slice action.
 */
enum class SceneSliceActionState
{
    Idle,
    Snapshotting,
    Preflighting,
    Slicing,
    Cancelling,
    Validating,
    LoadingResult,
    Completed,
    Blocked,
    Failed,
    Cancelled,
};

/**
 * @brief Stable failures emitted by the current-scene slice action.
 */
enum class SceneSliceActionErrorCode
{
    None,
    SceneUnavailable,
    ImportInProgress,
    InstanceBlocked,
    PipelineModeNotAdmitted,
    SnapshotFailed,
    SceneStale,
    ProcessLaunchFailed,
    ProcessFailed,
    PackageInvalid,
    Cancelled,
};

/**
 * @brief Current editable scene facts required by the action gate.
 */
struct SceneSliceActionSceneState
{
    QString sceneid;
    std::uint64_t scenerevision{0U};
    std::size_t visibleinstancecount{0U};
    bool importinprogress{false};
    bool allvisibleinstancesadmitted{false};
};

/**
 * @brief Immutable scene snapshot written before launching the CLI.
 */
struct SceneSliceActionSnapshot
{
    QString sceneid;
    std::uint64_t scenerevision{0U};
    QString scenehash;
    QString effectiveconfighash;
    QString effectiveconfigpath;
    QString profileid;
    QString sessionid;
    QString packagedir;
    slicer_core::SlicePipelineMode mode{
        slicer_core::SlicePipelineMode::Legacy};
};

/**
 * @brief Result of freezing and writing one scene snapshot.
 */
struct SceneSliceSnapshotResult
{
    std::optional<SceneSliceActionSnapshot> snapshot;
    QString errorcode;
    QString message;

    /**
     * @brief Report whether the immutable snapshot was written.
     * @return True when a snapshot is present and no error exists.
     */
    bool IsValid() const;
};

/**
 * @brief Result of validating one scene production package.
 */
struct SceneSlicePackageValidationResult
{
    bool valid{false};
    QString packagedir;
    QStringList errors;
};

/**
 * @brief User-selected mode used to start the current-scene action.
 */
struct SceneSliceActionRequest
{
    slicer_core::SlicePipelineMode mode{
        slicer_core::SlicePipelineMode::Legacy};
};

/**
 * @brief Convert a scene slice state to a stable ASCII name.
 * @param state State to convert.
 * @return Stable state name.
 */
std::string_view SceneSliceActionStateName(
    SceneSliceActionState state);

/**
 * @brief Convert a scene slice failure to a stable ASCII name.
 * @param code Error code to convert.
 * @return Stable failure name.
 */
std::string_view SceneSliceActionErrorCodeName(
    SceneSliceActionErrorCode code);

/**
 * @brief Orchestrate frozen scene snapshot, CLI process, validation, and result loading.
 */
class SceneSliceActionController final : public QObject
{
    Q_OBJECT

public:
    using SceneStateProvider =
        std::function<SceneSliceActionSceneState()>;
    using SnapshotWriter =
        std::function<SceneSliceSnapshotResult(
            const SceneSliceActionRequest&)>;
    using ProcessStarter =
        std::function<bool(const SceneSliceActionSnapshot&)>;
    using ProcessCanceller = std::function<void()>;
    using PackageValidator =
        std::function<SceneSlicePackageValidationResult(
            const SceneSliceActionSnapshot&,
            qint64)>;

    /**
     * @brief Create a current-scene slice action controller.
     * @param parent QObject owner.
     */
    explicit SceneSliceActionController(
        QObject* parent = nullptr);

    /**
     * @brief Install scene, snapshot, process, and package boundaries.
     * @param sceneStateProvider Current SceneDocument state provider.
     * @param snapshotWriter Immutable scene effective-config writer.
     * @param processStarter Explicit scene CLI launcher.
     * @param processCanceller Active process cancellation callback.
     * @param packageValidator Strict package and identity validator.
     */
    void Configure(
        SceneStateProvider sceneStateProvider,
        SnapshotWriter snapshotWriter,
        ProcessStarter processStarter,
        ProcessCanceller processCanceller,
        PackageValidator packageValidator);

    /**
     * @brief Start slicing the current frozen scene.
     * @param request Selected production mode.
     * @return True when the explicit scene process was launched.
     */
    bool Start(const SceneSliceActionRequest& request);

    /**
     * @brief Cancel the active current-scene process.
     */
    void Cancel();

    /**
     * @brief Consume normal process completion.
     * @param exitCode Process exit code.
     * @param elapsedMs Process elapsed milliseconds.
     */
    void OnProcessFinished(int exitCode, qint64 elapsedMs);

    /**
     * @brief Consume process launch or runtime failure.
     * @param message Process failure details.
     */
    void OnProcessFailed(const QString& message);

    /**
     * @brief Return the current state.
     * @return Current action state.
     */
    SceneSliceActionState State() const;

    /**
     * @brief Return whether the controller owns an active process.
     * @return True while slicing, cancelling, or validating.
     */
    bool IsRunning() const;

    /**
     * @brief Return the latest stable blocking or failure code.
     * @return Stable error code.
     */
    SceneSliceActionErrorCode ErrorCode() const;

    /**
     * @brief Return the latest Chinese status detail.
     * @return Current detail text.
     */
    QString Message() const;

signals:
    void SigStateChanged(
        SceneSliceActionState state,
        const QString& message);
    void SigPackageReady(const QString& packageDir);
    void SigFailed(const QString& code, const QString& message);

private:
    bool Fail(
        SceneSliceActionState state,
        SceneSliceActionErrorCode code,
        const QString& message);
    void Publish(
        SceneSliceActionState state,
        const QString& message);
    bool SnapshotMatchesCurrentScene() const;

    SceneStateProvider m_sceneStateProvider;
    SnapshotWriter m_snapshotWriter;
    ProcessStarter m_processStarter;
    ProcessCanceller m_processCanceller;
    PackageValidator m_packageValidator;
    SceneSliceActionState m_state{SceneSliceActionState::Idle};
    SceneSliceActionErrorCode m_errorcode{
        SceneSliceActionErrorCode::None};
    QString m_message;
    std::optional<SceneSliceActionSnapshot> m_snapshot;
};
