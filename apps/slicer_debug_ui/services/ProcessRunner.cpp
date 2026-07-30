#include "ProcessRunner.h"

ProcessRunner::ProcessRunner(QObject* parent)
    : QObject(parent)
{
    m_process.setProcessChannelMode(QProcess::SeparateChannels);
    m_forceKillTimer.setSingleShot(true);
    m_forceKillTimer.setInterval(kTerminateGracePeriodMs);

    connect(
        &m_forceKillTimer,
        &QTimer::timeout,
        this,
        [this]()
        {
            if (IsRunning())
            {
                m_process.kill();
            }
        });
    connect(
        &m_process,
        &QProcess::readyReadStandardOutput,
        this,
        [this]()
        {
            const QByteArray standardOutput =
                m_process.readAllStandardOutput();
            if (!standardOutput.isEmpty())
            {
                emit SigOutput(
                    QString::fromLocal8Bit(standardOutput));
            }
        });
    connect(
        &m_process,
        &QProcess::readyReadStandardError,
        this,
        [this]()
        {
            const QByteArray standardError =
                m_process.readAllStandardError();
            if (!standardError.isEmpty())
            {
                emit SigErrorOutput(
                    QString::fromLocal8Bit(standardError));
            }
        });
    connect(
        &m_process,
        &QProcess::errorOccurred,
        this,
        [this](const QProcess::ProcessError error)
        {
            if (m_stopRequested
                && error == QProcess::Crashed)
            {
                return;
            }
            emit SigFailed(m_process.errorString());
        });
    connect(
        &m_process,
        qOverload<int, QProcess::ExitStatus>(
            &QProcess::finished),
        this,
        [this](
            const int exitCode,
            const QProcess::ExitStatus status)
        {
            Q_UNUSED(status);
            m_forceKillTimer.stop();
            FlushOutput();
            m_stopRequested = false;
            emit SigFinished(
                exitCode,
                m_timer.isValid() ? m_timer.elapsed() : 0);
        });
}

bool ProcessRunner::IsRunning() const
{
    return m_process.state() != QProcess::NotRunning;
}

void ProcessRunner::Run(
    const QString& program,
    const QStringList& args,
    const QString& workingDir)
{
    if (IsRunning())
    {
        emit SigFailed(QStringLiteral("已有命令正在执行"));
        return;
    }
    m_forceKillTimer.stop();
    m_stopRequested = false;
    m_process.setProgram(program);
    m_process.setArguments(args);
    m_process.setWorkingDirectory(workingDir);
    m_timer.restart();
    emit SigStarted(FormatCommand(program, args));
    m_process.start();
}

void ProcessRunner::Stop()
{
    if (!IsRunning() || m_stopRequested)
    {
        return;
    }
    m_stopRequested = true;
    m_process.terminate();
    m_forceKillTimer.start();
}

QString ProcessRunner::FormatCommand(
    const QString& program,
    const QStringList& args) const
{
    QStringList parts;
    parts.push_back(program);
    for (const QString& arg : args)
    {
        if (arg.contains(QLatin1Char(' ')))
        {
            parts.push_back(
                QLatin1Char('"') + arg + QLatin1Char('"'));
        }
        else
        {
            parts.push_back(arg);
        }
    }
    return parts.join(QLatin1Char(' '));
}

void ProcessRunner::FlushOutput()
{
    const QByteArray standardOutput =
        m_process.readAllStandardOutput();
    if (!standardOutput.isEmpty())
    {
        emit SigOutput(QString::fromLocal8Bit(standardOutput));
    }
    const QByteArray standardError =
        m_process.readAllStandardError();
    if (!standardError.isEmpty())
    {
        emit SigErrorOutput(QString::fromLocal8Bit(standardError));
    }
}
