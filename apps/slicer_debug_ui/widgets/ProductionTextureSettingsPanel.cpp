#include "ProductionTextureSettingsPanel.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace
{

QLabel* MakeWrappedLabel(
    const QString& objectName,
    QWidget* parent)
{
    auto* label = new QLabel(parent);
    label->setObjectName(objectName);
    label->setWordWrap(true);
    return label;
}

}  // namespace

ProductionTextureSettingsPanel::ProductionTextureSettingsPanel(
    QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("productionTextureSettingsPanel"));
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 6, 0, 0);

    auto* notice = MakeWrappedLabel(
        QStringLiteral("productionSettingsNoticeLabel"),
        this);
    notice->setText(
        QStringLiteral(
            "生产设置：修改后会写入当前场景草稿，并使已有输出失效；"
            "诊断宽度位于“预检与诊断”，不会修改生产输出。"));
    notice->setToolTip(
        QStringLiteral(
            "Legacy 使用 Z 层数；Global 使用三维物理宽度；"
            "两者不会互相换算或覆盖。"));
    rootLayout->addWidget(notice);

    m_strategyLabel = MakeWrappedLabel(
        QStringLiteral("productionTextureStrategyLabel"),
        this);
    rootLayout->addWidget(m_strategyLabel);

    m_pages = new QStackedWidget(this);
    m_pages->setObjectName(
        QStringLiteral("productionTextureSettingsPages"));

    m_unsupportedPage = new QWidget(m_pages);
    auto* unsupportedLayout = new QVBoxLayout(m_unsupportedPage);
    auto* unsupported = MakeWrappedLabel(
        QStringLiteral("productionTextureUnsupportedLabel"),
        m_unsupportedPage);
    unsupported->setText(
        QStringLiteral(
            "当前 Profile 没有可由此面板编辑的生产纹理或单材料设置。"));
    unsupportedLayout->addWidget(unsupported);
    unsupportedLayout->addStretch(1);
    m_pages->addWidget(m_unsupportedPage);

    m_legacyPage = new QWidget(m_pages);
    auto* legacyForm = new QFormLayout(m_legacyPage);
    m_legacyTopLayers = new QSpinBox(m_legacyPage);
    m_legacyTopLayers->setObjectName(
        QStringLiteral("productionLegacyTopLayersSpin"));
    m_legacyTopLayers->setRange(1, 10000);
    m_legacyTopLayers->setSingleStep(1);
    m_legacyTopLayers->setToolTip(
        QStringLiteral(
            "沿切片 Z 方向写入顶面纹理的层数；不是三维法向壳层宽度。"));
    m_legacyEffectiveThickness = MakeWrappedLabel(
        QStringLiteral("productionLegacyEffectiveThicknessLabel"),
        m_legacyPage);
    legacyForm->addRow(
        QStringLiteral("顶面纹理层数"),
        m_legacyTopLayers);
    legacyForm->addRow(
        QStringLiteral("有效 Z 厚度"),
        m_legacyEffectiveThickness);
    m_pages->addWidget(m_legacyPage);

    m_globalPage = new QWidget(m_pages);
    auto* globalForm = new QFormLayout(m_globalPage);
    m_globalMode = new QComboBox(m_globalPage);
    m_globalMode->setObjectName(
        QStringLiteral("productionGlobalTextureModeCombo"));
    m_globalMode->addItem(
        QStringLiteral("有限宽度"),
        static_cast<int>(
            ProductionTexturePartitionMode::PartialShell));
    m_globalMode->addItem(
        QStringLiteral("全纹理"),
        static_cast<int>(
            ProductionTexturePartitionMode::AllTexture));
    m_globalMode->setToolTip(
        QStringLiteral(
            "有限宽度按三维表面距离分区；全纹理显式取消模型填充区。"));
    m_globalWidth = new QDoubleSpinBox(m_globalPage);
    m_globalWidth->setObjectName(
        QStringLiteral("productionGlobalTextureWidthSpin"));
    m_globalWidth->setRange(0.10, 100.00);
    m_globalWidth->setDecimals(2);
    m_globalWidth->setSingleStep(0.01);
    m_globalWidth->setSuffix(QStringLiteral(" mm"));
    m_globalWidth->setToolTip(
        QStringLiteral(
            "Global Surface Shell 的三维法向物理宽度，按 0.01 mm 量化。"));
    m_globalEffectiveWidth = MakeWrappedLabel(
        QStringLiteral("productionGlobalEffectiveWidthLabel"),
        m_globalPage);
    m_globalBackend = MakeWrappedLabel(
        QStringLiteral("productionGlobalBackendLabel"),
        m_globalPage);
    globalForm->addRow(QStringLiteral("纹理模式"), m_globalMode);
    globalForm->addRow(QStringLiteral("请求宽度"), m_globalWidth);
    globalForm->addRow(
        QStringLiteral("有效宽度"),
        m_globalEffectiveWidth);
    globalForm->addRow(QStringLiteral("生产后端"), m_globalBackend);
    m_pages->addWidget(m_globalPage);

    m_singleMaterialPage = new QWidget(m_pages);
    auto* materialForm = new QFormLayout(m_singleMaterialPage);
    m_singleMaterial = new QComboBox(m_singleMaterialPage);
    m_singleMaterial->setObjectName(
        QStringLiteral("productionSingleMaterialCombo"));
    m_singleMaterial->addItem(
        QStringLiteral("白墨"),
        static_cast<int>(SingleMaterialReliefMaterial::White));
    m_singleMaterial->addItem(
        QStringLiteral("光油"),
        static_cast<int>(SingleMaterialReliefMaterial::Varnish));
    m_singleMaterial->setToolTip(
        QStringLiteral(
            "切换时会原子更新 modelMaterial、modelFill、"
            "materialProcessProfile、校验和预览字段。"));
    m_singleEffectiveChannel = MakeWrappedLabel(
        QStringLiteral("productionSingleMaterialChannelLabel"),
        m_singleMaterialPage);
    auto* supportChannel = MakeWrappedLabel(
        QStringLiteral("productionSingleMaterialSupportLabel"),
        m_singleMaterialPage);
    supportChannel->setText(QStringLiteral("S（只读）"));
    materialForm->addRow(QStringLiteral("模型材料"), m_singleMaterial);
    materialForm->addRow(
        QStringLiteral("有效通道"),
        m_singleEffectiveChannel);
    materialForm->addRow(QStringLiteral("支撑通道"), supportChannel);
    m_pages->addWidget(m_singleMaterialPage);

    rootLayout->addWidget(m_pages);
    m_stateLabel = MakeWrappedLabel(
        QStringLiteral("productionSettingsStateLabel"),
        this);
    m_lockLabel = MakeWrappedLabel(
        QStringLiteral("productionSettingsLockLabel"),
        this);
    rootLayout->addWidget(m_stateLabel);
    rootLayout->addWidget(m_lockLabel);
    rootLayout->addStretch(1);

    connect(
        m_legacyTopLayers,
        qOverload<int>(&QSpinBox::valueChanged),
        this,
        [this](const int layerCount)
        {
            if (!m_updating)
            {
                emit SigLegacyTopLayersChanged(layerCount);
            }
        });
    connect(
        m_globalWidth,
        qOverload<double>(&QDoubleSpinBox::valueChanged),
        this,
        [this](const double)
        {
            OnGlobalControlChanged();
        });
    connect(
        m_globalMode,
        qOverload<int>(&QComboBox::currentIndexChanged),
        this,
        [this](const int)
        {
            OnGlobalControlChanged();
        });
    connect(
        m_singleMaterial,
        qOverload<int>(&QComboBox::currentIndexChanged),
        this,
        [this](const int index)
        {
            if (m_updating || index < 0)
            {
                return;
            }
            emit SigSingleMaterialChanged(
                static_cast<SingleMaterialReliefMaterial>(
                    m_singleMaterial->itemData(index).toInt()));
        });

    ProductionTextureSettingsPresentation presentation;
    SetPresentation(presentation);
}

