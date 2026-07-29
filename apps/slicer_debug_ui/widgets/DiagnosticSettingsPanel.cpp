#include "DiagnosticSettingsPanel.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace
{

constexpr double kMinimumWidthMm{0.10};
constexpr double kMaximumWidthMm{6.00};
constexpr int kWidthScale{100};

int WidthToSlider(const double widthMm)
{
    return static_cast<int>(
        std::lround(widthMm * kWidthScale));
}

double SliderToWidth(const int value)
{
    return static_cast<double>(value)
        / static_cast<double>(kWidthScale);
}

}  // namespace

DiagnosticSettingsPanel::DiagnosticSettingsPanel(
    QWidget* parent)
    : QWidget(parent)
{
    setObjectName(
        QStringLiteral("diagnosticSettingsPanel"));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 6, 0, 0);
    layout->setSpacing(6);

    auto* title = new QLabel(
        QStringLiteral(
            "纹理与填充诊断试算"),
        this);
    title->setToolTip(
        QStringLiteral(
            "这些参数只用于诊断请求，不会直接修改生产 Profile 或 TIFF。"));
    layout->addWidget(title);

    auto* diagnosticOnlyNotice = new QLabel(
        QStringLiteral(
            "仅用于分析当前模型的宽度上限与分区结果，"
            "不会修改生产 Profile、模型填充材料或 TIFF。"
            "生产设置请使用顶部“工艺 Profile”或中央“配置”页。"),
        this);
    diagnosticOnlyNotice->setObjectName(
        QStringLiteral("diagnosticOnlyNoticeLabel"));
    diagnosticOnlyNotice->setWordWrap(true);
    diagnosticOnlyNotice->setToolTip(
        QStringLiteral(
            "诊断参数保存到 session 专用 diagnostic effective config，"
            "与生产 Effective Config 相互隔离。"));
    layout->addWidget(diagnosticOnlyNotice);

    m_subjectLabel = new QLabel(this);
    m_subjectLabel->setObjectName(
        QStringLiteral("diagnosticSubjectSummaryLabel"));
    m_subjectLabel->setWordWrap(true);
    m_subjectLabel->setToolTip(
        QStringLiteral(
            "显示当前诊断绑定的场景、实例和 revision。"));
    layout->addWidget(m_subjectLabel);

    auto* form = new QFormLayout();
    m_widthSpin = new QDoubleSpinBox(this);
    m_widthSpin->setObjectName(
        QStringLiteral(
            "diagnosticTextureSurfaceWidthSpin"));
    m_widthSpin->setDecimals(2);
    m_widthSpin->setSingleStep(0.01);
    m_widthSpin->setRange(
        kMinimumWidthMm,
        kMaximumWidthMm);
    m_widthSpin->setSuffix(QStringLiteral(" mm"));
    m_widthSpin->setKeyboardTracking(false);
    m_widthSpin->setToolTip(
        QStringLiteral(
            "设置 Texture Surface Layer 的诊断宽度；步长 0.01 mm。"
            "该值不会直接写入生产 Profile。"));

    m_widthSlider = new QSlider(
        Qt::Horizontal,
        this);
    m_widthSlider->setObjectName(
        QStringLiteral(
            "diagnosticTextureSurfaceWidthSlider"));
    m_widthSlider->setRange(
        WidthToSlider(kMinimumWidthMm),
        WidthToSlider(kMaximumWidthMm));
    m_widthSlider->setSingleStep(1);
    m_widthSlider->setPageStep(10);
    m_widthSlider->setToolTip(
        QStringLiteral(
            "拖动调整诊断纹理宽度；每一格代表 0.01 mm。"));

    auto* widthContainer = new QWidget(this);
    auto* widthLayout = new QVBoxLayout(widthContainer);
    widthLayout->setContentsMargins(0, 0, 0, 0);
    widthLayout->setSpacing(3);
    widthLayout->addWidget(m_widthSpin);
    widthLayout->addWidget(m_widthSlider);
    form->addRow(
        QStringLiteral("诊断纹理宽度"),
        widthContainer);

    m_modelFillMaterial = new QComboBox(this);
    m_modelFillMaterial->setObjectName(
        QStringLiteral(
            "diagnosticModelFillMaterialCombo"));
    m_modelFillMaterial->addItem(
        QStringLiteral("白墨填充"),
        QStringLiteral("white"));
    m_modelFillMaterial->addItem(
        QStringLiteral("光油填充"),
        QStringLiteral("varnish"));
    m_modelFillMaterial->addItem(
        QStringLiteral("RGB 实体填充"),
        QStringLiteral("rgb"));
    m_modelFillMaterial->setToolTip(
        QStringLiteral(
            "选择诊断试算中的 Model Fill Layer 材料。"
            "该选择不会修改生产 Profile 或生产 TIFF；"
            "生产材料请使用顶部工艺 Profile 或中央配置页。"));
    form->addRow(
        QStringLiteral("诊断填充材料"),
        m_modelFillMaterial);
    layout->addLayout(form);

    m_widthBoundsLabel = new QLabel(this);
    m_widthBoundsLabel->setObjectName(
        QStringLiteral(
            "diagnosticWidthBoundsLabel"));
    m_widthBoundsLabel->setWordWrap(true);
    m_widthBoundsLabel->setToolTip(
        QStringLiteral(
            "工程最小值固定为 0.10 mm；模型最大值和全纹理阈值"
            "由后续异步诊断计算，未计算时明确显示未评估。"));
    layout->addWidget(m_widthBoundsLabel);

    m_backendLabel = new QLabel(this);
    m_backendLabel->setObjectName(
        QStringLiteral(
            "diagnosticBackendAvailabilityLabel"));
    m_backendLabel->setWordWrap(true);
    m_backendLabel->setToolTip(
        QStringLiteral(
            "显示 Legacy CPU 与可选 OpenVDB 诊断后端是否可用；"
            "不代表生产准入。"));
    layout->addWidget(m_backendLabel);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName(
        QStringLiteral("diagnosticStatusLabel"));
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setToolTip(
        QStringLiteral(
            "显示诊断参数状态；诊断结论不等同于生产准入。"));
    layout->addWidget(m_statusLabel);

    m_blockingReasonsLabel = new QLabel(this);
    m_blockingReasonsLabel->setObjectName(
        QStringLiteral(
            "diagnosticBlockingReasonsLabel"));
    m_blockingReasonsLabel->setWordWrap(true);
    m_blockingReasonsLabel->setToolTip(
        QStringLiteral(
            "显示阻止诊断执行或导致结果不可复用的原因。"));
    layout->addWidget(m_blockingReasonsLabel);

    auto* actionLayout = new QHBoxLayout();
    m_startButton = new QPushButton(
        QStringLiteral("开始诊断"),
        this);
    m_startButton->setObjectName(
        QStringLiteral("diagnosticStartAnalysisButton"));
    m_startButton->setToolTip(
        QStringLiteral(
            "在后台执行拓扑、距离、纹理分区和栅格映射诊断；"
            "不会写生产 TIFF 或 Package。"
            "严格拓扑失败时宽度上限保持“未评估”。"));
    m_cancelButton = new QPushButton(
        QStringLiteral("取消诊断"),
        this);
    m_cancelButton->setObjectName(
        QStringLiteral("diagnosticCancelAnalysisButton"));
    m_cancelButton->setToolTip(
        QStringLiteral(
            "逻辑取消当前诊断；同步核心阶段返回后会丢弃旧结果。"));
    actionLayout->addWidget(m_startButton);
    actionLayout->addWidget(m_cancelButton);
    layout->addLayout(actionLayout);
    layout->addStretch(1);

    connect(
        m_widthSpin,
        qOverload<double>(
            &QDoubleSpinBox::valueChanged),
        this,
        [this](const double value)
        {
            const int sliderValue =
                WidthToSlider(value);
            if (m_widthSlider->value()
                != sliderValue)
            {
                const QSignalBlocker blocker(
                    m_widthSlider);
                m_widthSlider->setValue(sliderValue);
            }
            emit SigTextureSurfaceWidthChanged(value);
        });
    connect(
        m_widthSlider,
        &QSlider::valueChanged,
        this,
        [this](const int value)
        {
            m_widthSpin->setValue(
                SliderToWidth(value));
        });
    connect(
        m_modelFillMaterial,
        qOverload<int>(
            &QComboBox::currentIndexChanged),
        this,
        [this](const int index)
        {
            emit SigModelFillMaterialChanged(
                m_modelFillMaterial
                    ->itemData(index)
                    .toString());
        });
    connect(
        m_startButton,
        &QPushButton::clicked,
        this,
        &DiagnosticSettingsPanel::
            SigStartAnalysisRequested);
    connect(
        m_cancelButton,
        &QPushButton::clicked,
        this,
        &DiagnosticSettingsPanel::
            SigCancelAnalysisRequested);

    SetRequestedSettings(
        kMinimumWidthMm,
        QStringLiteral("white"));
    SetPresentation({});
}

