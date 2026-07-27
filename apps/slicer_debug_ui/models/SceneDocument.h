#pragma once

#include "slicer_core/layout/GridLayoutPolicy.h"
#include "slicer_core/scene/SceneViewGeometry.h"
#include "slicer_core/preflight/TransformedModelPreflight.h"

#include <QObject>
#include <QString>

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

enum class SceneDocumentState
{
    Unloaded,
    Loading,
    Ready,
    Blocked,
    Failed,
    Cancelled,
};

enum class SceneTransformedPreflightState
{
    NotRun,
    Running,
    Ready,
    Failed,
    Cancelled,
    Stale,
};

enum class SceneDocumentOperationErrorCode
{
    None,
    SceneRevisionStale,
    InstanceLimitExceeded,
    InstanceIdDuplicate,
    InstanceNotFound,
    InstanceLocked,
    SceneIdentityMismatch,
    LayoutInvalid,
    LayoutRestoreUnavailable,
};

struct SceneDocumentOperationError
{
    SceneDocumentOperationErrorCode code{
        SceneDocumentOperationErrorCode::None};
    QString field;
    QString message;
};

struct SceneDocumentOperationResult
{
    bool changed{false};
    std::optional<SceneDocumentOperationError> error;

    /**
     * @brief Report whether the scene command passed validation.
     * @return True when no structured error exists.
     */
    bool IsValid() const;
};

struct SceneDocumentItem
{
    QString modelpath;
    QString sourcecachekey;
    QString sourcehash;
    QString resourcehash;
    slicer_core::ModelInstance instance;
    slicer_core::ModelTransform requestedtransform;
    slicer_core::ModelTransform derivedlayouttransform;
    int layoutrow{-1};
    int layoutcolumn{-1};
    std::optional<slicer_core::SceneViewGeometry> geometry;
};

/**
 * @brief Return the stable name for one scene document command error.
 * @param code Scene document command error.
 * @return Stable ASCII error name.
 */
std::string_view SceneDocumentOperationErrorCodeName(
    SceneDocumentOperationErrorCode code);

