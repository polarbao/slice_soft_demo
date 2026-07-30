#include "SceneSliceActionController.h"

#include <utility>

bool SceneSliceSnapshotResult::IsValid() const
{
    return snapshot.has_value()
        && errorcode.isEmpty()
        && message.isEmpty();
}

std::string_view SceneSliceActionStateName(
    const SceneSliceActionState state)
{
    switch (state)
    {
    case SceneSliceActionState::Idle:
        return "idle";
    case SceneSliceActionState::Snapshotting:
        return "snapshotting";
    case SceneSliceActionState::Preflighting:
        return "preflighting";
    case SceneSliceActionState::Slicing:
        return "slicing";
    case SceneSliceActionState::Cancelling:
        return "cancelling";
    case SceneSliceActionState::Validating:
        return "validating";
    case SceneSliceActionState::LoadingResult:
        return "loading_result";
    case SceneSliceActionState::Completed:
        return "completed";
    case SceneSliceActionState::Blocked:
        return "blocked";
    case SceneSliceActionState::Failed:
        return "failed";
    case SceneSliceActionState::Cancelled:
        return "cancelled";
    }
    return "failed";
}

std::string_view SceneSliceActionErrorCodeName(
    const SceneSliceActionErrorCode code)
{
    switch (code)
    {
    case SceneSliceActionErrorCode::None:
        return "NONE";
    case SceneSliceActionErrorCode::SceneUnavailable:
        return "SCENE_SLICE_SCENE_UNAVAILABLE";
    case SceneSliceActionErrorCode::ImportInProgress:
        return "SCENE_SLICE_IMPORT_IN_PROGRESS";
    case SceneSliceActionErrorCode::InstanceBlocked:
        return "SCENE_SLICE_INSTANCE_BLOCKED";
    case SceneSliceActionErrorCode::PipelineModeNotAdmitted:
        return "SCENE_SLICE_PIPELINE_MODE_NOT_ADMITTED";
    case SceneSliceActionErrorCode::SnapshotFailed:
        return "SCENE_SLICE_SNAPSHOT_FAILED";
    case SceneSliceActionErrorCode::SceneStale:
        return "SCENE_SLICE_STALE";
    case SceneSliceActionErrorCode::ProcessLaunchFailed:
        return "SCENE_SLICE_PROCESS_LAUNCH_FAILED";
    case SceneSliceActionErrorCode::ProcessFailed:
        return "SCENE_SLICE_PROCESS_FAILED";
    case SceneSliceActionErrorCode::PackageInvalid:
        return "SCENE_SLICE_PACKAGE_INVALID";
    case SceneSliceActionErrorCode::Cancelled:
        return "SCENE_SLICE_CANCELLED";
    }
    return "SCENE_SLICE_PROCESS_FAILED";
}

SceneSliceActionController::SceneSliceActionController(
    QObject* parent)
    : QObject(parent)
{
}

void SceneSliceActionController::Configure(
    SceneStateProvider sceneStateProvider,
    SnapshotWriter snapshotWriter,
    ProcessStarter processStarter,
    ProcessCanceller processCanceller,
    PackageValidator packageValidator)
{
    m_sceneStateProvider = std::move(sceneStateProvider);
    m_snapshotWriter = std::move(snapshotWriter);
    m_processStarter = std::move(processStarter);
    m_processCanceller = std::move(processCanceller);
    m_packageValidator = std::move(packageValidator);
}

