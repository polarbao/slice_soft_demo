#pragma once

#include "HostPackageReviewController.h"

#include <QWidget>

class QComboBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QSlider;
class QSpinBox;
class HostChannelChartWidget;

/** @brief Reference-host result workspace for one verified RGBWSV package. */
class HostPackageReviewPanel final : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Creates an empty package review workspace.
     * @param parent Optional Qt parent widget.
     */
    explicit HostPackageReviewPanel(QWidget* parent = nullptr);

    /**
     * @brief Displays a verified package summary and channel chart.
     * @param review Host-owned production package data.
     * @return This function does not return a value.
     */
    void SetPackage(const hostpackagereview& review);

    /**
     * @brief Stores Stage 16 strategy and Worker timing for result diagnostics.
     * @param samplingStrategyId Effective Profile geometry strategy identity.
     * @param timing Worker timing object returned by the completed job.
     */
    void SetStage16Context(
        const QString& samplingStrategyId,
        const QJsonObject& timing);

    /**
     * @brief Displays a module-rendered production layer preview.
     * @param imagePath Host cache path returned by the module.
     * @param layer Layer descriptor corresponding to the image.
     * @return This function does not return a value.
     */
    void ShowPreview(
        const QString& imagePath,
        const hostlayerdescriptor& layer);

    /**
     * @brief Displays the first production layer used as A-side reference.
     * @param imagePath Host cache path returned by the module.
     * @param layer Verified first-layer descriptor.
     */
    void ShowReferencePreview(
        const QString& imagePath,
        const hostlayerdescriptor& layer);

    /**
     * @brief Displays one structured package report.
     * @param report Report returned through package.read_report.
     * @return This function does not return a value.
     */
    void ShowReport(const hostpackagereport& report);

    /**
     * @brief Displays a non-destructive result-view error.
     * @param message User-readable error text.
     * @return This function does not return a value.
     */
    void ShowError(const QString& message);

    /**
     * @brief Returns the currently requested preview channels.
     * @return One or more RGBWSV channel names.
     */
    [[nodiscard]] QStringList SelectedChannels() const;

signals:
    /** @brief Requests a new production TIFF preview. */
    void SigLayerPreviewRequested(int layerIndex, QStringList channels);

    /** @brief Requests one manifest-registered named report. */
    void SigReportRequested(QString reportName);

    /** @brief Requests opening the exact verified production package directory. */
    void SigOpenPackageDirectoryRequested(QString packageDirectory);

private slots:
    void OnLayerSliderChanged(int layerIndex);
    void OnLayerSpinChanged(int layerIndex);
    void OnPreviewModeChanged(int index);
    void OnReportChanged(int index);
    void OnOpenPackageDirectory();

private:
    void EmitPreviewRequest();
    void RefreshStage16Summary(int layerIndex);

    QLabel* m_validationLabel{nullptr};
    QLabel* m_layerLabel{nullptr};
    QLabel* m_stage16SummaryLabel{nullptr};
    QLabel* m_previewLabel{nullptr};
    QLabel* m_referencePreviewLabel{nullptr};
    QPlainTextEdit* m_summaryView{nullptr};
    QPlainTextEdit* m_reportView{nullptr};
    QSlider* m_layerSlider{nullptr};
    QSpinBox* m_layerSpin{nullptr};
    QComboBox* m_previewModeCombo{nullptr};
    QComboBox* m_reportCombo{nullptr};
    QPushButton* m_openPackageDirectoryButton{nullptr};
    HostChannelChartWidget* m_channelChart{nullptr};
    hostpackagereview m_review;
    QString m_samplingStrategyId;
    QJsonObject m_timing;
};
