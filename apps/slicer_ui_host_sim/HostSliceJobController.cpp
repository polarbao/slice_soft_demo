#include "HostSliceJobController.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QUuid>

#include <algorithm>

namespace
{
constexpr int kPollIntervalMs{250};

bool ParseObject(
    const QByteArray& bytes,
    QJsonObject* object,
    QString* error)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(bytes, &parseError);
    if (!document.isObject())
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("模块返回的 JSON 对象无效：%1")
                         .arg(parseError.errorString());
        }
        return false;
    }
    *object = document.object();
    return true;
}

bool IsTerminalState(const QString& state)
{
    return state == QStringLiteral("succeeded")
        || state == QStringLiteral("failed")
        || state == QStringLiteral("cancelled");
}

HostSliceJobState ParseState(const QString& state)
{
    if (state == QStringLiteral("queued"))
    {
        return HostSliceJobState::Queued;
    }
    if (state == QStringLiteral("running"))
    {
        return HostSliceJobState::Running;
    }
    if (state == QStringLiteral("cancelling"))
    {
        return HostSliceJobState::Cancelling;
    }
    if (state == QStringLiteral("succeeded"))
    {
        return HostSliceJobState::Succeeded;
    }
    if (state == QStringLiteral("failed"))
    {
        return HostSliceJobState::Failed;
    }
    if (state == QStringLiteral("cancelled"))
    {
        return HostSliceJobState::Cancelled;
    }
    return HostSliceJobState::Idle;
}

QString NewIdentity(const QString& prefix)
{
    return prefix + QStringLiteral("-")
        + QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString EnsureSha256Prefix(const QString& value)
{
    return value.startsWith(QStringLiteral("sha256:"))
        ? value
        : QStringLiteral("sha256:%1").arg(value);
}

QString NormalizePathIdentity(const QString& value)
{
    QString result = QDir::fromNativeSeparators(
        QFileInfo(value).absoluteFilePath());
#if defined(Q_OS_WIN)
    result = result.toCaseFolded();
#endif
    return QDir::cleanPath(result);
}
}

HostSliceJobController::HostSliceJobController(
    ModuleClient& client,
    QObject* parent)
    : QObject(parent),
      m_client(client)
{
    m_pollTimer.setInterval(kPollIntervalMs);
    m_pollTimer.setTimerType(Qt::CoarseTimer);
    connect(
        &m_pollTimer,
        &QTimer::timeout,
        this,
        &HostSliceJobController::OnPollTimer);
}

HostSliceJobController::~HostSliceJobController()
{
    m_pollTimer.stop();
    if (m_job != nullptr)
    {
        QString ignored;
        (void)m_client.Cancel(m_job, &ignored);
        ReleaseJob();
    }
}

bool HostSliceJobController::Start(
    const quint64 sceneHandle,
    const hosteffectiveprofile& effectiveProfile,
    QString* error)
{
    if (IsActive())
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("已有切片作业正在运行。");
        }
        return false;
    }

    QJsonObject request;
    QString packageDirectory;
    if (!BuildRequest(
            sceneHandle,
            effectiveProfile,
            &request,
            &packageDirectory,
            error))
    {
        return false;
    }

    m_job = m_client.Submit(
        QJsonDocument(request).toJson(QJsonDocument::Compact),
        error);
    if (m_job == nullptr)
    {
        return false;
    }

    m_requestedPackageDirectory = packageDirectory;
    m_completion = {};
    m_cancelRequested = false;
    m_progress = {};
    m_progress.state = HostSliceJobState::Queued;
    m_progress.phase = QStringLiteral("queued");
    m_jobTimer.start();
    PublishProgress();
    m_pollTimer.start();
    return true;
}

bool HostSliceJobController::Cancel(QString* error)
{
    if (!IsActive())
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("当前没有可取消的切片作业。");
        }
        return false;
    }
    if (m_cancelRequested)
    {
        return true;
    }
    if (!m_client.Cancel(m_job, error))
    {
        return false;
    }
    m_cancelRequested = true;
    m_cancelTimer.start();
    m_progress.state = HostSliceJobState::Cancelling;
    m_progress.phase = QStringLiteral("cancelling");
    PublishProgress();
    return true;
}

bool HostSliceJobController::IsActive() const
{
    return m_job != nullptr;
}

HostSliceJobState HostSliceJobController::State() const
{
    return m_progress.state;
}

hostslicejobprogress HostSliceJobController::Progress() const
{
    return m_progress;
}

hostslicejobcompletion HostSliceJobController::Completion() const
{
    return m_completion;
}