bool SceneSliceActionController::Start(
    const SceneSliceActionRequest& request)
{
    if (IsRunning())
    {
        emit SigFailed(
            QStringLiteral(
                "SCENE_SLICE_PROCESS_LAUNCH_FAILED"),
            QStringLiteral(
                "已有当前场景切片任务正在执行。"));
        return false;
    }
    m_snapshot.reset();
    m_errorcode = SceneSliceActionErrorCode::None;

    if (!m_sceneStateProvider
        || !m_snapshotWriter
        || !m_processStarter
        || !m_processCanceller
        || !m_packageValidator)
    {
        return Fail(
            SceneSliceActionState::Failed,
            SceneSliceActionErrorCode::ProcessLaunchFailed,
            QStringLiteral("当前场景切片控制器尚未完成依赖配置。"));
    }

    const SceneSliceActionSceneState scene =
        m_sceneStateProvider();
    if (scene.importinprogress)
    {
        return Fail(
            SceneSliceActionState::Blocked,
            SceneSliceActionErrorCode::ImportInProgress,
            QStringLiteral("批量导入完成后才能切片当前场景。"));
    }
    if (scene.sceneid.trimmed().isEmpty()
        || scene.scenerevision == 0U
        || scene.visibleinstancecount == 0U)
    {
        return Fail(
            SceneSliceActionState::Blocked,
            SceneSliceActionErrorCode::SceneUnavailable,
            QStringLiteral("当前场景没有可切片的可见模型。"));
    }
    if (!scene.allvisibleinstancesadmitted)
    {
        return Fail(
            SceneSliceActionState::Blocked,
            SceneSliceActionErrorCode::InstanceBlocked,
            QStringLiteral("当前场景存在未通过预检的可见模型。"));
    }
    if (request.mode
        != slicer_core::SlicePipelineMode::Legacy)
    {
        return Fail(
            SceneSliceActionState::Blocked,
            SceneSliceActionErrorCode::PipelineModeNotAdmitted,
            QStringLiteral(
                "Global 多模型生产尚未准入，且禁止回退到 Legacy。"));
    }

    Publish(
        SceneSliceActionState::Snapshotting,
        QStringLiteral("正在冻结当前场景快照。"));
    const SceneSliceSnapshotResult written =
        m_snapshotWriter(request);
    if (!written.IsValid())
    {
        return Fail(
            SceneSliceActionState::Failed,
            SceneSliceActionErrorCode::SnapshotFailed,
            written.message.isEmpty()
                ? QStringLiteral("当前场景生效配置写入失败。")
                : written.message);
    }
    m_snapshot = written.snapshot;
    if (m_snapshot->sceneid != scene.sceneid
        || m_snapshot->scenerevision != scene.scenerevision
        || m_snapshot->effectiveconfigpath.trimmed().isEmpty()
        || m_snapshot->packagedir.trimmed().isEmpty())
    {
        return Fail(
            SceneSliceActionState::Blocked,
            SceneSliceActionErrorCode::SceneStale,
            QStringLiteral("场景在快照写入期间发生变化。"));
    }

    Publish(
        SceneSliceActionState::Preflighting,
        QStringLiteral("场景快照和生产合同校验通过。"));
    if (!m_processStarter(*m_snapshot))
    {
        return Fail(
            SceneSliceActionState::Failed,
            SceneSliceActionErrorCode::ProcessLaunchFailed,
            QStringLiteral("无法启动当前场景切片进程。"));
    }
    Publish(
        SceneSliceActionState::Slicing,
        QStringLiteral("正在切片当前场景。"));
    return true;
}

void SceneSliceActionController::Cancel()
{
    if (!IsRunning()
        || m_state == SceneSliceActionState::Cancelling)
    {
        return;
    }
    m_errorcode = SceneSliceActionErrorCode::Cancelled;
    Publish(
        SceneSliceActionState::Cancelling,
        QStringLiteral("正在取消当前场景切片，请等待进程退出。"));
    m_processCanceller();
}