void ProductionTextureSettingsPanel::SetPresentation(
    const ProductionTextureSettingsPresentation& presentation)
{
    m_updating = true;
    const QSignalBlocker legacyBlocker(m_legacyTopLayers);
    const QSignalBlocker globalModeBlocker(m_globalMode);
    const QSignalBlocker globalWidthBlocker(m_globalWidth);
    const QSignalBlocker materialBlocker(m_singleMaterial);

    if (presentation.singlematerialrelief.has_value())
    {
        const SingleMaterialReliefState& state =
            *presentation.singlematerialrelief;
        m_pages->setCurrentWidget(m_singleMaterialPage);
        m_strategyLabel->setText(
            QStringLiteral("生产策略：单材料浮雕 W/V"));
        const int materialIndex = m_singleMaterial->findData(
            static_cast<int>(state.requestedmaterial));
        if (materialIndex >= 0)
        {
            m_singleMaterial->setCurrentIndex(materialIndex);
        }
        m_singleMaterial->setEnabled(state.editable && state.valid);
        m_singleEffectiveChannel->setText(
            state.effectivechannel.isEmpty()
                ? QStringLiteral("未解析")
                : state.effectivechannel);
        UpdateStateLabels(
            state.stale,
            state.editable,
            state.valid,
            state.lockreason,
            state.issues);
        m_updating = false;
        return;
    }

    const ProductionTextureControlState& state = presentation.texture;
    if (state.strategy == ProductionTextureStrategy::LegacyTopBand)
    {
        m_pages->setCurrentWidget(m_legacyPage);
        m_strategyLabel->setText(
            QStringLiteral("生产策略：Legacy 顶面 Z 层带"));
        m_legacyTopLayers->setValue(
            std::max(1, state.requestedtoplayers));
        m_legacyTopLayers->setEnabled(state.editable && state.valid);
        m_legacyEffectiveThickness->setText(
            QStringLiteral("%1 mm（%2 层）")
                .arg(state.effectivetopthicknessmm, 0, 'f', 4)
                .arg(state.effectivetoplayers));
    }
    else if (state.strategy
             == ProductionTextureStrategy::GlobalSurfaceShell)
    {
        m_pages->setCurrentWidget(m_globalPage);
        m_strategyLabel->setText(
            QStringLiteral("生产策略：Global 三维表面壳层"));
        const int modeIndex = m_globalMode->findData(
            static_cast<int>(state.partitionmode));
        if (modeIndex >= 0)
        {
            m_globalMode->setCurrentIndex(modeIndex);
        }
        m_globalWidth->setValue(
            std::max(0.10, state.requestedwidthmm));
        const bool editable = state.editable && state.valid;
        m_globalMode->setEnabled(editable);
        m_globalWidth->setEnabled(
            editable
            && state.partitionmode
                == ProductionTexturePartitionMode::PartialShell);
        m_globalEffectiveWidth->setText(
            state.partitionmode
                    == ProductionTexturePartitionMode::AllTexture
                ? QStringLiteral("全纹理（无 Model Fill 分区）")
                : QStringLiteral("%1 mm")
                      .arg(state.effectivewidthmm, 0, 'f', 2));
        m_globalBackend->setText(
            state.backend.isEmpty()
                ? QStringLiteral("未解析")
                : state.backend);
    }
    else
    {
        m_pages->setCurrentWidget(m_unsupportedPage);
        m_strategyLabel->setText(
            QStringLiteral("生产策略：当前 Profile 不支持"));
    }

    UpdateStateLabels(
        state.stale,
        state.editable,
        state.valid,
        state.lockreason,
        state.issues);
    m_updating = false;
}