void DiagnosticSettingsPanel::SetRequestedSettings(
    const double widthMm,
    const QString& modelFillMaterial)
{
    const double clampedWidth =
        std::clamp(
            widthMm,
            kMinimumWidthMm,
            kMaximumWidthMm);
    const QSignalBlocker spinBlocker(m_widthSpin);
    const QSignalBlocker sliderBlocker(m_widthSlider);
    const QSignalBlocker materialBlocker(
        m_modelFillMaterial);
    m_widthSpin->setValue(clampedWidth);
    m_widthSlider->setValue(
        WidthToSlider(clampedWidth));
    const int materialIndex =
        m_modelFillMaterial->findData(
            modelFillMaterial);
    m_modelFillMaterial->setCurrentIndex(
        materialIndex >= 0 ? materialIndex : 0);
}

void DiagnosticSettingsPanel::SetPresentation(
    const DiagnosticSettingsPresentation& presentation)
{
    const double minimumWidth =
        presentation.minimumwidthmm.value_or(
            kMinimumWidthMm);
    const double maximumWidth =
        std::max(
            minimumWidth,
            presentation.maximumwidthmm.value_or(
                kMaximumWidthMm));
    {
        const QSignalBlocker spinBlocker(m_widthSpin);
        const QSignalBlocker sliderBlocker(
            m_widthSlider);
        m_widthSpin->setRange(
            minimumWidth,
            maximumWidth);
        m_widthSlider->setRange(
            WidthToSlider(minimumWidth),
            WidthToSlider(maximumWidth));
        m_widthSlider->setValue(
            WidthToSlider(m_widthSpin->value()));
    }
    m_subjectLabel->setText(
        QStringLiteral("诊断对象：")
        + presentation.subjectsummary);
    m_widthBoundsLabel->setText(
        QStringLiteral(
            "宽度边界：最小 %1；最大 %2；全纹理阈值 %3")
            .arg(
                FormatWidth(
                    presentation.minimumwidthmm,
                    QStringLiteral("0.10 mm（工程值）")),
                FormatWidth(
                    presentation.maximumwidthmm,
                    QStringLiteral("未评估")),
                FormatWidth(
                    presentation.alltexturethresholdmm,
                    QStringLiteral("未评估"))));
    m_backendLabel->setText(
        QStringLiteral("后端状态：")
        + presentation.backendavailability);
    m_statusLabel->setText(
        QStringLiteral("诊断状态：")
        + presentation.status);
    m_blockingReasonsLabel->setText(
        presentation.blockingreasons.isEmpty()
            ? QStringLiteral("阻断原因：无")
            : QStringLiteral("阻断原因：")
                + presentation.blockingreasons.join(
                    QStringLiteral("；")));
    const bool canEdit =
        presentation.controlsenabled
        && !presentation.analysisrunning;
    m_widthSpin->setEnabled(canEdit);
    m_widthSlider->setEnabled(canEdit);
    m_modelFillMaterial->setEnabled(canEdit);
    m_startButton->setEnabled(canEdit);
    m_cancelButton->setEnabled(
        presentation.analysisrunning);
}

double DiagnosticSettingsPanel::
RequestedTextureSurfaceWidthMm() const
{
    return m_widthSpin->value();
}

QString DiagnosticSettingsPanel::
RequestedModelFillMaterial() const
{
    return m_modelFillMaterial
        ->currentData()
        .toString();
}

QString DiagnosticSettingsPanel::FormatWidth(
    const std::optional<double>& width,
    const QString& missingText) const
{
    return width.has_value()
        ? QStringLiteral("%1 mm")
              .arg(*width, 0, 'f', 2)
        : missingText;
}
