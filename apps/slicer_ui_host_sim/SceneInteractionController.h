#pragma once

#include "ModuleClient.h"
#include "TransformCommitPolicy.h"

#include <QJsonObject>
#include <QString>

/** @brief Terminal outcome of one user transform Commit. */
enum class CommitOutcome
{
    Committed,
    StaleRecovered,
    Failed
};

/**
 * @brief Coordinates local transient transforms and authoritative scene commits.
 */
class SceneInteractionController final
{
public:
    /**
     * @brief Creates a controller over one already loaded public module client.
     * @param client Runtime-loaded public ABI client.
     */
    explicit SceneInteractionController(ModuleClient& client);

    /**
     * @brief Bootstraps a module scene and captures its authoritative snapshot.
     * @param scene Frozen multimodel scene JSON.
     * @param error Receives a user-readable failure reason.
     * @return True when scene handle, revision and hash are established.
     */
    bool Initialize(const QJsonObject& scene, QString* error);

    /**
     * @brief Starts local-only transient feedback for an instance.
     * @param instanceId Stable scene instance identity.
     * @return True when the transient state was initialized.
     */
    bool BeginTransient(const QString& instanceId);

    /**
     * @brief Updates local-only translation without a module call.
     * @param deltaXMm Translation along device X in millimetres.
     * @param deltaYMm Translation along device Y in millimetres.
     * @param deltaZMm Translation along device Z in millimetres.
     * @return True when the active transient preview was updated.
     */
    bool UpdateTransientTranslation(
        double deltaXMm,
        double deltaYMm,
        double deltaZMm);

    /**
     * @brief Atomically commits the active transform or performs Stale recovery.
     * @param error Receives a public error or recovery diagnostic.
     * @return Commit result; Stale recovery never auto-retries changed payload.
     */
    CommitOutcome CommitTransient(QString* error);

    /**
     * @brief Reports whether local transient state is active.
     * @return True between BeginTransient and Commit/discard.
     */
    bool HasTransient() const;

    /**
     * @brief Returns the latest authoritative scene revision.
     * @return Current module revision adopted by the host.
     */
    quint64 SceneRevision() const;

    /**
     * @brief Returns the module-owned scene handle.
     * @return Non-zero scene handle after successful initialization.
     */
    quint64 SceneHandle() const;

    /**
     * @brief Returns the latest authoritative scene hash.
     * @return Public production identity for the committed scene.
     */
    QString SceneHash() const;

    /**
     * @brief Returns the ViewData identity adopted from the latest Commit.
     * @return Empty after a snapshot refresh, otherwise the committed identity.
     */
    QString ViewDataIdentity() const;

    /**
     * @brief Returns the number of explicit snapshot recovery/refresh reads.
     * @return Snapshot count; normal Commit must not increment it.
     */
    quint64 SnapshotReadCount() const;

private:
    bool Bootstrap(const QJsonObject& scene, QJsonObject* result, QString* error);
    bool RefreshSnapshot(QString* error);
    bool ExecuteSync(
        const QJsonObject& request,
        QJsonObject* result,
        QString* error);
    bool AdoptCommit(const QJsonObject& result, QString* error);
    bool AdoptSnapshot(const QJsonObject& result, QString* error);

    ModuleClient& m_client;
    TransformCommitPolicy m_transformPolicy;
    QString m_externalSceneId;
    QString m_sceneHash;
    QString m_viewDataIdentity;
    quint64 m_sceneHandle{0};
    quint64 m_sceneRevision{0};
    quint64 m_snapshotReadCount{0};
};
