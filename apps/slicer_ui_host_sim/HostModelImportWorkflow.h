#pragma once

#include "HostSliceSettings.h"
#include "ModuleClient.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

class QJsonObject;
class QJsonArray;

/** @brief One issue returned by the synchronous model preflight. */
struct hostpreflightissue
{
    QString code;
    QString severity;
    quint64 count{0};
    QString detail;
};

/** @brief Host-owned presentation data for one imported model instance. */
struct hostmodelimportresult
{
    QString sourcepath;
    QString modelid;
    QString instanceid;
    QString admission;
    quint64 trianglecount{0};
    quint64 vertexcount{0};
    bool hasuv{false};
    bool hasnormals{false};
    double widthmm{0.0};
    double heightmm{0.0};
    double depthmm{0.0};
    QList<hostpreflightissue> issues;
};

/** @brief Incremental transform values committed to selected scene instances. */
struct hosttransformrequest
{
    double deltaxmm{0.0};
    double deltaymm{0.0};
    double deltazmm{0.0};
    double rotatezdegrees{0.0};
    double uniformscalefactor{1.0};
    bool mirrorx{false};
    bool mirrory{false};
};

/** @brief Deterministic grid-layout values owned by the host UI. */
struct hostgridlayoutrequest
{
    int maxcolumns{11};
    int maxrows{2};
    double columngapmm{10.0};
    double rowgapmm{10.0};
};

/** @brief Authoritative summary returned by one scene Commit. */
struct hostsceneeditresult
{
    quint64 scenerevision{0};
    QString scenehash;
    QString viewdataidentity;
    int collisioncount{0};
    int outofboundscount{0};
};

/**
 * @brief Runs the public-SPI model import, scene admission and fast preflight flow.
 *
 * The workflow owns only host session state. It never includes implementation
 * types and never constructs the canonical production scene JSON.
 */
class HostModelImportWorkflow final
{
public:
    /**
     * @brief Creates a workflow bound to one loaded module client.
     * @param client Public ABI client whose module session owns the scene.
     */
    explicit HostModelImportWorkflow(ModuleClient& client);

    /**
     * @brief Imports an OBJ, 3MF, or STL model and adds one scene instance.
     * @param modelPath Existing model path selected by the operator.
     * @param result Receives model metadata, instance identity and preflight data.
     * @param error Receives a user-readable fail-closed reason.
     * @return True when import, addInstance and fast preflight all complete.
     */
    bool ImportModel(
        const QString& modelPath,
        hostmodelimportresult* result,
        QString* error);

    /**
     * @brief Imports and admits multiple models with one atomic scene Commit.
     * @param modelPaths Existing OBJ, 3MF, or STL paths in operator order.
     * @param results Receives one metadata and preflight result per path.
     * @param error Receives a fail-closed reason; no instance is added on failure.
     * @return True when every resource passes preflight and all instances commit.
     */
    bool ImportModels(
        const QStringList& modelPaths,
        QList<hostmodelimportresult>* results,
        QString* error);

    /**
     * @brief Atomically removes existing scene instances.
     * @param instanceIds Stable instance identities selected by the host.
     * @param error Receives a user-readable fail-closed reason.
     * @return True when the module commits the complete removal set.
     */
    bool RemoveInstances(
        const QStringList& instanceIds,
        QString* error);

    /**
     * @brief Atomically applies incremental transforms to selected instances.
     * @param instanceIds Stable instance identities selected by the host.
     * @param request Finite translation, rotation, scale and mirror commands.
     * @param result Receives the authoritative Commit summary.
     * @param error Receives a user-readable fail-closed reason.
     * @return True when all requested operations commit in one revision.
     */
    bool ApplyTransforms(
        const QStringList& instanceIds,
        const hosttransformrequest& request,
        hostsceneeditresult* result,
        QString* error);

    /**
     * @brief Applies authoritative 11x2 grid layout through the public SPI.
     * @param request Valid host-owned row, column and spacing values.
     * @param result Receives the authoritative Commit summary.
     * @param error Receives a user-readable fail-closed reason.
     * @return True when applyGridLayout advances the scene once.
     */
    bool ApplyGridLayout(
        const hostgridlayoutrequest& request,
        hostsceneeditresult* result,
        QString* error);

    /**
     * @brief Returns the module-owned scene handle after the first import.
     * @return Zero before a scene has been created.
     */
    [[nodiscard]] quint64 SceneHandle() const;

    /**
     * @brief Returns the latest committed scene revision.
     * @return Monotonic scene revision, initially zero.
     */
    [[nodiscard]] quint64 SceneRevision() const;

    /** @brief Returns the number of host-tracked imported instances. */
    [[nodiscard]] int InstanceCount() const;

    /**
     * @brief Sets the host-owned context used by the first scene Commit.
     * @param profileId Selected host Profile identity.
     * @param buildVolume Device-owned build volume.
     * @param error Receives an immutable-scene or validation reason.
     * @return True when the pending context is accepted.
     */
    bool SetPendingSceneContext(
        const QString& profileId,
        const hostbuildvolume& buildVolume,
        QString* error);

    /** @brief Returns the Profile identity bound to the current scene. */
    [[nodiscard]] QString SceneProfileId() const;

    /** @brief Returns the device build volume bound to the current scene. */
    [[nodiscard]] hostbuildvolume SceneBuildVolume() const;

    /**
     * @brief Returns a stable imported model path for effective Profile input.
     * @return Lexicographically first current source path, or empty.
     */
    [[nodiscard]] QString ReferenceModelPath() const;

    /**
     * @brief Adopts a scene revision committed by another host controller.
     * @param sceneHandle Existing module-owned scene handle.
     * @param sceneRevision New authoritative revision after Commit or recovery.
     * @param error Receives an identity or monotonicity failure reason.
     * @return True when the workflow now shares the authoritative revision.
     */
    bool AdoptSceneState(
        quint64 sceneHandle,
        quint64 sceneRevision,
        QString* error);

private:
    bool ExecuteObject(
        const QJsonObject& request,
        QJsonObject* response,
        QString* error);
    bool ImportResource(
        const QString& modelPath,
        hostmodelimportresult* result,
        QString* error);
    bool CommitImportedInstances(
        QList<hostmodelimportresult>* results,
        QString* error);
    bool RunFastPreflight(
        const QString& modelId,
        hostmodelimportresult* result,
        QString* error);
    bool CommitSceneOperations(
        const QJsonArray& operations,
        hostsceneeditresult* result,
        QString* error);
    void ReleaseImportedModels(
        const QList<hostmodelimportresult>& results);

    ModuleClient& m_client;
    QHash<QString, QString> m_instanceModels;
    QHash<QString, QString> m_instanceSources;
    QString m_pendingProfileId{QStringLiteral("host-reference-default")};
    hostbuildvolume m_pendingBuildVolume;
    QString m_sceneProfileId;
    hostbuildvolume m_sceneBuildVolume;
    quint64 m_sceneHandle{0};
    quint64 m_sceneRevision{0};
};