class SceneDocument final : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Create the UI scene document.
     * @param parent QObject owner.
     */
    explicit SceneDocument(QObject* parent = nullptr);

    /**
     * @brief Reset the document to its unloaded state.
     */
    void Reset();

    /**
     * @brief Begin one asynchronous model-view generation.
     * @param generation Monotonic request generation.
     * @param modelPath Model path shown by the UI.
     */
    void SetLoading(quint64 generation, const QString& modelPath);

    /**
     * @brief Begin appending one model without discarding the current scene.
     * @param generation Monotonic request generation.
     * @param modelPath Model path being imported.
     */
    void SetAdding(quint64 generation, const QString& modelPath);

    /**
     * @brief Attach immutable source identity and the editable instance.
     * @param generation Current load generation.
     * @param sceneId Stable scene identity.
     * @param sceneRevision Current scene revision.
     * @param sourceCacheKey Immutable source repository key.
     * @param sourceHash Source content identity.
     * @param resourceHash Adjacent resource identity.
     * @param instance Editable instance initialized by the importer.
     * @return True when the generation was current.
     */
    bool SetSceneContext(
        quint64 generation,
        const QString& sceneId,
        quint64 sceneRevision,
        const QString& sourceCacheKey,
        const QString& sourceHash,
        const QString& resourceHash,
        slicer_core::ModelInstance instance);

    /**
     * @brief Append one imported model instance atomically.
     * @param generation Current append generation.
     * @param sceneId Existing scene identity.
     * @param sceneRevision Next scene revision.
     * @param sourceCacheKey Immutable source repository key.
     * @param sourceHash Source content identity.
     * @param resourceHash Adjacent resource identity.
     * @param instance Imported scene instance.
     * @return True when the complete append was accepted.
     */
    bool AddSceneContext(
        quint64 generation,
        const QString& sceneId,
        quint64 sceneRevision,
        const QString& sourceCacheKey,
        const QString& sourceHash,
        const QString& resourceHash,
        slicer_core::ModelInstance instance);

    /**
     * @brief Begin a transform reprojection without discarding last geometry.
     * @param generation Current projection generation.
     */
    void SetProjectionLoading(quint64 generation);

    /**
     * @brief Publish geometry for the current generation.
     * @param generation Generation that produced the geometry.
     * @param geometry Immutable projected scene geometry.
     * @return True when the generation was current and the result was accepted.
     */
    bool SetGeometry(
        quint64 generation,
        slicer_core::SceneViewGeometry geometry);

    /**
     * @brief Publish a loading failure for the current generation.
     * @param generation Generation that failed.
     * @param error User-facing failure detail.
     * @return True when the generation was current and the result was accepted.
     */
    bool SetFailure(quint64 generation, const QString& error);

    /**
     * @brief Mark the current generation cancelled.
     * @param generation Current generation.
     * @return True when the generation was current.
     */
    bool SetCancelled(quint64 generation);

    /**
     * @brief Return the current lifecycle state.
     * @return Scene document state.
     */
    SceneDocumentState State() const;

    /**
     * @brief Return the active request generation.
     * @return Monotonic generation.
     */
    quint64 Generation() const;

    /**
     * @brief Return the selected model path.
     * @return Model path shown by the UI.
     */
    QString ModelPath() const;

    /**
     * @brief Return the latest failure detail.
     * @return Empty text when no failure is active.
     */
    QString Error() const;

    /**
     * @brief Return the current projected geometry.
     * @return Geometry when a ready or blocked model is available.
     */
    const std::optional<slicer_core::SceneViewGeometry>& Geometry() const;

    /**
     * @brief Return the current editable instance.
     * @return Instance when a source has been loaded.
     */
    const std::optional<slicer_core::ModelInstance>& Instance() const;

    /**
     * @brief Return all scene items in stable insertion order.
     * @return Ordered scene item snapshots.
     */
    const std::vector<SceneDocumentItem>& Items() const;

    /**
     * @brief Return the number of editable model instances.
     * @return Scene instance count in the range 0..22.
     */
    std::size_t InstanceCount() const;

    /**
     * @brief Return the current instance identity.
     * @return Empty text when the scene has no current instance.
     */
    QString CurrentInstanceId() const;

    /**
     * @brief Make one existing instance current without changing revision.
     * @param instanceId Stable instance identity.
     * @return True when the instance exists.
     */
    bool SetCurrentInstance(const QString& instanceId);

    /**
     * @brief Duplicate one instance while sharing immutable source data.
     * @param sourceInstanceId Existing source instance identity.
     * @param newInstanceId New stable instance identity.
     * @param expectedSceneRevision Caller-observed scene revision.
     * @return Atomic command result.
     */
    SceneDocumentOperationResult DuplicateInstance(
        const QString& sourceInstanceId,
        const QString& newInstanceId,
        quint64 expectedSceneRevision);

    /**
     * @brief Delete one unlocked scene instance.
     * @param instanceId Existing instance identity.
     * @param expectedSceneRevision Caller-observed scene revision.
     * @return Atomic command result.
     */
    SceneDocumentOperationResult DeleteInstance(
        const QString& instanceId,
        quint64 expectedSceneRevision);

    /**
     * @brief Change one instance visibility.
     * @param instanceId Existing instance identity.
     * @param visible Requested visibility.
     * @param expectedSceneRevision Caller-observed scene revision.
     * @return Atomic command result.
     */
    SceneDocumentOperationResult SetInstanceVisible(
        const QString& instanceId,
        bool visible,
        quint64 expectedSceneRevision);

    /**
     * @brief Change one instance lock state.
     * @param instanceId Existing instance identity.
     * @param locked Requested lock state.
     * @param expectedSceneRevision Caller-observed scene revision.
     * @return Atomic command result.
     */
    SceneDocumentOperationResult SetInstanceLocked(
        const QString& instanceId,
        bool locked,
        quint64 expectedSceneRevision);

    /**
     * @brief Return current deterministic scene layout settings.
     * @return Grid-layout settings persisted with the scene.
     */
    const slicer_core::SceneLayout& Layout() const;

    /**
     * @brief Apply one atomic deterministic row-major layout.
     * @param layout Requested 11x2 grid settings.
     * @param expectedSceneRevision Caller-observed scene revision.
     * @return Atomic command result; failures leave every item unchanged.
     */
    SceneDocumentOperationResult ApplyGridLayout(
        const slicer_core::SceneLayout& layout,
        quint64 expectedSceneRevision);

    /**
     * @brief Report whether the latest successful layout can be restored.
     * @return True when an unmodified pre-layout snapshot exists.
     */
    bool CanRestoreGridLayout() const;

    /**
     * @brief Restore transforms captured before the latest successful layout.
     * @param expectedSceneRevision Caller-observed scene revision.
     * @return Atomic restore result.
     */
    SceneDocumentOperationResult RestoreGridLayout(
        quint64 expectedSceneRevision);

    /**
     * @brief Return the current scene identity.
     * @return Stable scene id.
     */
    QString SceneId() const;

    /**
     * @brief Return the current scene revision.
     * @return Monotonic scene revision.
     */
    quint64 SceneRevision() const;

    /**
     * @brief Return the immutable source repository key.
     * @return Empty text when no source is loaded.
     */
    QString SourceCacheKey() const;

    /**
     * @brief Return source content identity.
     * @return Source SHA-256 identity or empty text.
     */
    QString SourceHash() const;

    /**
     * @brief Return adjacent resource identity.
     * @return Resource SHA-256 identity or empty text.
     */
    QString ResourceHash() const;

    /**
     * @brief Report whether the scene differs from its saved revision.
     * @return True after an effective transform change.
     */
    bool IsDirty() const;

    /**
     * @brief Report whether projected geometry trails the instance revision.
     * @return True while reprojection is pending or failed.
     */
    bool IsGeometryStale() const;

    /**
     * @brief Return the saved scene draft path.
     * @return Empty text before the first successful save.
     */
    QString SceneConfigPath() const;

    /**
     * @brief Return the saved effective config path.
     * @return Empty text before the first successful save.
     */
    QString EffectiveConfigPath() const;

    /**
     * @brief Begin transformed preflight for the current revisions.
     * @param generation Monotonic preflight request generation.
     * @param sceneRevision Expected current scene revision.
     * @param transformRevision Expected current transform revision.
     * @return True when the request identity was accepted.
     */
    bool SetTransformedPreflightRunning(
        quint64 generation,
        quint64 sceneRevision,
        quint64 transformRevision);

    /**
     * @brief Publish transformed preflight for the current revisions.
     * @param generation Request generation.
     * @param execution Source/transformed admission evidence.
     * @return True when identity and revisions were current.
     */
    bool SetTransformedPreflightResult(
        quint64 generation,
        slicer_core::TransformedModelPreflightExecution execution);

    /**
     * @brief Publish transformed preflight failure.
     * @param generation Request generation.
     * @param error Stable diagnostic detail.
     * @return True when the generation was current.
     */
    bool SetTransformedPreflightFailure(
        quint64 generation,
        const QString& error);

    /**
     * @brief Mark transformed preflight cancelled.
     * @param generation Request generation.
     * @return True when the generation was current.
     */
    bool SetTransformedPreflightCancelled(quint64 generation);

    /**
     * @brief Return transformed preflight lifecycle.
     * @return Current lifecycle state.
     */
    SceneTransformedPreflightState TransformedPreflightState() const;

    /**
     * @brief Return current transformed preflight evidence.
     * @return Evidence retained for the matching revision.
     */
    const std::optional<
        slicer_core::TransformedModelPreflightExecution>&
    TransformedPreflight() const;

    /**
     * @brief Return the transformed preflight error detail.
     * @return Empty when no failure is active.
     */
    QString TransformedPreflightError() const;

    /**
     * @brief Commit one validated instance update atomically.
     * @param instance Updated instance with incremented transform revision.
     * @param expectedSceneRevision Caller-observed scene revision.
     * @return True when committed.
     */
    bool CommitInstance(
        const slicer_core::ModelInstance& instance,
        quint64 expectedSceneRevision);

    /**
     * @brief Mark the current revisions saved after effective-config readback.
     * @param sceneConfigPath Saved scene draft.
     * @param effectiveConfigPath Saved effective config.
     * @param configHash Effective config hash.
     * @param sceneRevision Saved scene revision.
     * @param transformRevision Saved transform revision.
     * @return True when revisions still match.
     */
    bool MarkSaved(
        const QString& sceneConfigPath,
        const QString& effectiveConfigPath,
        const QString& configHash,
        quint64 sceneRevision,
        quint64 transformRevision);

