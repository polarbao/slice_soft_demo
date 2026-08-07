#pragma once

#include "render/TopViewRenderPolicy.h"

#include <QString>

/**
 * @brief Owns host-local movement prediction over cached top ViewData.
 *
 * The policy has no ModuleClient reference by design. Pointer movement can
 * therefore update matrices and repaint without crossing the DLL boundary.
 */
class MoveOptimizationPolicy final
{
public:
    /**
     * @brief Starts a local movement preview from an authoritative frame.
     * @param frame Current decoded frame.
     * @param instanceId Instance receiving the transient movement.
     * @return True when the instance exists and preview state is active.
     */
    bool Begin(const TopViewFrame& frame, const QString& instanceId);

    /**
     * @brief Applies an absolute local translation delta to the preview.
     * @param deltaXMm Translation along device X in millimetres.
     * @param deltaYMm Translation along device Y in millimetres.
     * @param deltaZMm Translation along device Z in millimetres.
     * @return True when the active instance matrix was updated.
     */
    bool UpdateTranslation(
        double deltaXMm,
        double deltaYMm,
        double deltaZMm);

    /**
     * @brief Accepts the local preview after an authoritative Commit succeeds.
     * @param sceneRevision New authoritative revision.
     * @param viewDataIdentity Commit-returned ViewData identity.
     * @return True when an active preview was promoted.
     */
    bool AcceptCommit(
        quint64 sceneRevision,
        const QString& viewDataIdentity);

    /** @brief Discards local movement and restores the starting frame. */
    void Rollback();

    /**
     * @brief Reports whether a local movement is active.
     * @return True after Begin and before AcceptCommit/Rollback.
     */
    bool IsActive() const;

    /**
     * @brief Returns the current local preview frame.
     * @return Frame suitable for zero-call repainting.
     */
    const TopViewFrame& Frame() const;

private:
    TopViewFrame m_baseline;
    TopViewFrame m_preview;
    QString m_instanceId;
    std::array<double, 16> m_baselineMatrix{};
    int m_instanceIndex{-1};
    bool m_active{false};
};
