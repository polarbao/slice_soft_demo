#pragma once

#include "HostRipSettingsStore.h"
#include "HostRipSafety.h"

#include <QElapsedTimer>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVector>

#include <atomic>
#include <cstdint>
#include <memory>

class QThread;

namespace slicesoft::rip
{
struct RipOutputValidationResult;
}

/** @brief Runs one isolated external RIP process and publishes validated output. */
class HostRipJobController final : public QObject
{
    Q_OBJECT

public:
    explicit HostRipJobController(QObject* parent = nullptr);
    ~HostRipJobController() override;

    [[nodiscard]] static QString DefaultModuleDirectory();
    [[nodiscard]] bool CheckRuntime(
        const QString& moduleDirectory,
        QString* error = nullptr) const;
    [[nodiscard]] bool CheckRequest(
        const QString& packageDirectory,
        const QString& moduleDirectory,
        const hostripsettings& settings,
        QString* error = nullptr) const;
    [[nodiscard]] bool Start(
        const QString& packageDirectory,
        const QString& moduleDirectory,
        const hostripsettings& settings,
        QString* error = nullptr);
    [[nodiscard]] bool Cancel(QString* error = nullptr);
    [[nodiscard]] bool IsActive() const;

signals:
    void SigStateChanged(QString state, QString message);
    void SigCompleted(
        bool success,
        bool cancelled,
        QString code,
        QString message,
        QString outputDirectory,
        qint64 elapsedMs);

private:
    enum class Phase
    {
        Idle,
        ValidatingInput,
        RunningProcess,
        ValidatingOutput,
        Publishing
    };

    struct PackageMetadata
    {
        QString packageDirectory;
        QString inputDirectory;
        QString manifestPath;
        QString manifestSha256;
        QStringList layerPaths;
        int layerCount{0};
        std::uint32_t widthPx{0U};
        std::uint32_t heightPx{0U};
    };

    struct RuntimeMetadata
    {
        QString moduleDirectory;
        QString manifestPath;
        QString manifestSha256;
        QString version;
        QString cliPath;
        QString dllPath;
        QString resourceDirectory;
    };

    [[nodiscard]] bool InspectRuntime(
        const QString& moduleDirectory,
        RuntimeMetadata* metadata,
        QString* error) const;
    [[nodiscard]] bool InspectPackage(
        const QString& packageDirectory,
        PackageMetadata* metadata,
        QString* error) const;
    void OnReadyStandardOutput();
    void OnReadyStandardError();
    void OnProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void OnProcessError(QProcess::ProcessError error);
    void OnTimeout();
    void StartInputValidation();
    void StartExternalProcess();
    void StartOutputValidation(int exitCode);
    void FinalizeValidatedOutput(
        int exitCode,
        const slicesoft::rip::RipOutputValidationResult& validation);
    void FinishFailure(const QString& code, const QString& message);
    void FinishCancelled(const QString& message);
    [[nodiscard]] bool CleanupOwnedStaging(QString* error = nullptr);
    void ResetProcess();
    void PublishState(const QString& state, const QString& message);

    QProcess* m_process{nullptr};
    QTimer m_timeoutTimer;
    QTimer m_killTimer;
    QElapsedTimer m_elapsed;
    PackageMetadata m_package;
    RuntimeMetadata m_runtime;
    hostripsettings m_settings;
    QString m_stagingDirectory;
    QByteArray m_stdout;
    QByteArray m_stderr;
    QVector<hostripsourcefileidentity> m_sourceIdentity;
    QThread* m_validationThread{nullptr};
    std::shared_ptr<std::atomic_bool> m_cancelToken;
    quint64 m_jobGeneration{0U};
    Phase m_phase{Phase::Idle};
    bool m_cancelRequested{false};
    bool m_timedOut{false};
};
