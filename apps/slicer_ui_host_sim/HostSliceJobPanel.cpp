#include "HostSliceJobPanel.h"

#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QList>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace
{
QLabel* MakeTimingValue(const QString& objectName, QWidget* parent)
{
    auto* label = new QLabel(QStringLiteral("未提供"), parent);
    label->setObjectName(objectName);
    label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return label;
}

void AddTimingRow(
    QGridLayout* layout,
    const int row,
    const QString& title,
    QLabel* value)
{
    layout->addWidget(new QLabel(title, value->parentWidget()), row, 0);
    layout->addWidget(value, row, 1);
}

QString FormatDuration(const double milliseconds)
{
    if (!std::isfinite(milliseconds) || milliseconds < 0.0)
    {
        return QStringLiteral("未提供");
    }
    if (milliseconds < 1000.0)
    {
        return QStringLiteral("%1 ms").arg(milliseconds, 0, 'f', 1);
    }
    if (milliseconds < 60000.0)
    {
        return QStringLiteral("%1 s").arg(milliseconds / 1000.0, 0, 'f', 2);
    }
    const int totalSeconds = static_cast<int>(milliseconds / 1000.0);
    return QStringLiteral("%1 分 %2 秒")
        .arg(totalSeconds / 60)
        .arg(totalSeconds % 60);
}

QString StateText(const QString& state)
{
    if (state == QStringLiteral("queued"))
    {
        return QStringLiteral("排队中");
    }
    if (state == QStringLiteral("running"))
    {
        return QStringLiteral("处理中");
    }
    if (state == QStringLiteral("cancelling"))
    {
        return QStringLiteral("正在取消");
    }
    return state.isEmpty() ? QStringLiteral("未知") : state;
}

QString PhaseText(const QString& phase)
{
    if (phase == QStringLiteral("queued"))
    {
        return QStringLiteral("等待 Worker 接收作业");
    }
    if (phase == QStringLiteral("scene_config_load"))
    {
        return QStringLiteral("读取并冻结场景配置");
    }
    if (phase == QStringLiteral("scene_model_load"))
    {
        return QStringLiteral("加载场景模型与材料资源");
    }
    if (phase == QStringLiteral("scene_admission"))
    {
        return QStringLiteral("校验排版、碰撞与打印范围");
    }
    if (phase == QStringLiteral("scene_instance_slice"))
    {
        return QStringLiteral("逐模型生成 RGBWSV 图层");
    }
    if (phase == QStringLiteral("scene_composition"))
    {
        return QStringLiteral("合成全场景 RGBWSV 图层");
    }
    if (phase == QStringLiteral("scene_package_write"))
    {
        return QStringLiteral("保存并严格校验 TIFF、预览与报告");
    }
    if (phase == QStringLiteral("scene_package_validation"))
    {
        return QStringLiteral("校验生产包与 TIFF 协议");
    }
    if (phase == QStringLiteral("completed"))
    {
        return QStringLiteral("Worker 核心完成，等待模块终结结果");
    }
    if (phase == QStringLiteral("cancelling"))
    {
        return QStringLiteral("取消作业并清理临时产物");
    }
    return phase.isEmpty() ? QStringLiteral("未知阶段") : phase;
}

QString SamplingStrategyText(const QString& strategyId)
{
    if (strategyId == QStringLiteral(
            "layer_slab_supersample_2x2_at_least_two_candidate"))
    {
        return QStringLiteral("S3 诊断候选｜层体积 2×2（至少 2/4）");
    }
    return QStringLiteral("S0 生产默认｜Legacy 中心采样");
}

void SetTimingValue(
    QLabel* label,
    const QJsonObject& timing,
    const QString& key)
{
    label->setText(FormatDuration(
        timing.value(key).toDouble(-1.0)));
}
}

HostSliceJobPanel::HostSliceJobPanel(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("hostSliceJobPanel"));
    BuildInterface();
    SetReady(false, QStringLiteral("请先选择可切片 Profile 并导入模型。"));
}

