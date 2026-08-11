#pragma once

#include <QJsonObject>
#include <QWidget>

class QLabel;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;

/** @brief Operator controls and diagnostics for one reference-host slice job. */
class HostSliceJobPanel final : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Creates an idle slice job panel.
     * @param parent Optional Qt parent widget.
     */
    explicit HostSliceJobPanel(QWidget* parent = nullptr);

    /**
     * @brief Updates whether a validated scene and effective Profile are ready.
     * @param ready True when the start command may be enabled.
     * @param reason User-facing readiness explanation.
     */
    void SetReady(bool ready, const QString& reason);

    /** @brief Marks the panel active after the module accepts a job. */
    void SetActive();

    /**
     * @brief Updates monotonic public job progress.
     * @param state Frozen lowercase lifecycle state.
     * @param phase Worker progress phase.
     * @param current Completed work units.
     * @param total Total work units.
     * @param percent Monotonic percentage in [0, 100].
     * @param elapsedMs Worker elapsed time in milliseconds.
     */
    void UpdateProgress(
        const QString& state,
        const QString& phase,
        int current,
        int total,
        int percent,
        qint64 elapsedMs);

    /**
     * @brief Displays one terminal result and returns controls to idle.
     * @param success True for a valid published package.
     * @param cancelled True for cooperative cancellation.
     * @param code Stable result/error code.
     * @param message User-facing detail.
     * @param detail Additional Worker or preflight diagnostic.
     * @param packageDirectory Published package directory, if any.
     * @param timing Worker-core detailed timing telemetry, if available.
     * @param elapsedMs Host-observed total job time.
     * @param cancelLatencyMs Host-observed cancellation latency, or -1.
     */
    void ShowCompletion(
        bool success,
        bool cancelled,
        const QString& code,
        const QString& message,
        const QString& detail,
        const QString& packageDirectory,
        const QJsonObject& timing,
        qint64 elapsedMs,
        qint64 cancelLatencyMs);

signals:
    /** @brief Requests submission of the current committed scene. */
    void SigStartRequested();

    /** @brief Requests cooperative cancellation of the active job. */
    void SigCancelRequested();

private slots:
    void OnStartRequested();
    void OnCancelRequested();

private:
    void BuildInterface();
    void ApplyTiming(const QJsonObject& timing, qint64 hostElapsedMs);
    void ResetTiming();
    void UpdateButtons();

    QLabel* m_statusLabel{nullptr};
    QLabel* m_phaseLabel{nullptr};
    QProgressBar* m_progressBar{nullptr};
    QPushButton* m_startButton{nullptr};
    QPushButton* m_cancelButton{nullptr};
    QPlainTextEdit* m_detailView{nullptr};
    QLabel* m_engineValue{nullptr};
    QLabel* m_configLoadValue{nullptr};
    QLabel* m_modelLoadValue{nullptr};
    QLabel* m_gridSetupValue{nullptr};
    QLabel* m_sliceProcessingValue{nullptr};
    QLabel* m_layerComputeValue{nullptr};
    QLabel* m_layerComposeValue{nullptr};
    QLabel* m_tiffWriteValue{nullptr};
    QLabel* m_previewWriteValue{nullptr};
    QLabel* m_reportValue{nullptr};
    QLabel* m_outputWriteValue{nullptr};
    QLabel* m_workerTotalValue{nullptr};
    QLabel* m_hostTotalValue{nullptr};
    bool m_ready{false};
    bool m_active{false};
    bool m_hasCompletion{false};
    qint64 m_lastWorkerElapsedMs{0};
};
