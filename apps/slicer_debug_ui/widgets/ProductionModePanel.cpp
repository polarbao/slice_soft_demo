#include "ProductionModePanel.h"

#include <QComboBox>
#include <QFontMetrics>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace
{

QString ModeValue(const slicer_core::SlicePipelineMode mode)
{
    const ProductionModeCapability* capability =
        ProductionModeCatalog::FindMode(mode);
    return capability == nullptr
        ? QString{}
        : QString::fromStdString(capability->stablevalue);
}

QString AdmissionText(const ProductionAdmissionState state)
{
    switch (state)
    {
    case ProductionAdmissionState::Stale:
        return QStringLiteral("配置已变化，准入结果已过期");
    case ProductionAdmissionState::Running:
        return QStringLiteral("正在执行模型预检与生产准入");
    case ProductionAdmissionState::Blocked:
        return QStringLiteral("已阻断");
    case ProductionAdmissionState::Admitted:
        return QStringLiteral("已准入");
    case ProductionAdmissionState::Pending:
    default:
        return QStringLiteral("待预检");
    }
}

QString DisplayMode(const slicer_core::SlicePipelineMode mode)
{
    const ProductionModeCapability* capability =
        ProductionModeCatalog::FindMode(mode);
    return capability == nullptr
        ? QStringLiteral("未知")
        : QString::fromStdString(capability->displaynamezh);
}

QString FormatDuration(const double milliseconds)
{
    if (milliseconds < 1000.0)
    {
        return QStringLiteral("%1 ms").arg(milliseconds, 0, 'f', 1);
    }
    return QStringLiteral("%1 s").arg(milliseconds / 1000.0, 0, 'f', 2);
}

QString FormatMemory(const std::uint64_t bytes)
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

}  // namespace

ProductionModePanel::ProductionModePanel(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("productionModePanel"));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* group = new QGroupBox(QStringLiteral("生产切片模式"), this);
    group->setObjectName(QStringLiteral("productionModeGroup"));
    group->setToolTip(
        QStringLiteral(
            "产品模式只包含传统切片和全局纹理壳层。OpenVDB 是内部实现依赖或独立诊断工具，不是第三种产品模式。"));
    auto* form = new QFormLayout(group);

    m_modeCombo = new QComboBox(group);
    m_modeCombo->setObjectName(QStringLiteral("productionModeCombo"));
    m_modeCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_modeCombo->setMinimumContentsLength(16);
    for (const ProductionModeCapability& capability : ProductionModeCatalog::Modes())
    {
        m_modeCombo->addItem(
            QString::fromStdString(capability.displaynamezh),
            QString::fromStdString(capability.stablevalue));
    }
    const QString defaultMode = ModeValue(ProductionModeCatalog::DefaultMode().mode);
    m_modeCombo->setCurrentIndex(m_modeCombo->findData(defaultMode));
    m_modeCombo->setToolTip(
        QStringLiteral(
            "默认使用传统切片。全局纹理壳层必须由用户显式选择，并通过当前模型的生产准入。"));
    form->addRow(QStringLiteral("切片模式"), m_modeCombo);

    m_profileCombo = new QComboBox(group);
    m_profileCombo->setObjectName(QStringLiteral("productionProfileCombo"));
    m_profileCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_profileCombo->setMinimumContentsLength(24);
    m_profileCombo->setToolTip(
        QStringLiteral(
            "全局纹理壳层只开放已完成生产准入的 Profile；Profile 会锁定材料、支撑和光油能力。"));
    form->addRow(QStringLiteral("全局 Profile"), m_profileCombo);

    m_capabilityLabel = new QLabel(group);
    m_capabilityLabel->setObjectName(QStringLiteral("productionCapabilityLabel"));
    m_capabilityLabel->setWordWrap(true);
    form->addRow(QStringLiteral("能力范围"), m_capabilityLabel);

    m_admissionLabel = new QLabel(group);
    m_admissionLabel->setObjectName(QStringLiteral("productionAdmissionLabel"));
    m_admissionLabel->setWordWrap(true);
    form->addRow(QStringLiteral("准入状态"), m_admissionLabel);

    m_blockingLabel = new QLabel(group);
    m_blockingLabel->setObjectName(QStringLiteral("productionBlockingLabel"));
    m_blockingLabel->setWordWrap(true);
    form->addRow(QStringLiteral("阻断信息"), m_blockingLabel);

    m_resourceLabel = new QLabel(group);
    m_resourceLabel->setObjectName(QStringLiteral("productionResourceLabel"));
    m_resourceLabel->setWordWrap(true);
    form->addRow(QStringLiteral("资源提示"), m_resourceLabel);

    m_resultIdentityLabel = new QLabel(group);
    m_resultIdentityLabel->setObjectName(
        QStringLiteral("productionResultIdentityLabel"));
    m_resultIdentityLabel->setWordWrap(true);
    form->addRow(QStringLiteral("本次模式"), m_resultIdentityLabel);

    m_resultOutputLabel = new QLabel(group);
    m_resultOutputLabel->setObjectName(
        QStringLiteral("productionResultOutputLabel"));
    m_resultOutputLabel->setWordWrap(true);
    form->addRow(QStringLiteral("生产结果"), m_resultOutputLabel);

    m_resultResourceLabel = new QLabel(group);
    m_resultResourceLabel->setObjectName(
        QStringLiteral("productionResultResourceLabel"));
    m_resultResourceLabel->setWordWrap(true);
    form->addRow(QStringLiteral("本次资源"), m_resultResourceLabel);

    layout->addWidget(group);

    connect(
        m_modeCombo,
        qOverload<int>(&QComboBox::currentIndexChanged),
        this,
        &ProductionModePanel::OnModeChanged);
    connect(
        m_profileCombo,
        qOverload<int>(&QComboBox::currentIndexChanged),
        this,
        &ProductionModePanel::OnProfileChanged);

    RefreshProfileItems();
    RefreshPresentation();
}