QString HostSliceJobController::StateId(const HostSliceJobState state)
{
    switch (state)
    {
    case HostSliceJobState::Idle:
        return QStringLiteral("idle");
    case HostSliceJobState::Queued:
        return QStringLiteral("queued");
    case HostSliceJobState::Running:
        return QStringLiteral("running");
    case HostSliceJobState::Cancelling:
        return QStringLiteral("cancelling");
    case HostSliceJobState::Succeeded:
        return QStringLiteral("succeeded");
    case HostSliceJobState::Failed:
        return QStringLiteral("failed");
    case HostSliceJobState::Cancelled:
        return QStringLiteral("cancelled");
    }
    return QStringLiteral("idle");
}

bool HostSliceJobController::BuildRequest(
    const quint64 sceneHandle,
    const hosteffectiveprofile& effectiveProfile,
    QJsonObject* request,
    QString* packageDirectory,
    QString* error)
{
    if (sceneHandle == 0U || effectiveProfile.profile.isEmpty()
        || effectiveProfile.profilehash.isEmpty()
        || request == nullptr || packageDirectory == nullptr)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("场景和有效 Profile 必须先通过校验。");
        }
        return false;
    }
    if (effectiveProfile.profile.value(
            QStringLiteral("profileHash")).toString()
        != effectiveProfile.profilehash)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("有效 Profile 哈希与对象内容不闭合。");
        }
        return false;
    }

    const QJsonObject output = effectiveProfile.profile.value(
        QStringLiteral("output")).toObject();
    const QString requestedDirectory = QDir::cleanPath(
        output.value(QStringLiteral("packageDir")).toString());
    if (requestedDirectory.isEmpty()
        || !QFileInfo(requestedDirectory).isAbsolute())
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("有效 Profile 缺少绝对输出目录。");
        }
        return false;
    }

    const QJsonObject snapshotRequest{
        {QStringLiteral("capability"), QStringLiteral("scene.get_snapshot")},
        {QStringLiteral("sceneHandle"), static_cast<qint64>(sceneHandle)}};
    QByteArray snapshotBytes;
    if (!m_client.Execute(
            QJsonDocument(snapshotRequest).toJson(QJsonDocument::Compact),
            &snapshotBytes,
            error))
    {
        return false;
    }
    QJsonObject snapshot;
    if (!ParseObject(snapshotBytes, &snapshot, error)
        || !snapshot.value(QStringLiteral("ok")).toBool()
        || !snapshot.value(QStringLiteral("scene")).isObject())
    {
        if (error != nullptr && error->isEmpty())
        {
            *error = QStringLiteral("场景快照未返回权威 scene 对象。");
        }
        return false;
    }
    const QString snapshotSceneHash = snapshot.value(
        QStringLiteral("sceneHash")).toString();
    if (snapshotSceneHash.isEmpty())
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("场景快照缺少 sceneHash。");
        }
        return false;
    }
    const QString sceneHash = EnsureSha256Prefix(snapshotSceneHash);

    *packageDirectory = QDir::fromNativeSeparators(requestedDirectory);
    *request = QJsonObject{
        {QStringLiteral("capability"), QStringLiteral("slice.rgbwsv")},
        {QStringLiteral("jobId"), NewIdentity(QStringLiteral("host-slice"))},
        {QStringLiteral("correlationId"),
         NewIdentity(QStringLiteral("host-correlation"))},
        {QStringLiteral("sceneHash"), sceneHash},
        {QStringLiteral("scene"), snapshot.value(QStringLiteral("scene"))},
        {QStringLiteral("profile"), effectiveProfile.profile},
        {QStringLiteral("output"), QJsonObject{
             {QStringLiteral("contract"), QStringLiteral("p0.rgbwsv.2")},
             {QStringLiteral("packageDir"), *packageDirectory}}},
        {QStringLiteral("options"), QJsonObject{
             {QStringLiteral("backend"), QStringLiteral("worker")}}}};
    return true;
}

bool HostSliceJobController::ApplyProgress(
    const QJsonObject& progress,
    QString* error)
{
    const QString stateId = progress.value(QStringLiteral("state")).toString();
    const HostSliceJobState state = ParseState(stateId);
    const int percent = progress.value(QStringLiteral("percent")).toInt(-1);
    if (state == HostSliceJobState::Idle || percent < 0 || percent > 100
        || percent < m_progress.percent)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("模块返回了无效或回退的作业进度。");
        }
        return false;
    }
    m_progress.state = state;
    m_progress.phase = progress.value(QStringLiteral("phase")).toString();
    m_progress.current = progress.value(QStringLiteral("current")).toInt();
    m_progress.total = progress.value(QStringLiteral("total")).toInt();
    m_progress.percent = percent;
    m_progress.elapsedms = progress.value(
        QStringLiteral("elapsedMs")).toVariant().toLongLong();
    PublishProgress();
    return true;
}

