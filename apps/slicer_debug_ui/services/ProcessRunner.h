#pragma once

#include <QObject>
#include <QElapsedTimer>
#include <QProcess>
#include <QTimer>

class ProcessRunner final : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Create a reusable external-process runner.
     * @param parent QObject owner.
     */
    explicit ProcessRunner(QObject* parent = nullptr);

    /**
     * @brief Report whether the external process is still alive.
     * @return True until QProcess reports NotRunning.
     */
    bool IsRunning() const;

    /**
     * @brief Start one external command.
     * @param program Executable path.
     * @param args Command-line arguments.
     * @param workingDir Process working directory.
     */
    void Run(
        const QString& program,
        const QStringList& args,
        const QString& workingDir);

    /**
     * @brief Request cancellation and force termination after a grace period.
     */
    void Stop();

signals:
    void SigStarted(const QString& command);
    void SigOutput(const QString& text);
    void SigErrorOutput(const QString& text);
    void SigFinished(int exitCode, qint64 elapsedMs);
    void SigFailed(const QString& message);

private:
    QString FormatCommand(
        const QString& program,
        const QStringList& args) const;
    void FlushOutput();

    static constexpr int kTerminateGracePeriodMs{1000};

    QProcess m_process;
    QElapsedTimer m_timer;
    QTimer m_forceKillTimer;
    bool m_stopRequested{false};
};
