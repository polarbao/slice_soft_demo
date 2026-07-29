#pragma once

#include "../services/SliceProgressProtocolParser.h"

#include <cstdint>
#include <QElapsedTimer>
#include <QWidget>

class QLabel;
class QProgressBar;
class QTimer;

/**
 * @brief Display live slicing progress and post-run timing breakdown.
 */
class SliceTimingPanel final : public QWidget
{
public:
    explicit SliceTimingPanel(QWidget* parent = nullptr);

    /**
     * @brief Reset the panel for a newly started action.
     * @param action User-facing action name.
     */
    void Reset(const QString& action);

    /**
     * @brief Apply one live progress event.
     * @param event Parsed slicer progress event.
     */
    void UpdateProgress(const SliceProgressEvent& event);

    /**
     * @brief Display detailed timing values emitted by slicer_cli.
     * @param event Parsed slicer timing event.
     */
    void ShowTiming(const SliceTimingEvent& event);

    /**
     * @brief Mark the process as finished and preserve any detailed timing values.
     * @param success True when the process exited successfully.
     * @param processelapsedms QProcess wall-clock elapsed milliseconds.
     */
    void Finish(bool success, qint64 processelapsedms);

    /**
     * @brief Return a compact text summary for UI smoke tests.
     * @return Current phase and timing text.
     */
    QString SummaryText() const;

private:
    static QString FormatDuration(double milliseconds);
    static QString FormatBytes(std::uint64_t bytes);
    static QString PhaseText(const SliceProgressEvent& event);
    static QString EngineText(const QString& engine);
    void SetValue(QLabel* label, double milliseconds);

    QLabel* m_phaseLabel{nullptr};
    QLabel* m_engineLabel{nullptr};
    QProgressBar* m_progressBar{nullptr};
    QLabel* m_modelLoadValue{nullptr};
    QLabel* m_gridSetupValue{nullptr};
    QLabel* m_sliceProcessingValue{nullptr};
    QLabel* m_layerComputeValue{nullptr};
    QLabel* m_layerComposeValue{nullptr};
    QLabel* m_tiffWriteValue{nullptr};
    QLabel* m_previewWriteValue{nullptr};
    QLabel* m_reportValue{nullptr};
    QLabel* m_outputWriteValue{nullptr};
    QLabel* m_totalValue{nullptr};
    QLabel* m_peakMemoryValue{nullptr};
    QTimer* m_refreshTimer{nullptr};
    QElapsedTimer m_elapsedTimer;
    bool m_hasDetailedTiming{false};
};
