#pragma once

#include "HostSliceSettings.h"
#include "ModuleClient.h"

#include <QElapsedTimer>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QTimer>

/** @brief Host-visible lifecycle state for one public slice job. */
enum class HostSliceJobState
{
    Idle,
    Queued,
    Running,
    Cancelling,
    Succeeded,
    Failed,
    Cancelled
};

/** @brief Latest monotonic progress snapshot returned by pm_poll. */
struct hostslicejobprogress
{
    HostSliceJobState state{HostSliceJobState::Idle};
    QString phase;
    int current{0};
    int total{0};
    int percent{0};
    qint64 elapsedms{0};
};

/** @brief Terminal result retained by the host after releasing the job handle. */
struct hostslicejobcompletion
{
    bool success{false};
    bool cancelled{false};
    QString code;
    QString message;
    QString detail;
    QString packagedirectory;
    QJsonObject timing;
    qint64 elapsedms{0};
    qint64 cancellatencyms{-1};
    QJsonObject result;
};

/**
 * @brief Owns one asynchronous slice.rgbwsv job through the frozen public SPI.
 *
 * The controller polls with a Qt timer and never blocks the event loop while a
 * Worker job is active. It releases each public job handle exactly once.
 */
class HostSliceJobController final : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Creates an idle controller bound to one loaded module client.
     * @param client Public ABI client that owns the module session.
     * @param parent Optional Qt object parent.
     */
    explicit HostSliceJobController(
        ModuleClient& client,
        QObject* parent = nullptr);

    /** @brief Cancels and releases an active job before destruction. */
    ~HostSliceJobController() override;

    /**
     * @brief Submits one committed scene and effective Profile for slicing.
     * @param sceneHandle Module-owned committed scene handle.
     * @param effectiveProfile H-B-05 validated Profile and self hash.
     * @param error Receives a fail-closed validation or transport reason.
     * @return True when the Worker job was accepted.
     */
    bool Start(
        quint64 sceneHandle,
        const hosteffectiveprofile& effectiveProfile,
        QString* error);

    /**
     * @brief Requests cooperative cancellation of the active job.
     * @param error Receives a public ABI rejection reason.
     * @return True when cancellation was accepted or already pending.
     */
    bool Cancel(QString* error);

    /** @brief Returns whether a non-terminal public job is retained. */
    [[nodiscard]] bool IsActive() const;

    /** @brief Returns the latest host lifecycle state. */
    [[nodiscard]] HostSliceJobState State() const;

    /** @brief Returns the latest progress snapshot. */
    [[nodiscard]] hostslicejobprogress Progress() const;

    /** @brief Returns the last terminal completion, if any. */
    [[nodiscard]] hostslicejobcompletion Completion() const;

    /**
     * @brief Converts a job state to the frozen lowercase SPI spelling.
     * @param state Host job state.
     * @return Stable state identifier for diagnostics and tests.
     */
    static QString StateId(HostSliceJobState state);

signals:
    /** @brief Publishes one validated monotonic progress snapshot. */
    void SigProgressChanged(
        QString state,
        QString phase,
        int current,
        int total,
        int percent,
        qint64 elapsedMs);

    /** @brief Publishes the terminal result after releasing the job handle. */
    void SigCompleted(
        bool success,
        bool cancelled,
        QString code,
        QString message,
        QString detail,
        QString packageDirectory,
        QJsonObject timing,
        qint64 elapsedMs,
        qint64 cancelLatencyMs);

private slots:
    void OnPollTimer();

private:
    bool BuildRequest(
        quint64 sceneHandle,
        const hosteffectiveprofile& effectiveProfile,
        QJsonObject* request,
        QString* packageDirectory,
        QString* error);
    bool ApplyProgress(const QJsonObject& progress, QString* error);
    void FinishTerminal(const QString& terminalState);
    void FinishTransportFailure(const QString& message);
    void PublishProgress();
    void ReleaseJob();

    ModuleClient& m_client;
    QTimer m_pollTimer;
    QElapsedTimer m_jobTimer;
    QElapsedTimer m_cancelTimer;
    pm_job_t* m_job{nullptr};
    hostslicejobprogress m_progress;
    hostslicejobcompletion m_completion;
    QString m_requestedPackageDirectory;
    bool m_cancelRequested{false};
};
