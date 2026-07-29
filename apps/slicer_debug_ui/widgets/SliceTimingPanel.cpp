#include "SliceTimingPanel.h"

#include <QFont>
#include <QGridLayout>
#include <QLabel>
#include <QProgressBar>
#include <QTimer>
#include <QVBoxLayout>

namespace
{

QLabel* MakeValueLabel(QWidget* parent)
{
    auto* label = new QLabel(QStringLiteral("--"), parent);
    label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return label;
}

void AddTimingRow(QGridLayout* layout, const int row, const QString& name, QLabel* value)
{
    auto* nameLabel = new QLabel(name, value->parentWidget());
    layout->addWidget(nameLabel, row, 0);
    layout->addWidget(value, row, 1);
}

}  // namespace

SliceTimingPanel::SliceTimingPanel(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("sliceTimingPanel"));
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 6, 0, 0);
    rootLayout->setSpacing(4);

    auto* titleLabel = new QLabel(QStringLiteral("切片进度与耗时"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    rootLayout->addWidget(titleLabel);

    m_phaseLabel = new QLabel(QStringLiteral("等待切片"), this);
    m_phaseLabel->setWordWrap(true);
    rootLayout->addWidget(m_phaseLabel);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    rootLayout->addWidget(m_progressBar);

    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(500);
    connect(m_refreshTimer, &QTimer::timeout, this, [this]()
    {
        if (m_elapsedTimer.isValid())
        {
            m_progressBar->setFormat(QStringLiteral("%p% · 已用 %1")
                                         .arg(FormatDuration(static_cast<double>(m_elapsedTimer.elapsed()))));
        }
    });

    m_engineLabel = new QLabel(QStringLiteral("引擎：--"), this);
    rootLayout->addWidget(m_engineLabel);

    auto* timingLayout = new QGridLayout();
    timingLayout->setContentsMargins(0, 0, 0, 0);
    timingLayout->setHorizontalSpacing(8);
    timingLayout->setVerticalSpacing(2);
    m_modelLoadValue = MakeValueLabel(this);
    m_gridSetupValue = MakeValueLabel(this);
    m_sliceProcessingValue = MakeValueLabel(this);
    m_layerComputeValue = MakeValueLabel(this);
    m_layerComposeValue = MakeValueLabel(this);
    m_tiffWriteValue = MakeValueLabel(this);
    m_previewWriteValue = MakeValueLabel(this);
    m_reportValue = MakeValueLabel(this);
    m_outputWriteValue = MakeValueLabel(this);
    m_totalValue = MakeValueLabel(this);
    m_peakMemoryValue = MakeValueLabel(this);
    AddTimingRow(timingLayout, 0, QStringLiteral("模型加载"), m_modelLoadValue);
    AddTimingRow(timingLayout, 1, QStringLiteral("网格/场景准入"), m_gridSetupValue);
    AddTimingRow(timingLayout, 2, QStringLiteral("切片处理"), m_sliceProcessingValue);
    AddTimingRow(timingLayout, 3, QStringLiteral("逐层计算"), m_layerComputeValue);
    AddTimingRow(timingLayout, 4, QStringLiteral("场景图层合成"), m_layerComposeValue);
    AddTimingRow(timingLayout, 5, QStringLiteral("TIFF 保存"), m_tiffWriteValue);
    AddTimingRow(timingLayout, 6, QStringLiteral("预览保存"), m_previewWriteValue);
    AddTimingRow(timingLayout, 7, QStringLiteral("报告处理"), m_reportValue);
    AddTimingRow(timingLayout, 8, QStringLiteral("切片保存合计"), m_outputWriteValue);
    AddTimingRow(timingLayout, 9, QStringLiteral("总耗时"), m_totalValue);
    AddTimingRow(timingLayout, 10, QStringLiteral("峰值内存"), m_peakMemoryValue);
    rootLayout->addLayout(timingLayout);

    m_sliceProcessingValue->setToolTip(
        QStringLiteral("网格、几何采样、纹理准备、支撑生成和逐层材料计算耗时；不包含 TIFF、预览和报告写盘。"));
    m_gridSetupValue->setToolTip(
        QStringLiteral("单模型表示切片网格建立耗时；当前场景表示排版、碰撞与打印范围准入耗时。"));
    m_layerComputeValue->setToolTip(
        QStringLiteral("逐模型、逐层生成 RGBWSV 内存图层的耗时。"));
    m_layerComposeValue->setToolTip(
        QStringLiteral("将多个模型的局部图层合成为全场景图层的耗时；单模型旧入口通常接近零。"));
    m_outputWriteValue->setToolTip(
        QStringLiteral("TIFF、预览、报告写盘和输出包发布耗时之和。"));
    m_reportValue->setToolTip(
        QStringLiteral("报告对象生成与 JSON 写盘耗时；其中写盘部分也包含在“切片保存合计”中，各行不能直接全部相加。"));
    m_totalValue->setToolTip(
        QStringLiteral("切片核心返回的整次运行墙钟耗时；若细分数据不可用，则显示界面等待的进程耗时。"));
    m_peakMemoryValue->setToolTip(
        QStringLiteral("本次 slicer_cli 进程报告的实际峰值工作集；平台不支持采集时显示“未提供”。"));
}

void SliceTimingPanel::Reset(const QString& action)
{
    m_hasDetailedTiming = false;
    m_phaseLabel->setText(QStringLiteral("正在启动：") + action);
    m_engineLabel->setText(QStringLiteral("引擎：等待识别"));
    m_progressBar->setValue(0);
    m_progressBar->setFormat(QStringLiteral("%p%"));
    m_elapsedTimer.restart();
    m_refreshTimer->start();
    const QList<QLabel*> values{
        m_modelLoadValue,
        m_gridSetupValue,
        m_sliceProcessingValue,
        m_layerComputeValue,
        m_layerComposeValue,
        m_tiffWriteValue,
        m_previewWriteValue,
        m_reportValue,
        m_outputWriteValue,
        m_totalValue,
        m_peakMemoryValue};
    for (QLabel* value : values)
    {
        value->setText(QStringLiteral("--"));
    }
}

void SliceTimingPanel::UpdateProgress(const SliceProgressEvent& event)
{
    m_phaseLabel->setText(PhaseText(event));
    m_progressBar->setValue(event.percent);
    m_progressBar->setFormat(QStringLiteral("%1% · 已用 %2")
                                 .arg(event.percent)
                                 .arg(FormatDuration(event.elapsedms)));
}

void SliceTimingPanel::ShowTiming(const SliceTimingEvent& event)
{
    m_hasDetailedTiming = true;
    m_engineLabel->setText(QStringLiteral("引擎：") + EngineText(event.engine));
    SetValue(m_modelLoadValue, event.modelloadms);
    SetValue(m_gridSetupValue, event.gridsetupms);
    SetValue(m_sliceProcessingValue, event.sliceprocessingms);
    SetValue(m_layerComputeValue, event.layercomputems);
    SetValue(m_layerComposeValue, event.layercomposems);
    SetValue(m_tiffWriteValue, event.tiffwritems);
    SetValue(m_previewWriteValue, event.previewwritems);
    SetValue(m_reportValue, event.reportbuildms + event.reportwritems);
    SetValue(m_outputWriteValue, event.outputwritems);
    SetValue(m_totalValue, event.totalms);
    m_peakMemoryValue->setText(
        event.memoryavailable
            ? FormatBytes(event.peakworkingsetbytes)
            : QStringLiteral("未提供"));
}

void SliceTimingPanel::Finish(const bool success, const qint64 processelapsedms)
{
    m_refreshTimer->stop();
    m_elapsedTimer.invalidate();
    if (!m_hasDetailedTiming)
    {
        m_totalValue->setText(FormatDuration(static_cast<double>(processelapsedms)) + QStringLiteral("（进程）"));
    }
    if (success)
    {
        m_progressBar->setValue(100);
        m_progressBar->setFormat(QStringLiteral("100%"));
        m_phaseLabel->setText(QStringLiteral("切片处理完成"));
    }
    else
    {
        m_phaseLabel->setText(QStringLiteral("处理失败，详细信息请查看日志"));
    }
}

QString SliceTimingPanel::SummaryText() const
{
    return QStringLiteral(
               "%1 | %2 | 切片=%3 | 保存=%4 | 总计=%5 | 准入=%6 | 合成=%7")
        .arg(m_phaseLabel->text())
        .arg(m_engineLabel->text())
        .arg(m_sliceProcessingValue->text())
        .arg(m_outputWriteValue->text())
        .arg(m_totalValue->text())
        .arg(m_gridSetupValue->text())
        .arg(m_layerComposeValue->text());
}

QString SliceTimingPanel::FormatDuration(const double milliseconds)
{
    if (milliseconds < 1000.0)
    {
        return QStringLiteral("%1 ms").arg(milliseconds, 0, 'f', 1);
    }
    if (milliseconds < 60000.0)
    {
        return QStringLiteral("%1 s").arg(milliseconds / 1000.0, 0, 'f', 2);
    }
    const int totalSeconds = static_cast<int>(milliseconds / 1000.0);
    const int minutes = totalSeconds / 60;
    const int seconds = totalSeconds % 60;
    return QStringLiteral("%1 分 %2 秒").arg(minutes).arg(seconds);
}

QString SliceTimingPanel::FormatBytes(const std::uint64_t bytes)
{
    constexpr double bytesPerMegabyte = 1024.0 * 1024.0;
    constexpr double bytesPerGigabyte = bytesPerMegabyte * 1024.0;
    if (static_cast<double>(bytes) >= bytesPerGigabyte)
    {
        return QStringLiteral("%1 GiB")
            .arg(static_cast<double>(bytes) / bytesPerGigabyte, 0, 'f', 2);
    }
    return QStringLiteral("%1 MiB")
        .arg(static_cast<double>(bytes) / bytesPerMegabyte, 0, 'f', 1);
}

QString SliceTimingPanel::PhaseText(const SliceProgressEvent& event)
{
    if (event.phase == QStringLiteral("config_load"))
    {
        return QStringLiteral("正在读取切片配置");
    }
    if (event.phase == QStringLiteral("model_load"))
    {
        return QStringLiteral("正在加载模型与材料");
    }
    if (event.phase == QStringLiteral("grid_setup"))
    {
        return QStringLiteral("正在建立切片网格");
    }
    if (event.phase == QStringLiteral("mask_sampling"))
    {
        return QStringLiteral("正在进行几何采样");
    }
    if (event.phase == QStringLiteral("texture_prepare"))
    {
        return QStringLiteral("正在准备纹理数据");
    }
    if (event.phase == QStringLiteral("support_generation"))
    {
        return QStringLiteral("正在生成支撑数据");
    }
    if (event.phase == QStringLiteral("openvdb_prepare"))
    {
        return QStringLiteral("正在执行 OpenVDB 几何准备");
    }
    if (event.phase == QStringLiteral("layer_buffer_prepare"))
    {
        return QStringLiteral("正在建立 OpenVDB 图层缓冲");
    }
    if (event.phase == QStringLiteral("layer_processing"))
    {
        if (event.current <= 0)
        {
            return QStringLiteral("准备逐层计算并保存，共 %1 层").arg(event.total);
        }
        return QStringLiteral("正在处理并保存图层 %1 / %2").arg(event.current).arg(event.total);
    }
    if (event.phase == QStringLiteral("report_build"))
    {
        return QStringLiteral("正在生成切片报告");
    }
    if (event.phase == QStringLiteral("report_write"))
    {
        return QStringLiteral("正在保存切片报告");
    }
    if (event.phase == QStringLiteral("package_publish"))
    {
        return QStringLiteral("正在发布输出包");
    }
    if (event.phase == QStringLiteral("scene_config_load"))
    {
        return QStringLiteral("正在冻结并校验当前场景配置");
    }
    if (event.phase == QStringLiteral("scene_model_load"))
    {
        return QStringLiteral("正在加载场景模型与资源");
    }
    if (event.phase == QStringLiteral("scene_admission"))
    {
        return QStringLiteral("正在校验排版、碰撞与打印范围");
    }
    if (event.phase == QStringLiteral("scene_instance_slice"))
    {
        if (event.current <= 0)
        {
            return QStringLiteral("准备切片 %1 个场景模型")
                .arg(event.total);
        }
        return QStringLiteral("正在切片场景模型 %1 / %2")
            .arg(event.current)
            .arg(event.total);
    }
    if (event.phase == QStringLiteral("scene_composition"))
    {
        return QStringLiteral("正在合成全场景 RGBWSV 图层");
    }
    if (event.phase == QStringLiteral("scene_package_write"))
    {
        if (event.current <= 0)
        {
            return QStringLiteral("准备保存 %1 个场景图层")
                .arg(event.total);
        }
        return QStringLiteral("正在保存场景图层 %1 / %2")
            .arg(event.current)
            .arg(event.total);
    }
    if (event.phase
        == QStringLiteral("scene_package_validation"))
    {
        return QStringLiteral("正在校验场景输出包与 TIFF 协议");
    }
    if (event.phase == QStringLiteral("completed"))
    {
        return QStringLiteral("切片核心处理完成");
    }
    return QStringLiteral("正在处理：") + event.phase;
}

QString SliceTimingPanel::EngineText(const QString& engine)
{
    if (engine == QStringLiteral("legacy"))
    {
        return QStringLiteral("传统切片引擎");
    }
    if (engine == QStringLiteral("openvdb-candidate"))
    {
        return QStringLiteral("OpenVDB 候选引擎");
    }
    if (engine == QStringLiteral("legacy-scene"))
    {
        return QStringLiteral("传统场景切片引擎");
    }
    return engine;
}

void SliceTimingPanel::SetValue(QLabel* label, const double milliseconds)
{
    label->setText(FormatDuration(milliseconds));
}
