#include "CapabilityCoverageRunner.h"

#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QThread>

namespace
{
constexpr int kCancelLatencyLimitMs{2000};

QByteArray Compact(const QJsonObject& value)
{
    return QJsonDocument(value).toJson(QJsonDocument::Compact);
}

bool IsSuccessful(const QJsonObject& value)
{
    return value.value(QStringLiteral("ok")).toBool();
}

}

CapabilityCoverageRunner::CapabilityCoverageRunner(ModuleClient& client)
    : m_client(client)
{
}

bool CapabilityCoverageRunner::ExecuteJob(
    const QJsonObject& request,
    const int timeoutMs,
    invocationresult* result,
    QString* error)
{
    if (result == nullptr)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("能力调用结果目标不能为空。");
        }
        return false;
    }
    *result = {};
    QElapsedTimer timer;
    timer.start();
    pm_job_t* job = m_client.Submit(Compact(request), error);
    if (job == nullptr)
    {
        result->elapsedms = timer.elapsed();
        return false;
    }
    result->transportok = true;

    while (timer.elapsed() < timeoutMs)
    {
        QByteArray progressBytes;
        if (!m_client.Poll(job, &progressBytes, error))
        {
            m_client.Release(job);
            result->elapsedms = timer.elapsed();
            return false;
        }
        const QJsonDocument progress = QJsonDocument::fromJson(progressBytes);
        const QString state = progress.object().value(
            QStringLiteral("state")).toString();
        if (state == QStringLiteral("succeeded")
            || state == QStringLiteral("failed")
            || state == QStringLiteral("cancelled"))
        {
            result->terminal = true;
            result->terminalstate = state;
            break;
        }
        QThread::msleep(10);
    }

    if (!result->terminal)
    {
        QString cancelError;
        (void)m_client.Cancel(job, &cancelError);
        m_client.Release(job);
        result->elapsedms = timer.elapsed();
        if (error != nullptr)
        {
            *error = QStringLiteral("能力调用超时：%1")
                         .arg(request.value(
                             QStringLiteral("capability")).toString());
        }
        return false;
    }

    QByteArray resultBytes;
    if (!m_client.Result(job, &resultBytes, error))
    {
        m_client.Release(job);
        result->elapsedms = timer.elapsed();
        return false;
    }
    m_client.Release(job);
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(
        resultBytes,
        &parseError);
    if (!document.isObject())
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("能力结果不是 JSON 对象：%1")
                         .arg(parseError.errorString());
        }
        result->elapsedms = timer.elapsed();
        return false;
    }
    result->payload = document.object();
    result->code = result->payload.value(QStringLiteral("code")).toString();
    result->elapsedms = timer.elapsed();
    return true;
}

void CapabilityCoverageRunner::Record(
    const QString& tier,
    const QString& capability,
    const QString& carrier,
    const QString& requirement,
    const invocationresult& result,
    const bool satisfied)
{
    m_entries.append(QJsonObject{
        {QStringLiteral("tier"), tier},
        {QStringLiteral("capability"), capability},
        {QStringLiteral("carrier"), carrier},
        {QStringLiteral("requirement"), requirement},
        {QStringLiteral("satisfied"), satisfied},
        {QStringLiteral("transportOk"), result.transportok},
        {QStringLiteral("terminal"), result.terminal},
        {QStringLiteral("terminalState"), result.terminalstate},
        {QStringLiteral("resultOk"), IsSuccessful(result.payload)},
        {QStringLiteral("code"), result.code},
        {QStringLiteral("elapsedMs"), result.elapsedms}});
}