void HostSliceJobPanel::BuildInterface()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(8);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName(QStringLiteral("hostSliceJobStatusLabel"));
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    m_phaseLabel = new QLabel(this);
    m_phaseLabel->setObjectName(QStringLiteral("hostSliceJobPhaseLabel"));
    m_phaseLabel->setWordWrap(true);

    m_stage16ContextLabel = new QLabel(
        QStringLiteral(
            "几何采样：S0 生产默认｜Legacy 中心采样\n"
            "姿态：P0 生产默认；P3 仅诊断，未获生产应用授权"),
        this);
    m_stage16ContextLabel->setObjectName(
        QStringLiteral("hostStage16JobContextLabel"));
    m_stage16ContextLabel->setWordWrap(true);
    m_stage16ContextLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setObjectName(QStringLiteral("hostSliceJobProgressBar"));
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);

    auto* commands = new QHBoxLayout();
    m_startButton = new QPushButton(QStringLiteral("开始切片"), this);
    m_startButton->setObjectName(QStringLiteral("hostSliceStartButton"));
    m_startButton->setToolTip(QStringLiteral(
        "提交当前已 Commit 场景与有效 Profile，不会静默切换引擎"));
    m_cancelButton = new QPushButton(QStringLiteral("取消作业"), this);
    m_cancelButton->setObjectName(QStringLiteral("hostSliceCancelButton"));
    m_cancelButton->setToolTip(QStringLiteral(
        "协作式取消 Worker 作业并等待 staging 安全清理"));
    commands->addWidget(m_startButton);
    commands->addWidget(m_cancelButton);

    m_detailView = new QPlainTextEdit(this);
    m_detailView->setObjectName(QStringLiteral("hostSliceJobDetailView"));
    m_detailView->setReadOnly(true);
    m_detailView->setPlaceholderText(QStringLiteral(
        "终结状态、错误码和输出目录将在这里显示。"));

    auto* timingGroup = new QGroupBox(QStringLiteral("切片耗时"), this);
    timingGroup->setToolTip(QStringLiteral(
        "Worker 核心实测与宿主墙钟耗时；部分细分项存在包含关系，不能直接相加。"));
    auto* timingLayout = new QGridLayout(timingGroup);
    timingLayout->setContentsMargins(6, 6, 6, 6);
    timingLayout->setHorizontalSpacing(8);
    timingLayout->setVerticalSpacing(2);
    m_engineValue = MakeTimingValue(
        QStringLiteral("hostSliceTimingEngineValue"), timingGroup);
    m_configLoadValue = MakeTimingValue(
        QStringLiteral("hostSliceTimingConfigLoadValue"), timingGroup);
    m_modelLoadValue = MakeTimingValue(
        QStringLiteral("hostSliceTimingModelLoadValue"), timingGroup);
    m_gridSetupValue = MakeTimingValue(
        QStringLiteral("hostSliceTimingGridSetupValue"), timingGroup);
    m_sliceProcessingValue = MakeTimingValue(
        QStringLiteral("hostSliceTimingProcessingValue"), timingGroup);
    m_layerComputeValue = MakeTimingValue(
        QStringLiteral("hostSliceTimingLayerComputeValue"), timingGroup);
    m_layerComposeValue = MakeTimingValue(
        QStringLiteral("hostSliceTimingLayerComposeValue"), timingGroup);
    m_tiffWriteValue = MakeTimingValue(
        QStringLiteral("hostSliceTimingTiffWriteValue"), timingGroup);
    m_previewWriteValue = MakeTimingValue(
        QStringLiteral("hostSliceTimingPreviewWriteValue"), timingGroup);
    m_reportValue = MakeTimingValue(
        QStringLiteral("hostSliceTimingReportValue"), timingGroup);
    m_outputWriteValue = MakeTimingValue(
        QStringLiteral("hostSliceTimingOutputWriteValue"), timingGroup);
    m_workerTotalValue = MakeTimingValue(
        QStringLiteral("hostSliceTimingWorkerTotalValue"), timingGroup);
    m_hostTotalValue = MakeTimingValue(
        QStringLiteral("hostSliceTimingHostTotalValue"), timingGroup);
    m_supportStatisticsScanValue = MakeTimingValue(
        QStringLiteral("hostSupportStatisticsScanValue"), timingGroup);
    AddTimingRow(timingLayout, 0, QStringLiteral("引擎"), m_engineValue);
    AddTimingRow(timingLayout, 1, QStringLiteral("配置加载"), m_configLoadValue);
    AddTimingRow(timingLayout, 2, QStringLiteral("模型加载"), m_modelLoadValue);
    AddTimingRow(timingLayout, 3, QStringLiteral("网格/场景准入"), m_gridSetupValue);
    AddTimingRow(timingLayout, 4, QStringLiteral("切片处理"), m_sliceProcessingValue);
    AddTimingRow(timingLayout, 5, QStringLiteral("逐层计算"), m_layerComputeValue);
    AddTimingRow(timingLayout, 6, QStringLiteral("场景图层合成"), m_layerComposeValue);
    AddTimingRow(timingLayout, 7, QStringLiteral("TIFF 保存"), m_tiffWriteValue);
    AddTimingRow(timingLayout, 8, QStringLiteral("预览保存"), m_previewWriteValue);
    AddTimingRow(timingLayout, 9, QStringLiteral("报告处理"), m_reportValue);
    AddTimingRow(timingLayout, 10, QStringLiteral("切片保存合计"), m_outputWriteValue);
    AddTimingRow(timingLayout, 11, QStringLiteral("Worker 总耗时"), m_workerTotalValue);
    AddTimingRow(timingLayout, 12, QStringLiteral("宿主总耗时"), m_hostTotalValue);
    AddTimingRow(
        timingLayout,
        13,
        QStringLiteral("支撑统计扫描"),
        m_supportStatisticsScanValue);

    layout->addWidget(m_statusLabel);
    layout->addWidget(m_phaseLabel);
    layout->addWidget(m_stage16ContextLabel);
    layout->addWidget(m_progressBar);
    layout->addLayout(commands);
    layout->addWidget(timingGroup);
    layout->addWidget(m_detailView, 1);

    connect(
        m_startButton,
        &QPushButton::clicked,
        this,
        &HostSliceJobPanel::OnStartRequested);
    connect(
        m_cancelButton,
        &QPushButton::clicked,
        this,
        &HostSliceJobPanel::OnCancelRequested);
}