void HostSliceJobController::OnPollTimer()
{
    QByteArray progressBytes;
    QString error;
    if (!m_client.Poll(m_job, &progressBytes, &error))
    {
        FinishTransportFailure(error);
        return;
    }
    QJsonObject progress;
    if (!ParseObject(progressBytes, &progress, &error)
        || !ApplyProgress(progress, &error))
    {
        FinishTransportFailure(error);
        return;
    }
    const QString state = progress.value(QStringLiteral("state")).toString();
    if (IsTerminalState(state))
    {
        FinishTerminal(state);
    }
}

void HostSliceJobController::FinishTerminal(const QString& terminalState)
{
    m_pollTimer.stop();
    QByteArray resultBytes;
    QString error;
    QJsonObject result;
    const bool read = m_client.Result(m_job, &resultBytes, &error)
        && ParseObject(resultBytes, &result, &error);
    ReleaseJob();

    m_completion = {};
    m_completion.elapsedms = m_jobTimer.isValid() ? m_jobTimer.elapsed() : 0;
    m_completion.cancellatencyms = m_cancelRequested
        && m_cancelTimer.isValid() ? m_cancelTimer.elapsed() : -1;
    m_completion.result = result;
    m_completion.cancelled = terminalState == QStringLiteral("cancelled");
    m_completion.success = read
        && terminalState == QStringLiteral("succeeded")
        && result.value(QStringLiteral("ok")).toBool();
    m_completion.code = result.value(QStringLiteral("code")).toString();
    m_completion.message = result.value(
        QStringLiteral("message")).toString();
    m_completion.detail = result.value(
        QStringLiteral("detail")).toString();
    m_completion.timing = result.value(
        QStringLiteral("timing")).toObject();
    if (!m_completion.timing.contains(QStringLiteral("totalMs")))
    {
        const double workerElapsedMs = result.value(
            QStringLiteral("elapsedMs")).toDouble(-1.0);
        if (workerElapsedMs >= 0.0)
        {
            m_completion.timing.insert(
                QStringLiteral("totalMs"), workerElapsedMs);
        }
    }
    if (result.value(QStringLiteral("error")).isObject())
    {
        const QJsonObject resultError = result.value(
            QStringLiteral("error")).toObject();
        const QString nestedCode = resultError.value(
            QStringLiteral("code")).toString();
        if (!nestedCode.isEmpty())
        {
            m_completion.code = nestedCode;
        }
        m_completion.message = resultError.value(
            QStringLiteral("message")).toString();
        m_completion.detail = resultError.value(
            QStringLiteral("detail")).toString();
    }
    if (!read)
    {
        m_completion.message = error;
    }
    if (m_completion.success)
    {
        m_completion.packagedirectory = result.value(
            QStringLiteral("packageDir")).toString();
        if (NormalizePathIdentity(m_completion.packagedirectory)
            != NormalizePathIdentity(m_requestedPackageDirectory))
        {
            m_completion.success = false;
            m_completion.code = QStringLiteral("HOST-SLICE-OUTPUT-MISMATCH");
            m_completion.message = QStringLiteral(
                "模块返回的 packageDir 与提交目标不一致：requested=%1, actual=%2")
                .arg(
                    m_requestedPackageDirectory,
                    m_completion.packagedirectory);
            m_progress.state = HostSliceJobState::Failed;
        }
    }

    emit SigCompleted(
        m_completion.success,
        m_completion.cancelled,
        m_completion.code,
        m_completion.message,
        m_completion.detail,
        m_completion.packagedirectory,
        m_completion.timing,
        m_completion.elapsedms,
        m_completion.cancellatencyms);
}

void HostSliceJobController::FinishTransportFailure(const QString& message)
{
    m_pollTimer.stop();
    if (m_job != nullptr)
    {
        QString ignored;
        (void)m_client.Cancel(m_job, &ignored);
        ReleaseJob();
    }
    m_progress.state = HostSliceJobState::Failed;
    m_completion = {};
    m_completion.code = QStringLiteral("HOST-SLICE-TRANSPORT-FAILED");
    m_completion.message = message.isEmpty()
        ? QStringLiteral("切片作业通信失败。") : message;
    m_completion.elapsedms = m_jobTimer.isValid() ? m_jobTimer.elapsed() : 0;
    emit SigCompleted(
        false,
        false,
        m_completion.code,
        m_completion.message,
        QString{},
        QString{},
        QJsonObject{},
        m_completion.elapsedms,
        -1);
}

void HostSliceJobController::PublishProgress()
{
    emit SigProgressChanged(
        StateId(m_progress.state),
        m_progress.phase,
        m_progress.current,
        m_progress.total,
        m_progress.percent,
        m_progress.elapsedms);
}

void HostSliceJobController::ReleaseJob()
{
    if (m_job != nullptr)
    {
        m_client.Release(m_job);
        m_job = nullptr;
    }
}
