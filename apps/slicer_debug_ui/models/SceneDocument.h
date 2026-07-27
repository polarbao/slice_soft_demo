#pragma once

#include "slicer_core/scene/SceneViewGeometry.h"
#include "slicer_core/preflight/TransformedModelPreflight.h"

#include <QObject>
#include <QString>

#include <optional>

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
    void PublishState(SceneDocumentState state);

    SceneDocumentState m_state{SceneDocumentState::Unloaded};
    quint64 m_generation{0U};
    QString m_modelPath;
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
