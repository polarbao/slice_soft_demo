#pragma once

#include <QJsonObject>
#include <QString>

/**
 * @brief Host-local transient transform and Commit request policy.
 *
 * Transient methods never receive a ModuleClient and therefore cannot cross
 * the DLL boundary during pointer movement.
 */
class TransformCommitPolicy final
{
public:
    /** @brief Creates an inactive transient policy. */
    TransformCommitPolicy();

    /**
     * @brief Starts a local transient translation for one instance.
     * @param instanceId Stable scene instance identity.
     * @return True when the transient state was initialized.
     */
    bool Begin(const QString& instanceId);

    /**
     * @brief Replaces the current local translation preview.
     * @param deltaXMm Translation along device X in millimetres.
     * @param deltaYMm Translation along device Y in millimetres.
     * @param deltaZMm Translation along device Z in millimetres.
     * @return True when an active transient state was updated.
     */
    bool UpdateTranslation(
        double deltaXMm,
        double deltaYMm,
        double deltaZMm);

    /**
     * @brief Discards the local transient state.
     * @return This function does not return a value.
     */
    void Reset();

    /**
     * @brief Reports whether a transient transform is active.
     * @return True while local feedback is waiting for Commit or discard.
     */
    bool IsActive() const;

    /**
     * @brief Builds one atomic scene.apply_operation request.
     * @param sceneHandle Module-owned scene handle established at bootstrap.
     * @param sceneRevision Current and expected authoritative scene revision.
     * @param operationId Unique idempotency identity for this payload.
     * @return Frozen Commit-lane request, or an empty object when inactive.
     */
    QJsonObject BuildRequest(
        quint64 sceneHandle,
        quint64 sceneRevision,
        const QString& operationId) const;

    /**
     * @brief Detects the frozen SceneRevisionStale public error.
     * @param response Terminal scene.apply_operation result.
     * @return True only for PM-SLICER-LAYOUT-0022.
     */
    static bool IsStale(const QJsonObject& response);

private:
    QString m_instanceId;
    double m_deltaXMm{0.0};
    double m_deltaYMm{0.0};
    double m_deltaZMm{0.0};
    bool m_active{false};
};
