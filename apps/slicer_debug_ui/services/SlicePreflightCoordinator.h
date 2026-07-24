#pragma once

#include "ModelPreflightController.h"
#include "slicer_core/config/SlicePipelineConfig.h"

#include <QObject>
#include <QStringList>

enum class SlicePreflightActionKind
{
    Legacy,
    GlobalProduction,
    OpenVdbCandidate,
    OpenVdbDiagnostic,
};

struct SlicePreflightAction
{
    SlicePreflightActionKind kind{SlicePreflightActionKind::Legacy};
    slicer_core::SlicePipelineMode productionmode{
        slicer_core::SlicePipelineMode::Legacy};
    QString productionprofileid;
    QString sessionid;
    QString configpath;
    QString packagedir;
    QString reportpath;
    QString capabilityprogram;
};

class SlicePreflightCoordinator final : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Coordinate one pending slice action with the shared preflight controller.
     * @param controller Controller owned by the same UI thread.
     * @param parent QObject owner.
     */
    explicit SlicePreflightCoordinator(
        ModelPreflightController* controller,
        QObject* parent = nullptr);

    /**
     * @brief Submit one slice action and start a fresh preflight generation.
     * @param action Effective config and exact engine action to run after admission.
     */
    void RequestAction(const SlicePreflightAction& action);

    /**
     * @brief Resolve the explicit legacy warning confirmation.
     * @param accepted True only when the operator chose to continue legacy slicing.
     */
    void ConfirmLegacyWarning(bool accepted);

    /**
     * @brief Cancel the pending action and active preflight.
     */
    void CancelPending();

    /**
     * @brief Return the action admitted by the latest generation.
     * @return Exact action to run; valid after SigActionAdmitted.
     */
    SlicePreflightAction AdmittedAction() const;

    /**
     * @brief Return warning codes awaiting explicit legacy confirmation.
     * @return Stable warning codes from the latest admission.
     */
    QStringList PendingWarningCodes() const;

    /**
     * @brief Return whether an action is awaiting preflight or confirmation.
     * @return True while a slice action is pending.
     */
    bool HasPendingAction() const;

signals:
    void SigActionAdmitted();
    void SigActionBlocked();
    void SigLegacyConfirmationRequired();

private slots:
    void OnControllerStateChanged();

private:
    const slicer_core::ModeAdmissionResult& CurrentAdmission() const;
    slicer_core::ModelPreflightPipelineMode ActionMode() const;

    ModelPreflightController* m_controller{nullptr};
    SlicePreflightAction m_pendingAction;
    SlicePreflightAction m_admittedAction;
    QStringList m_pendingWarningCodes;
    quint64 m_lastHandledGeneration{0U};
    bool m_hasPendingAction{false};
    bool m_waitingConfirmation{false};
};