signals:
    void SigChanged();

private:
    static constexpr std::size_t kMaximumInstanceCount{22U};

    struct SceneLayoutSnapshotItem
    {
        QString instanceid;
        slicer_core::ModelTransform requestedtransform;
        slicer_core::ModelTransform derivedlayouttransform;
        slicer_core::ModelTransform effectivetransform;
        slicer_core::BoundingBox effectivebboxmm;
        int layoutrow{-1};
        int layoutcolumn{-1};
    };

    struct SceneLayoutSnapshot
    {
        slicer_core::SceneLayout layout;
        std::vector<SceneLayoutSnapshotItem> items;
    };

    void PublishState(SceneDocumentState state);
    void ClearCurrentInstance();
    void LoadCurrentItem(std::size_t index);
    void SyncCurrentItem();
    void AdvanceSceneRevision();
    void InvalidateLayoutRestore();
    void TranslateGeometry(
        SceneDocumentItem& item,
        double translateX,
        double translateY,
        const std::string& transformHash);
    bool SnapshotMatchesCurrentScene() const;
    std::optional<std::size_t> FindItemIndex(
        const QString& instanceId) const;
    SceneDocumentOperationResult ValidateOperation(
        const QString& instanceId,
        quint64 expectedSceneRevision) const;
    SceneDocumentOperationResult OperationFailure(
        SceneDocumentOperationErrorCode code,
        const QString& field,
        const QString& message) const;

    SceneDocumentState m_state{SceneDocumentState::Unloaded};
    quint64 m_generation{0U};
    QString m_modelPath;
    QString m_pendingModelPath;
    QString m_error;
    std::optional<slicer_core::SceneViewGeometry> m_geometry;
    std::optional<slicer_core::ModelInstance> m_instance;
    QString m_sceneId;
    quint64 m_sceneRevision{0U};
    QString m_sourceCacheKey;
    QString m_sourceHash;
    QString m_resourceHash;
    bool m_dirty{false};
    bool m_geometryStale{false};
    QString m_sceneConfigPath;
    QString m_effectiveConfigPath;
    QString m_effectiveConfigHash;
    std::vector<SceneDocumentItem> m_items;
    QString m_currentInstanceId;
    slicer_core::SceneLayout m_layout;
    std::optional<SceneLayoutSnapshot> m_layoutSnapshot;
    bool m_additionInProgress{false};
    SceneTransformedPreflightState m_transformedPreflightState{
        SceneTransformedPreflightState::NotRun};
    quint64 m_transformedPreflightGeneration{0U};
    quint64 m_transformedPreflightSceneRevision{0U};
    quint64 m_transformedPreflightTransformRevision{0U};
    std::optional<
        slicer_core::TransformedModelPreflightExecution>
        m_transformedPreflight;
    QString m_transformedPreflightError;
};