slicer_core::SlicePipelineMode ProductionModePanel::SelectedMode() const
{
    const QString stableValue = m_modeCombo->currentData().toString();
    for (const ProductionModeCapability& capability : ProductionModeCatalog::Modes())
    {
        if (QString::fromStdString(capability.stablevalue) == stableValue)
        {
            return capability.mode;
        }
    }
    return ProductionModeCatalog::DefaultMode().mode;
}

QString ProductionModePanel::SelectedProfileId() const
{
    if (SelectedMode() == slicer_core::SlicePipelineMode::Legacy)
    {
        return {};
    }
    return m_profileCombo->currentData().toString();
}

void ProductionModePanel::MarkAdmissionStale(const QString& reason)
{
    ShowAdmissionState(ProductionAdmissionState::Stale, reason);
}

void ProductionModePanel::ShowAdmissionState(
    const ProductionAdmissionState state,
    const QString& detail)
{
    m_admissionState = state;
    m_admissionDetail = detail;
    RefreshPresentation();
}

void ProductionModePanel::ShowProductionResult(
    const ProductionModeUiDto& result)
{
    m_result = result;
    RefreshPresentation();
}

void ProductionModePanel::ClearProductionResult()
{
    m_result.reset();
    RefreshPresentation();
}

void ProductionModePanel::OnModeChanged(const int index)
{
    Q_UNUSED(index);
    m_admissionState = ProductionAdmissionState::Stale;
    m_admissionDetail = QStringLiteral("切片模式已改变，需要重新执行模型预检与生产准入。");
    RefreshProfileItems();
    RefreshPresentation();
    emit SigSelectionChanged();
}

void ProductionModePanel::OnProfileChanged(const int index)
{
    Q_UNUSED(index);
    if (SelectedMode() == slicer_core::SlicePipelineMode::Legacy)
    {
        return;
    }
    m_admissionState = ProductionAdmissionState::Stale;
    m_admissionDetail = QStringLiteral("全局 Production Profile 已改变，需要重新执行生产准入。");
    RefreshPresentation();
    emit SigSelectionChanged();
}

void ProductionModePanel::RefreshProfileItems()
{
    const QSignalBlocker blocker(m_profileCombo);
    m_profileCombo->clear();
    if (SelectedMode() == slicer_core::SlicePipelineMode::Legacy)
    {
        m_profileCombo->addItem(QStringLiteral("沿用当前传统切片 Profile"), QString{});
        m_profileCombo->setEnabled(false);
        return;
    }

    for (const ProductionProfileCapability& capability : ProductionModeCatalog::Profiles())
    {
        m_profileCombo->addItem(
            QString::fromStdString(capability.displaynamezh),
            QString::fromStdString(capability.profileid));
    }
    m_profileCombo->setEnabled(m_profileCombo->count() > 0);
    if (m_profileCombo->count() > 0)
    {
        m_profileCombo->setCurrentIndex(0);
    }
}

