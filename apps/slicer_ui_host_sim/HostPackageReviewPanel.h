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

/** @brief 参考宿主中用于已验证 RGBWSV Package 的结果工作区。 */
class HostPackageReviewPanel final : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief 创建一个空的包审阅工作区。
     * @param parent 可选的 Qt 父控件。
     */
    explicit HostPackageReviewPanel(QWidget* parent = nullptr);

    /**
     * @brief 显示经过验证的包摘要和通道图表。
     * @param review 由宿主持有的生产包数据。
     * @return 该函数不返回值。
     */
    void SetPackage(const hostpackagereview& review);

    /**
     * @brief 保存 Stage 16 策略与 Worker 计时，用于结果诊断。
     * @param samplingStrategyId 有效 Profile 的几何策略标识。
     * @param timing 已完成作业返回的 Worker 计时对象。
     */
    void SetStage16Context(
        const QString& samplingStrategyId,
        const QJsonObject& timing);

    /**
     * @brief 显示模块渲染的生产层预览。
     * @param imagePath 模块返回的宿主缓存路径。
     * @param layer 与图像对应的层描述符。
     * @return 该函数不返回值。
     */
    void ShowPreview(
        const QString& imagePath,
        const hostlayerdescriptor& layer);

    /**
     * @brief 显示一份结构化包报告。
     * @param report 通过 package.read_report 返回的报告。
     * @return 该函数不返回值。
     */
    void ShowReport(const hostpackagereport& report);

    /**
     * @brief 显示非破坏性结果查看错误。
     * @param message 用户可读的错误文本。
     * @return 该函数不返回值。
     */
    void ShowError(const QString& message);

    /**
     * @brief 返回当前请求的预览通道。
     * @return 一个或多个 RGBWSV 通道名称。
     */
    [[nodiscard]] QStringList SelectedChannels() const;

signals:
    /** @brief 请求新的生产 TIFF 预览。 */
    void SigLayerPreviewRequested(int layerIndex, QStringList channels);

    /** @brief 请求一份在 manifest 中登记的命名报告。 */
    void SigReportRequested(QString reportName);

    /** @brief 请求打开已精确验证的生产 Package 目录。 */
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
