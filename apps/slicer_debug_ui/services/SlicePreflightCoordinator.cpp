#include "SlicePreflightCoordinator.h"

SlicePreflightCoordinator::SlicePreflightCoordinator(
    ModelPreflightController* controller,
    QObject* parent)
    : QObject(parent), m_controller(controller)
{
    connect(
        m_controller,
        &ModelPreflightController::SigStateChanged,
        this,
        &SlicePreflightCoordinator::OnControllerStateChanged);
}

void SlicePreflightCoordinator::RequestAction(
    const SlicePreflightAction& action)
{
    m_pendingAction = action;
    m_pendingWarningCodes.clear();
    m_hasPendingAction = true;
    m_waitingConfirmation = false;
    m_lastHandledGeneration = 0U;

    UiModelPreflightRequest request;
    request.configpath = action.configpath;
    request.capabilityprogram = action.capabilityprogram;
    request.mode = ActionMode();
    if (action.kind == SlicePreflightActionKind::Legacy)
    {
        request.globalbackendavailabilityoverride = false;
    }
    else if (action.kind == SlicePreflightActionKind::GlobalProduction)
    {
        // The admitted 08D production pipeline is part of the normal slicer_cli.
        request.globalbackendavailabilityoverride = true;
    }
    m_controller->RequestPreflight(request);
}

void SlicePreflightCoordinator::ConfirmLegacyWarning(const bool accepted)
{
    if (!m_hasPendingAction || !m_waitingConfirmation)
    {
        return;
    }
    m_waitingConfirmation = false;
    if (!accepted)
    {
        m_hasPendingAction = false;
        emit SigActionBlocked();
        return;
    }

    m_admittedAction = m_pendingAction;
    m_hasPendingAction = false;
    emit SigActionAdmitted();
}

void SlicePreflightCoordinator::CancelPending()
{
    m_hasPendingAction = false;
    m_waitingConfirmation = false;
    m_pendingWarningCodes.clear();
    m_controller->Cancel();
}

SlicePreflightAction SlicePreflightCoordinator::AdmittedAction() const
{
    return m_admittedAction;
}

QStringList SlicePreflightCoordinator::PendingWarningCodes() const
{
    return m_pendingWarningCodes;
}

bool SlicePreflightCoordinator::HasPendingAction() const
{
    return m_hasPendingAction;
}

void SlicePreflightCoordinator::OnControllerStateChanged()
{
    if (!m_hasPendingAction)
    {
        return;
    }

    const slicer_core::ModelPreflightExecutionResult& execution =
        m_controller->CurrentExecution();
    if (execution.result.status == slicer_core::ModelPreflightStatus::Running
        || execution.result.status == slicer_core::ModelPreflightStatus::Pending
        || execution.result.status == slicer_core::ModelPreflightStatus::NotRun)
    {
        return;
    }
    if (execution.generation == m_lastHandledGeneration)
    {
        return;
    }
    m_lastHandledGeneration = execution.generation;

    if (m_pendingAction.kind == SlicePreflightActionKind::OpenVdbDiagnostic
        && execution.result.status != slicer_core::ModelPreflightStatus::Cancelled
        && execution.result.status != slicer_core::ModelPreflightStatus::Stale)
    {
        m_admittedAction = m_pendingAction;
        m_hasPendingAction = false;
        emit SigActionAdmitted();
        return;
    }

    const slicer_core::ModeAdmissionResult& admission = CurrentAdmission();
    if (admission.status == slicer_core::ModelPreflightAdmissionStatus::Blocked)
    {
        m_hasPendingAction = false;
        emit SigActionBlocked();
        return;
    }

    if (admission.status == slicer_core::ModelPreflightAdmissionStatus::Warning
        && ActionMode() == slicer_core::ModelPreflightPipelineMode::Legacy)
    {
        m_pendingWarningCodes.clear();
        for (const std::string& code : admission.warningCodes)
        {
            m_pendingWarningCodes.push_back(QString::fromStdString(code));
        }
        m_waitingConfirmation = true;
        emit SigLegacyConfirmationRequired();
        return;
    }

    m_admittedAction = m_pendingAction;
    m_hasPendingAction = false;
    emit SigActionAdmitted();
}

const slicer_core::ModeAdmissionResult&
SlicePreflightCoordinator::CurrentAdmission() const
{
    const slicer_core::ModelPreflightResult& result =
        m_controller->CurrentExecution().result;
    return ActionMode() == slicer_core::ModelPreflightPipelineMode::Legacy
        ? result.legacyAdmission
        : result.globalAdmission;
}

slicer_core::ModelPreflightPipelineMode
SlicePreflightCoordinator::ActionMode() const
{
    return m_pendingAction.kind == SlicePreflightActionKind::Legacy
        ? slicer_core::ModelPreflightPipelineMode::Legacy
        : slicer_core::ModelPreflightPipelineMode::GlobalSurfaceShell;
}
