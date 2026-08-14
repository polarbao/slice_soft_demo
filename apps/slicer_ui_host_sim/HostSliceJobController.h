#pragma once

#include "HostSliceSettings.h"
#include "ModuleClient.h"

#include <QElapsedTimer>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QTimer>

/** @brief 一项公共切片作业的宿主可见生命周期状态。 */
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

/** @brief pm_poll 返回的最新单调进度快照。 */
struct hostslicejobprogress
{
    HostSliceJobState state{HostSliceJobState::Idle};
    QString phase;
    int current{0};
    int total{0};
    int percent{0};
    qint64 elapsedms{0};
};

/** @brief 释放作业句柄后，最终结果由宿主持有。 */
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
 * @brief 通过冻结的公共 SPI 拥有一项异步 slice.rgbwsv 作业。
 *
 * 控制器使用 Qt 计时器进行轮询，并且不会在事件循环发生时阻塞事件循环。
 * Worker 作业活动期间由该控制器独占句柄，并保证每个公共作业句柄只释放一次。
 */
class HostSliceJobController final : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 创建一个绑定到一个已加载模块客户端的空闲控制器。
     * @param client 拥有模块会话的公共 ABI 客户端。
     * @param parent 可选的 Qt 对象父对象。
     */
    explicit HostSliceJobController(
        ModuleClient& client,
        QObject* parent = nullptr);

    /** @brief 在销毁之前取消并释放活动作业。 */
    ~HostSliceJobController() override;

    /**
     * @brief 提交一个已 Commit 场景与有效切片 Profile。
     * @param sceneHandle 模块拥有的提交场景句柄。
     * @param effectiveProfile H-B-05 验证了Profile和自哈希。
     * @param error 接收失败即拒绝的验证或传输原因。
     * @return Worker 接受作业时返回 true。
     */
    bool Start(
        quint64 sceneHandle,
        const hosteffectiveprofile& effectiveProfile,
        QString* error);

    /**
     * @brief 请求合作取消活动作业。
     * @param error 接收公共 ABI 的拒绝原因。
     * @return 当取消已被接受或已待处理时为 true。
     */
    bool Cancel(QString* error);

    /** @brief 返回是否保留非终端公共作业。 */
    [[nodiscard]] bool IsActive() const;

    /** @brief 返回最新的宿主生命周期状态。 */
    [[nodiscard]] HostSliceJobState State() const;

    /** @brief 返回最新的进度快照。 */
    [[nodiscard]] hostslicejobprogress Progress() const;

    /** @brief 返回最后一个终端完成情况（如果有）。 */
    [[nodiscard]] hostslicejobcompletion Completion() const;

    /**
     * @brief 将作业状态转换为冻结的小写 SPI 拼写。
     * @param state 宿主作业状态。
     * @return 用于诊断和测试的稳定状态标识符。
     */
    static QString StateId(HostSliceJobState state);

signals:
    /** @brief 发布一份经过验证的单调进度快照。 */
    void SigProgressChanged(
        QString state,
        QString phase,
        int current,
        int total,
        int percent,
        qint64 elapsedMs);

    /** @brief 释放作业句柄后发布最终结果。 */
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
    void RecordObservedPhase(const QString& phase, qint64 elapsedMs);
    QJsonObject FinalizeObservedTiming(qint64 elapsedMs);
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
    QJsonObject m_observedTiming;
    QString m_observedPhase;
    qint64 m_observedPhaseStartMs{0};
    bool m_cancelRequested{false};
};