void HostSliceJobPanel::SetStage16Context(const QString& samplingStrategyId)
{
    m_stage16ContextLabel->setText(
        QStringLiteral(
            "几何采样：%1\n姿态：P0 生产默认；P3 仅诊断，未获生产应用授权")
            .arg(SamplingStrategyText(samplingStrategyId)));
}

void HostSliceJobPanel::SetReady(
    const bool ready,
    const QString& reason)
{
    m_ready = ready;
    if (!m_active && !m_hasCompletion)
    {
        m_statusLabel->setText(
            ready ? QStringLiteral("切片作业已就绪") : reason);
    }
    UpdateButtons();
}

void HostSliceJobPanel::SetActive()
{
    m_active = true;
    m_hasCompletion = false;
    m_progressBar->setValue(0);
    m_statusLabel->setText(QStringLiteral("切片作业已提交"));
    m_phaseLabel->setText(QStringLiteral("排队中"));
    m_detailView->clear();
    m_lastWorkerElapsedMs = 0;
    ResetTiming();
    for (QLabel* value : {
             m_engineValue,
             m_configLoadValue,
             m_modelLoadValue,
             m_gridSetupValue,
             m_sliceProcessingValue,
             m_layerComputeValue,
             m_layerComposeValue,
             m_tiffWriteValue,
             m_previewWriteValue,
             m_reportValue,
             m_outputWriteValue,
             m_supportStatisticsScanValue})
    {
        value->setText(QStringLiteral("终结结果返回后提供"));
    }
    m_workerTotalValue->setText(QStringLiteral("0.0 ms"));
    m_hostTotalValue->setText(QStringLiteral("计时中"));
    UpdateButtons();
}