void ProductionModePanel::RefreshPresentation()
{
    const slicer_core::SlicePipelineMode mode = SelectedMode();
    const bool isLegacy = mode == slicer_core::SlicePipelineMode::Legacy;
    const ProductionProfileCapability* profile = isLegacy
        ? nullptr
        : ProductionModeCatalog::FindProfile(SelectedProfileId().toStdString());

    if (isLegacy)
    {
        m_capabilityLabel->setText(
            QStringLiteral("沿用当前配置中的 RGB、白墨、支撑和光油能力。"));
        m_resourceLabel->setText(
            QStringLiteral("常规资源开销；实际耗时和峰值内存以本次运行结果为准。"));
        m_blockingLabel->setText(QStringLiteral("无；运行前仍会执行当前模型预检。"));
    }
    else if (profile == nullptr)
    {
        m_capabilityLabel->setText(QStringLiteral("未选择有效的全局 Production Profile。"));
        m_resourceLabel->setText(
            QStringLiteral("全局纹理壳层资源开销较高；尚未选择可执行 Profile。"));
        m_blockingLabel->setText(QStringLiteral("请选择已准入的全局 Production Profile。"));
    }
    else
    {
        const bool materialParity =
            profile->supportscope == ProductionSupportScope::LowerAndInternalVoid;
        m_capabilityLabel->setText(
            materialParity
                ? QStringLiteral(
                      "RGB + 白墨；仅下表面/内部镂空支撑；仅表面/外侧光油。Profile 锁定，不支持上表面、双面或完整垂直投影支撑。")
                : QStringLiteral(
                      "RGB + 白墨；不生成支撑和光油。相关配置控件由 Profile 锁定。"));
        m_resourceLabel->setText(
            QStringLiteral(
                "全局纹理壳层属于高资源开销模式；实际耗时和峰值内存以本次运行结果为准，不使用固定倍数估算。"));
        m_blockingLabel->setText(
            m_admissionState == ProductionAdmissionState::Blocked
                ? m_admissionDetail
                : QStringLiteral("无固定阻断；当前模型仍需通过严格拓扑与生产准入。"));
    }

    m_admissionLabel->setText(
        m_admissionDetail.trimmed().isEmpty()
            ? AdmissionText(m_admissionState)
            : AdmissionText(m_admissionState) + QStringLiteral("：") + m_admissionDetail);

    if (!m_result.has_value())
    {
        m_resultIdentityLabel->setText(QStringLiteral("尚无当前 session 生产结果"));
        m_resultOutputLabel->setText(QStringLiteral("尚未校验"));
        m_resultResourceLabel->setText(QStringLiteral("尚未测量"));
        return;
    }

    const ProductionModeUiDto& result = *m_result;
    const QString requested = DisplayMode(result.requestedmode);
    const QString effective = result.effectivemode.has_value()
        ? DisplayMode(*result.effectivemode)
        : QStringLiteral("未确认");
    m_resultIdentityLabel->setText(
        QStringLiteral("requested=%1；effective=%2；session=%3")
            .arg(
                requested,
                effective,
                QString::fromStdString(result.sessionid)));
    m_resultOutputLabel->setText(
        QStringLiteral("TIFF=%1；fallback=%2；包=%3")
            .arg(
                result.productionoutputwritten
                    ? QStringLiteral("已写入")
                    : QStringLiteral("未写入"),
                result.fallbackapplied
                    ? QStringLiteral("是")
                    : QStringLiteral("否"),
                QString::fromStdString(result.packagepath)));

    const QString total = result.measuredtotalms.has_value()
        ? FormatDuration(*result.measuredtotalms)
        : QStringLiteral("未提供");
    const QString peakMemory = result.measuredpeakworkingsetbytes.has_value()
        ? FormatMemory(*result.measuredpeakworkingsetbytes)
        : QStringLiteral("未提供");
    QString resourceText =
        QStringLiteral("总耗时=%1；峰值内存=%2").arg(total, peakMemory);
    if (result.resourcecost == ProductionResourceCostLevel::High)
    {
        resourceText +=
            QStringLiteral("；全局纹理壳层为高开销模式，请以本次实测值判断。");
    }
    m_resultResourceLabel->setText(resourceText);
}
