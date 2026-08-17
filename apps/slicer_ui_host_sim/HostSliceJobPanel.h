#pragma once

#include <QJsonObject>
#include <QWidget>

class QLabel;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;

/** @brief 针对一项参考宿主切片作业的操作员控制和诊断。 */
class HostSliceJobPanel final : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief 创建一个空闲切片作业面板。
     * @param parent 可选的 Qt 父控件。
     */
    explicit HostSliceJobPanel(QWidget* parent = nullptr);

    /**
     * @brief 更新已验证场景和有效 Profile 的就绪状态。
     * @param ready 当可以启用启动命令时为 true。
     * @param reason 面向用户的准备情况说明。
     */
    void SetReady(bool ready, const QString& reason);

    /**
     * @brief 显示 Stage 16 策略和冻结姿态状态。
     * @param samplingStrategyId 有效 Profile 的几何策略标识。
     */
    void SetStage16Context(const QString& samplingStrategyId);

    /** @brief 模块接受作业后将面板标记为活动。 */
    void SetActive();

    /**
     * @brief 更新单调递增的公共作业进度。
     * @param state 冻结的小写生命周期状态。
     * @param phase Worker 进度阶段。
     * @param current 已完成工作单元数。
     * @param total 工作单元总数。
     * @param percent [0, 100] 中的单调百分比。
     * @param elapsedMs 从宿主提交成功起连续递增的墙钟时间，单位为毫秒。
     */
    void UpdateProgress(
        const QString& state,
        const QString& phase,
        int current,
        int total,
        int percent,
        qint64 elapsedMs);

    /**
     * @brief 显示由 pm_poll 阶段边界推导的非权威实时耗时。
     * @param timing 带 approximate=true 的宿主轮询估算。
     */
    void UpdateLiveTiming(const QJsonObject& timing);

    /**
     * @brief 显示一个最终结果并将控件返回到空闲状态。
     * @param success 已发布生产包有效时为 true。
     * @param cancelled 发生协作式取消时为 true。
     * @param code 稳定的结果/错误代码。
     * @param message 面向用户的细节。
     * @param detail 附加 Worker 或预检诊断。
     * @param packageDirectory 已发布的包目录（如果有）。
     * @param timing Worker 核心阶段耗时 telemetry（如果有）。
     * @param elapsedMs 宿主观测的总作业时间。
     * @param cancelLatencyMs 宿主观测的取消延迟，未测量时为 -1。
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
    /** @brief 请求提交当前提交的场景。 */
    void SigStartRequested();

    /** @brief 请求协作式取消活动作业。 */
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
    QLabel* m_stage16ContextLabel{nullptr};
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
    QLabel* m_supportStatisticsScanValue{nullptr};
    bool m_ready{false};
    bool m_active{false};
    bool m_hasCompletion{false};
    bool m_hasWorkerElapsed{false};
    qint64 m_lastWorkerElapsedMs{0};
};