void HostSliceJobPanel::UpdateProgress(
    const QString& state,
    const QString& phase,
    const int current,
    const int total,
    const int percent,
    const qint64 elapsedMs)
{
    const bool terminal = state == QStringLiteral("succeeded")
        || state == QStringLiteral("failed")
        || state == QStringLiteral("cancelled");
    const int displayPercent = terminal ? percent : std::min(percent, 99);
    m_progressBar->setValue(displayPercent);
    m_statusLabel->setText(QStringLiteral("状态：%1 · %2%")
                               .arg(StateText(state))
                               .arg(displayPercent));
    m_phaseLabel->setText(
        QStringLiteral("阶段：%1 · %2/%3 · 已用 %4")
            .arg(PhaseText(phase))
            .arg(current)
            .arg(total)
            .arg(FormatDuration(static_cast<double>(elapsedMs))));
    m_workerTotalValue->setText(
        FormatDuration(static_cast<double>(elapsedMs)));
    m_lastWorkerElapsedMs = elapsedMs;
}

void HostSliceJobPanel::ShowCompletion(
    const bool success,
    const bool cancelled,
    const QString& code,
    const QString& message,
    const QString& detail,
    const QString& packageDirectory,
    const QJsonObject& timing,
    const qint64 elapsedMs,
    const qint64 cancelLatencyMs)
{
    m_active = false;
    m_hasCompletion = true;
    m_progressBar->setValue(success || cancelled ? 100 : m_progressBar->value());
    m_statusLabel->setText(
        success ? QStringLiteral("切片完成")
                : cancelled ? QStringLiteral("切片已取消")
                            : QStringLiteral("切片失败"));
    m_phaseLabel->setText(
        success ? QStringLiteral("作业已完成，可在结果页检查生产包。")
                : cancelled ? QStringLiteral("作业已取消，临时产物已清理。")
                            : QStringLiteral(
                                  "作业失败，请检查下方错误码与详细信息。"));
    QJsonObject displayTiming = timing;
    if (!displayTiming.contains(QStringLiteral("totalMs")))
    {
        displayTiming.insert(
            QStringLiteral("totalMs"),
            static_cast<double>(m_lastWorkerElapsedMs));
    }
    ApplyTiming(displayTiming, elapsedMs);
    QStringList details{
        QStringLiteral("错误码：%1").arg(
            code.isEmpty() ? QStringLiteral("未返回") : code),
        QStringLiteral("宿主总耗时：%1").arg(
            FormatDuration(static_cast<double>(elapsedMs)))};
    const double workerTotalMs = displayTiming.value(
        QStringLiteral("totalMs")).toDouble(-1.0);
    if (workerTotalMs >= 0.0)
    {
        details.append(QStringLiteral("Worker 总耗时：%1")
                           .arg(FormatDuration(workerTotalMs)));
    }
    if (cancelLatencyMs >= 0)
    {
        details.append(QStringLiteral("取消清理耗时：%1 ms")
                           .arg(cancelLatencyMs));
    }
    if (!packageDirectory.isEmpty())
    {
        details.append(QStringLiteral("生产包目录：%1").arg(packageDirectory));
    }
    if (!message.isEmpty())
    {
        details.append(QStringLiteral("错误说明：%1").arg(message));
    }
    if (!detail.isEmpty())
    {
        details.append(QStringLiteral("详细信息：%1").arg(detail));
    }
    if (!success && !cancelled && message.isEmpty() && detail.isEmpty())
    {
        details.append(QStringLiteral(
            "错误说明：模块未返回更多信息，请复制错误码并检查模块诊断日志。"));
    }
    m_detailView->setPlainText(details.join(QLatin1Char('\n')));
    UpdateButtons();
}