void SceneSliceActionController::OnProcessFinished(
    const int exitCode,
    const qint64 elapsedMs)
{
    if (m_state == SceneSliceActionState::Cancelling)
    {
        m_snapshot.reset();
        Publish(
            SceneSliceActionState::Cancelled,
            QStringLiteral(
                "当前场景切片已取消，已有输出不会自动加载。"));
        return;
    }
    if (m_state == SceneSliceActionState::Cancelled
        || !m_snapshot.has_value())
    {
        return;
    }
    if (m_state != SceneSliceActionState::Slicing)
    {
        return;
    }
    if (exitCode != 0)
    {
        Fail(
            SceneSliceActionState::Failed,
            SceneSliceActionErrorCode::ProcessFailed,
            QStringLiteral("当前场景切片进程失败，退出码=%1。")
                .arg(exitCode));
        m_snapshot.reset();
        return;
    }
    if (!SnapshotMatchesCurrentScene())
    {
        Fail(
            SceneSliceActionState::Blocked,
            SceneSliceActionErrorCode::SceneStale,
            QStringLiteral(
                "切片期间场景 revision 已变化，拒绝加载过期输出包。"));
        m_snapshot.reset();
        return;
    }

    Publish(
        SceneSliceActionState::Validating,
        QStringLiteral("正在校验场景生产包和冻结身份。"));
    const SceneSlicePackageValidationResult validation =
        m_packageValidator(*m_snapshot, elapsedMs);
    if (!validation.valid)
    {
        Fail(
            SceneSliceActionState::Failed,
            SceneSliceActionErrorCode::PackageInvalid,
            validation.errors.isEmpty()
                ? QStringLiteral("场景生产包校验失败。")
                : validation.errors.join(QStringLiteral("\n")));
        m_snapshot.reset();
        return;
    }

    Publish(
        SceneSliceActionState::LoadingResult,
        QStringLiteral("正在加载单一场景 Package。"));
    const QString packageDir = validation.packagedir.isEmpty()
        ? m_snapshot->packagedir
        : validation.packagedir;
    emit SigPackageReady(packageDir);
    Publish(
        SceneSliceActionState::Completed,
        QStringLiteral("当前场景切片完成并已加载生产 TIFF 预览。"));
    m_snapshot.reset();
}

void SceneSliceActionController::OnProcessFailed(
    const QString& message)
{
    if (m_state == SceneSliceActionState::Cancelled
        || m_state == SceneSliceActionState::Cancelling)
    {
        return;
    }
    if (!IsRunning())
    {
        return;
    }
    Fail(
        SceneSliceActionState::Failed,
        SceneSliceActionErrorCode::ProcessFailed,
        QStringLiteral("当前场景切片进程错误：") + message);
    m_snapshot.reset();
}

SceneSliceActionState SceneSliceActionController::State() const
{
    return m_state;
}

bool SceneSliceActionController::IsRunning() const
{
    return m_state == SceneSliceActionState::Snapshotting
        || m_state == SceneSliceActionState::Preflighting
        || m_state == SceneSliceActionState::Slicing
        || m_state == SceneSliceActionState::Cancelling
        || m_state == SceneSliceActionState::Validating
        || m_state == SceneSliceActionState::LoadingResult;
}

SceneSliceActionErrorCode
SceneSliceActionController::ErrorCode() const
{
    return m_errorcode;
}

QString SceneSliceActionController::Message() const
{
    return m_message;
}

bool SceneSliceActionController::Fail(
    const SceneSliceActionState state,
    const SceneSliceActionErrorCode code,
    const QString& message)
{
    m_errorcode = code;
    Publish(state, message);
    emit SigFailed(
        QString::fromLatin1(
            SceneSliceActionErrorCodeName(code).data()),
        message);
    return false;
}

void SceneSliceActionController::Publish(
    const SceneSliceActionState state,
    const QString& message)
{
    m_state = state;
    m_message = message;
    emit SigStateChanged(state, message);
}

bool SceneSliceActionController::SnapshotMatchesCurrentScene() const
{
    if (!m_snapshot.has_value() || !m_sceneStateProvider)
    {
        return false;
    }
    const SceneSliceActionSceneState current =
        m_sceneStateProvider();
    return !current.importinprogress
        && current.sceneid == m_snapshot->sceneid
        && current.scenerevision == m_snapshot->scenerevision;
}