void ProductionTextureSettingsPanel::OnGlobalControlChanged()
{
    if (m_updating || m_globalMode->currentIndex() < 0)
    {
        return;
    }
    const ProductionTexturePartitionMode mode =
        static_cast<ProductionTexturePartitionMode>(
            m_globalMode->currentData().toInt());
    m_globalWidth->setEnabled(
        m_globalMode->isEnabled()
        && mode == ProductionTexturePartitionMode::PartialShell);
    emit SigGlobalTextureChanged(m_globalWidth->value(), mode);
}

void ProductionTextureSettingsPanel::UpdateStateLabels(
    const bool stale,
    const bool editable,
    const bool valid,
    const QString& lockReason,
    const QStringList& issues)
{
    m_stateLabel->setText(
        stale
            ? QStringLiteral("输出状态：需要重新切片（stale）")
            : QStringLiteral("输出状态：当前设置已同步"));
    QString detail;
    if (!valid && !issues.isEmpty())
    {
        detail = QStringLiteral("配置错误：")
            + issues.join(QStringLiteral("；"));
    }
    else if (!editable)
    {
        detail = lockReason.isEmpty()
            ? QStringLiteral("当前 Profile 锁定了该设置。")
            : lockReason;
    }
    else
    {
        detail = QStringLiteral("可编辑；保存后可回读，重新切片后生效。");
    }
    m_lockLabel->setText(detail);
}