void HostSliceJobPanel::ApplyTiming(
    const QJsonObject& timing,
    const qint64 hostElapsedMs)
{
    ResetTiming();
    const QString engine = timing.value(
        QStringLiteral("engine")).toString();
    m_engineValue->setText(
        timing.value(QStringLiteral("approximate")).toBool()
            ? QStringLiteral("失败前阶段进度估算")
            : engine.isEmpty() ? QStringLiteral("未提供") : engine);
    if (timing.value(QStringLiteral("available")).toBool())
    {
        SetTimingValue(
            m_configLoadValue,
            timing,
            QStringLiteral("configLoadMs"));
        SetTimingValue(m_modelLoadValue, timing, QStringLiteral("modelLoadMs"));
        SetTimingValue(m_gridSetupValue, timing, QStringLiteral("gridSetupMs"));
        SetTimingValue(
            m_sliceProcessingValue,
            timing,
            QStringLiteral("sliceProcessingMs"));
        SetTimingValue(
            m_layerComputeValue,
            timing,
            QStringLiteral("layerComputeMs"));
        SetTimingValue(
            m_layerComposeValue,
            timing,
            QStringLiteral("layerComposeMs"));
        SetTimingValue(m_tiffWriteValue, timing, QStringLiteral("tiffWriteMs"));
        SetTimingValue(
            m_previewWriteValue,
            timing,
            QStringLiteral("previewWriteMs"));
        const double reportBuildMs = timing.value(
            QStringLiteral("reportBuildMs")).toDouble(-1.0);
        const double reportWriteMs = timing.value(
            QStringLiteral("reportWriteMs")).toDouble(-1.0);
        m_reportValue->setText(
            reportBuildMs >= 0.0 && reportWriteMs >= 0.0
                ? FormatDuration(reportBuildMs + reportWriteMs)
                : QStringLiteral("未提供"));
        SetTimingValue(
            m_outputWriteValue,
            timing,
            QStringLiteral("outputWriteMs"));
        const int scanCount = timing.value(
            QStringLiteral("supportStatisticsScanCount")).toInt(-1);
        m_supportStatisticsScanValue->setText(
            scanCount >= 0
                ? QStringLiteral("%1 次（实例累计）").arg(scanCount)
                : QStringLiteral("未提供"));
    }
    SetTimingValue(
        m_workerTotalValue,
        timing,
        QStringLiteral("totalMs"));
    m_hostTotalValue->setText(
        FormatDuration(static_cast<double>(hostElapsedMs)));
}

void HostSliceJobPanel::ResetTiming()
{
    const QList<QLabel*> values{
        m_engineValue,
        m_configLoadValue,
        m_modelLoadValue,
        m_gridSetupValue,
        m_sliceProcessingValue,
        m_layerComputeValue,
        m_layerComposeValue,
        m_tiffWriteValue,
        m_previewWriteValue,
        m_reportValue,
        m_outputWriteValue,
        m_workerTotalValue,
        m_hostTotalValue,
        m_supportStatisticsScanValue};
    for (QLabel* value : values)
    {
        value->setText(QStringLiteral("未提供"));
    }
}

void HostSliceJobPanel::OnStartRequested()
{
    if (m_ready && !m_active)
    {
        emit SigStartRequested();
    }
}

void HostSliceJobPanel::OnCancelRequested()
{
    if (m_active)
    {
        m_cancelButton->setEnabled(false);
        emit SigCancelRequested();
    }
}

void HostSliceJobPanel::UpdateButtons()
{
    m_startButton->setEnabled(m_ready && !m_active);
    m_cancelButton->setEnabled(m_active);
}