bool CapabilityCoverageRunner::RunCancellationGate(
    const QJsonObject& request,
    const QString& evidenceRoot,
    QJsonObject* evidence,
    QString* error)
{
    if (evidence == nullptr)
    {
        return false;
    }
    pm_job_t* job = m_client.Submit(Compact(request), error);
    if (job == nullptr)
    {
        return false;
    }

    QElapsedTimer startWait;
    startWait.start();
    bool running = false;
    while (startWait.elapsed() < 10000)
    {
        QByteArray progressBytes;
        if (!m_client.Poll(job, &progressBytes, error))
        {
            m_client.Release(job);
            return false;
        }
        const QString state = QJsonDocument::fromJson(progressBytes)
                                  .object()
                                  .value(QStringLiteral("state"))
                                  .toString();
        if (state == QStringLiteral("running"))
        {
            running = true;
            break;
        }
        if (state == QStringLiteral("succeeded")
            || state == QStringLiteral("failed"))
        {
            break;
        }
        QThread::msleep(5);
    }

    QElapsedTimer cancelTimer;
    cancelTimer.start();
    const bool cancelAccepted = running && m_client.Cancel(job, error);
    QString terminalState;
    while (cancelAccepted && cancelTimer.elapsed() < kCancelLatencyLimitMs)
    {
        QByteArray progressBytes;
        if (!m_client.Poll(job, &progressBytes, error))
        {
            break;
        }
        terminalState = QJsonDocument::fromJson(progressBytes)
                            .object()
                            .value(QStringLiteral("state"))
                            .toString();
        if (terminalState == QStringLiteral("cancelled")
            || terminalState == QStringLiteral("failed")
            || terminalState == QStringLiteral("succeeded"))
        {
            break;
        }
        QThread::msleep(5);
    }

    QByteArray resultBytes;
    QString resultError;
    const bool resultRead = !terminalState.isEmpty()
        && m_client.Result(job, &resultBytes, &resultError);
    m_client.Release(job);
    const QJsonObject result = QJsonDocument::fromJson(resultBytes).object();
    const QString code = result.value(QStringLiteral("code")).toString();

    QJsonArray residues;
    QDirIterator iterator(
        evidenceRoot,
        QDir::AllEntries | QDir::NoDotAndDotDot,
        QDirIterator::Subdirectories);
    while (iterator.hasNext())
    {
        const QString path = QDir::fromNativeSeparators(iterator.next());
        const QString name = QFileInfo(path).fileName();
        if (name.contains(QStringLiteral(".staging"))
            || name.contains(QStringLiteral(".backup"))
            || name.contains(QStringLiteral(".lease")))
        {
            residues.append(path);
        }
    }
    const bool passed = cancelAccepted
        && terminalState == QStringLiteral("cancelled")
        && resultRead
        && code == QStringLiteral("PM-SLICER-CANCELLED-0070")
        && cancelTimer.elapsed() <= kCancelLatencyLimitMs
        && residues.isEmpty();
    *evidence = QJsonObject{
        {QStringLiteral("gate"), QStringLiteral("UI-M5")},
        {QStringLiteral("cancelAccepted"), cancelAccepted},
        {QStringLiteral("terminalState"), terminalState},
        {QStringLiteral("code"), code},
        {QStringLiteral("latencyMs"), cancelTimer.elapsed()},
        {QStringLiteral("latencyLimitMs"), kCancelLatencyLimitMs},
        {QStringLiteral("residues"), residues},
        {QStringLiteral("passed"), passed}};
    if (!passed && error != nullptr)
    {
        *error = QStringLiteral("UI-M5 取消门禁失败：%1")
                     .arg(QString::fromUtf8(Compact(*evidence)));
    }
    return passed;
}

bool CapabilityCoverageRunner::WriteEvidence(
    const QString& evidenceRoot,
    const QJsonObject& report,
    QString* error) const
{
    const QString path = QDir(evidenceRoot).filePath(
        QStringLiteral("capability_coverage.json"));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        || file.write(QJsonDocument(report).toJson(QJsonDocument::Indented)) < 0)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("无法写入能力覆盖证据：%1")
                         .arg(path);
        }
        return false;
    }
    return true;
}
