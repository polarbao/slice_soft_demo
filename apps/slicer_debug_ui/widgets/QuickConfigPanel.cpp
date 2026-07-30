#include "QuickConfigPanel.h"

#include "../services/HelpTextProvider.h"
#include "../services/ProductionModeCatalog.h"
#include "slicer_core/config.h"

#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace
{

QPushButton* MakeButton(const QString& text, QWidget* parent)
{
    auto* button = new QPushButton(text, parent);
    button->setMinimumHeight(28);
    return button;
}

QLineEdit* MakePathEdit(QWidget* parent)
{
    auto* edit = new QLineEdit(parent);
    edit->setMinimumWidth(360);
    return edit;
}

QDoubleSpinBox* MakeScaleSpin(const QString& objectName, QWidget* parent)
{
    auto* spin = new QDoubleSpinBox(parent);
    spin->setObjectName(objectName);
    spin->setRange(0.01, 100.0);
    spin->setDecimals(4);
    spin->setSingleStep(0.05);
    spin->setValue(1.0);
    spin->setMinimumWidth(92);
    spin->setKeyboardTracking(false);
    return spin;
}

void AddComboOption(QComboBox* combo, const QString& label, const QString& value)
{
    combo->addItem(label, value);
}

void SetComboValue(QComboBox* combo, const QString& value)
{
    int index = combo->findData(value);
    if (index < 0)
    {
        index = combo->count();
        combo->addItem("未知值：" + value, value);
    }
    combo->setCurrentIndex(index);
}

QString ComboValue(const QComboBox* combo, const int index)
{
    return combo->itemData(index).toString();
}

void ApplyHelp(QWidget* widget, const QString& key)
{
    widget->setToolTip(HelpTextProvider::ToolTip(key));
}

}  // namespace

QuickConfigPanel::QuickConfigPanel(ConfigDocument* document, QWidget* parent)
    : QWidget(parent)
    , m_document(document)
{
    auto* layout = new QVBoxLayout(this);
    auto* basicGroup = new QGroupBox("基础", this);
    basicGroup->setToolTip("基础输入输出：选择模型、输出目录、模型缩放、X/Y DPI 和层高。模型缩放为 1.0 时保持原始尺寸，DPI 只控制输出栅格密度。");
    auto* basicForm = new QFormLayout(basicGroup);

    m_modelPathEdit = MakePathEdit(this);
    ApplyHelp(m_modelPathEdit, QStringLiteral("input.modelPath"));
    auto* modelBrowseButton = MakeButton("...", this);
    modelBrowseButton->setToolTip("选择 OBJ、STL 或 3MF 模型文件。");
    auto* modelPathRow = new QHBoxLayout();
    modelPathRow->addWidget(m_modelPathEdit, 1);
    modelPathRow->addWidget(modelBrowseButton);
    basicForm->addRow("模型文件", modelPathRow);

    m_outputDirEdit = MakePathEdit(this);
    ApplyHelp(m_outputDirEdit, QStringLiteral("output.packageDir"));
    auto* outputBrowseButton = MakeButton("...", this);
    outputBrowseButton->setToolTip("选择输出目录。");
    auto* outputDirRow = new QHBoxLayout();
    outputDirRow->addWidget(m_outputDirEdit, 1);
    outputDirRow->addWidget(outputBrowseButton);
    basicForm->addRow("输出目录", outputDirRow);

    m_modelScaleXSpin = MakeScaleSpin(QStringLiteral("modelScaleXSpin"), this);
    m_modelScaleYSpin = MakeScaleSpin(QStringLiteral("modelScaleYSpin"), this);
    m_modelScaleZSpin = MakeScaleSpin(QStringLiteral("modelScaleZSpin"), this);
    ApplyHelp(m_modelScaleXSpin, QStringLiteral("modelTransform.scale"));
    ApplyHelp(m_modelScaleYSpin, QStringLiteral("modelTransform.scale"));
    ApplyHelp(m_modelScaleZSpin, QStringLiteral("modelTransform.scale"));
    auto* modelScaleRow = new QHBoxLayout();
    modelScaleRow->addWidget(new QLabel(QStringLiteral("X"), this));
    modelScaleRow->addWidget(m_modelScaleXSpin);
    modelScaleRow->addWidget(new QLabel(QStringLiteral("Y"), this));
    modelScaleRow->addWidget(m_modelScaleYSpin);
    modelScaleRow->addWidget(new QLabel(QStringLiteral("Z"), this));
    modelScaleRow->addWidget(m_modelScaleZSpin);
    auto* resetModelScaleButton = MakeButton(QStringLiteral("重置 1:1"), this);
    resetModelScaleButton->setObjectName(QStringLiteral("resetModelScaleButton"));
    resetModelScaleButton->setToolTip(QStringLiteral("将 X/Y/Z 模型缩放恢复为 1.0，不改变模型原始物理尺寸。"));
    modelScaleRow->addWidget(resetModelScaleButton);
    modelScaleRow->addStretch(1);
    basicForm->addRow("模型缩放", modelScaleRow);

    m_layerHeightSpin = new QDoubleSpinBox(this);
    m_layerHeightSpin->setObjectName(
        QStringLiteral("layerHeightSpin"));
    m_layerHeightSpin->setRange(0.001, 1.0);
    m_layerHeightSpin->setDecimals(4);
    m_layerHeightSpin->setSingleStep(0.005);
    m_layerHeightSpin->setSuffix(" mm");
    m_layerHeightSpin->setKeyboardTracking(false);
    ApplyHelp(m_layerHeightSpin, QStringLiteral("output.layerThicknessMm"));
    basicForm->addRow("层高", m_layerHeightSpin);

    m_outputDpiXSpin = new QSpinBox(this);
    m_outputDpiXSpin->setObjectName(QStringLiteral("outputDpiXSpin"));
    m_outputDpiXSpin->setRange(
        slicer_core::kMinimumOutputDpi,
        slicer_core::kMaximumOutputDpi);
    m_outputDpiXSpin->setSingleStep(1);
    m_outputDpiXSpin->setSuffix(QStringLiteral(" dpi"));
    m_outputDpiXSpin->setMinimumWidth(112);
    m_outputDpiXSpin->setKeyboardTracking(false);
    ApplyHelp(m_outputDpiXSpin, QStringLiteral("output.dpiX"));

    m_outputDpiYSpin = new QSpinBox(this);
    m_outputDpiYSpin->setObjectName(QStringLiteral("outputDpiYSpin"));
    m_outputDpiYSpin->setRange(
        slicer_core::kMinimumOutputDpi,
        slicer_core::kMaximumOutputDpi);
    m_outputDpiYSpin->setSingleStep(1);
    m_outputDpiYSpin->setSuffix(QStringLiteral(" dpi"));
    m_outputDpiYSpin->setMinimumWidth(112);
    m_outputDpiYSpin->setKeyboardTracking(false);
    ApplyHelp(m_outputDpiYSpin, QStringLiteral("output.dpiY"));

    auto* outputDpiRow = new QHBoxLayout();
    outputDpiRow->addWidget(new QLabel(QStringLiteral("X"), this));
    outputDpiRow->addWidget(m_outputDpiXSpin);
    outputDpiRow->addSpacing(12);
    outputDpiRow->addWidget(new QLabel(QStringLiteral("Y"), this));
    outputDpiRow->addWidget(m_outputDpiYSpin);
    outputDpiRow->addStretch(1);
    basicForm->addRow(QStringLiteral("输出分辨率"), outputDpiRow);

    m_outputPixelSizeLabel = new QLabel(this);
    m_outputPixelSizeLabel->setObjectName(QStringLiteral("outputPixelSizeLabel"));
    m_outputPixelSizeLabel->setWordWrap(true);
    m_outputPixelSizeLabel->setToolTip(
        QStringLiteral("由 25.4 / DPI 计算。DPI 控制栅格密度，不等于模型缩放。"));
    basicForm->addRow(QStringLiteral("物理像素尺寸"), m_outputPixelSizeLabel);

    auto* materialGroup = new QGroupBox("材料", this);
    materialGroup->setToolTip("常用材料设置：控制 RGB 纹理、非表面 RGB、白墨和光油。");
    auto* materialForm = new QFormLayout(materialGroup);
    m_texturePolicyCombo = new QComboBox(this);
    AddComboOption(m_texturePolicyCombo, "顶面纹理带", "top_surface_band");
    AddComboOption(m_texturePolicyCombo, "顶面纹理投影到实体", "solid_volume_from_top_surface");
    AddComboOption(m_texturePolicyCombo, "实体填充", "solid_volume");
    AddComboOption(m_texturePolicyCombo, "SDF 表面壳层", "surface_shell_from_sdf");
    AddComboOption(m_texturePolicyCombo, "禁用纹理", "disabled");
    ApplyHelp(m_texturePolicyCombo, QStringLiteral("texture.applyMode"));
    materialForm->addRow("纹理策略", m_texturePolicyCombo);

    m_nonSurfaceRgbPolicyCombo = new QComboBox(this);
    AddComboOption(m_nonSurfaceRgbPolicyCombo, "使用模型材料", "model_material");
    AddComboOption(m_nonSurfaceRgbPolicyCombo, "视为空白", "empty");
    AddComboOption(m_nonSurfaceRgbPolicyCombo, "使用备用 RGB", "fallback_rgb");
    AddComboOption(m_nonSurfaceRgbPolicyCombo, "交给材料策略", "material_policy");
    ApplyHelp(m_nonSurfaceRgbPolicyCombo, QStringLiteral("texture.nonSurfaceRgbPolicy"));
    materialForm->addRow("非表面 RGB", m_nonSurfaceRgbPolicyCombo);

    m_modelFillMaterialCombo = new QComboBox(this);
    m_modelFillMaterialCombo->setObjectName(QStringLiteral("modelFillMaterialCombo"));
    AddComboOption(m_modelFillMaterialCombo, "全实体 RGB（无白墨）", "rgb");
    AddComboOption(m_modelFillMaterialCombo, "白墨填充", "white");
    AddComboOption(m_modelFillMaterialCombo, "光油填充", "varnish");
    ApplyHelp(m_modelFillMaterialCombo, QStringLiteral("modelFill.material"));
    materialForm->addRow("模型内部填充", m_modelFillMaterialCombo);

    m_supportEnabledCheck = new QCheckBox("启用支撑", this);
    m_supportEnabledCheck->setObjectName(QStringLiteral("supportEnabledCheck"));
    m_whiteEnabledCheck = new QCheckBox("叠加白墨底层", this);
    m_whiteEnabledCheck->setObjectName(QStringLiteral("whitePolicyEnabledCheck"));
    m_varnishEnabledCheck = new QCheckBox("启用顶部光油策略", this);
    m_previewEnabledCheck = new QCheckBox("自动生成诊断图", this);
    m_previewEnabledCheck->setObjectName(
        QStringLiteral("previewDiagnosticImagesCheck"));
    m_openVdbEnabledCheck = new QCheckBox("启用 OpenVDB 实验管线", this);
    ApplyHelp(m_supportEnabledCheck, QStringLiteral("support.enabled"));
    ApplyHelp(m_whiteEnabledCheck, QStringLiteral("materialPolicy.white.enabled"));
    ApplyHelp(m_varnishEnabledCheck, QStringLiteral("materialPolicy.varnish.enabled"));
    ApplyHelp(m_previewEnabledCheck, QStringLiteral("preview.enabled"));
    ApplyHelp(m_openVdbEnabledCheck, QStringLiteral("engine.openvdbCandidate"));
    materialForm->addRow(m_whiteEnabledCheck);
    materialForm->addRow(m_varnishEnabledCheck);

    m_varnishTopLayersSpin = new QSpinBox(this);
    m_varnishTopLayersSpin->setRange(0, 100000);
    m_varnishTopLayersSpin->setKeyboardTracking(false);
    ApplyHelp(m_varnishTopLayersSpin, QStringLiteral("materialPolicy.varnish.topLayers"));
    materialForm->addRow("光油顶部层数", m_varnishTopLayersSpin);

    m_surfaceVarnishEnabledCheck = new QCheckBox("启用表面光油", this);
    m_surfaceVarnishEnabledCheck->setObjectName(QStringLiteral("surfaceVarnishEnabledCheck"));
    ApplyHelp(m_surfaceVarnishEnabledCheck, QStringLiteral("surfaceVarnish.enabled"));
    materialForm->addRow(m_surfaceVarnishEnabledCheck);

    m_surfaceVarnishThicknessSpin = new QSpinBox(this);
    m_surfaceVarnishThicknessSpin->setRange(0, 100);
    m_surfaceVarnishThicknessSpin->setSuffix(" px");
    m_surfaceVarnishThicknessSpin->setKeyboardTracking(false);
    ApplyHelp(m_surfaceVarnishThicknessSpin, QStringLiteral("surfaceVarnish.thicknessPx"));
    materialForm->addRow("表面光油厚度", m_surfaceVarnishThicknessSpin);

    m_outerVarnishEnabledCheck = new QCheckBox("启用外侧光油壳层", this);
    m_outerVarnishEnabledCheck->setObjectName(QStringLiteral("outerVarnishEnabledCheck"));
    ApplyHelp(m_outerVarnishEnabledCheck, QStringLiteral("outerVarnish.enabled"));
    materialForm->addRow(m_outerVarnishEnabledCheck);

    m_outerVarnishThicknessSpin = new QDoubleSpinBox(this);
    m_outerVarnishThicknessSpin->setRange(0.0, 10.0);
    m_outerVarnishThicknessSpin->setDecimals(2);
    m_outerVarnishThicknessSpin->setSingleStep(0.01);
    m_outerVarnishThicknessSpin->setSuffix(" mm");
    m_outerVarnishThicknessSpin->setKeyboardTracking(false);
    ApplyHelp(m_outerVarnishThicknessSpin, QStringLiteral("outerVarnish.thicknessMm"));
    materialForm->addRow("外侧光油厚度", m_outerVarnishThicknessSpin);

    auto* supportGroup = new QGroupBox("支撑", this);
    supportGroup->setToolTip("模型外部 S 通道支撑。可设置摆放位置和内部镂空填充。");
    auto* supportForm = new QFormLayout(supportGroup);
    supportForm->addRow(m_supportEnabledCheck);
    m_supportPlacementCombo = new QComboBox(this);
    m_supportPlacementCombo->setObjectName(QStringLiteral("supportPlacementCombo"));
    AddComboOption(m_supportPlacementCombo, "下表面", "lower");
    AddComboOption(m_supportPlacementCombo, "上表面", "upper");
    AddComboOption(m_supportPlacementCombo, "上、下表面", "both");
    AddComboOption(m_supportPlacementCombo, "仅悬空区域", "unsupported_only");
    AddComboOption(m_supportPlacementCombo, "完整垂直投影", "full_vertical_projection");
    ApplyHelp(m_supportPlacementCombo, QStringLiteral("support.placement"));
    supportForm->addRow("支撑位置", m_supportPlacementCombo);

    m_internalVoidEnabledCheck = new QCheckBox("填充内部镂空", this);
    ApplyHelp(m_internalVoidEnabledCheck, QStringLiteral("support.internalVoid.enabled"));
    supportForm->addRow(m_internalVoidEnabledCheck);
    m_internalVoidMinAreaSpin = new QSpinBox(this);
    m_internalVoidMinAreaSpin->setRange(0, 100000000);
    m_internalVoidMinAreaSpin->setSuffix(" px");
    m_internalVoidMinAreaSpin->setKeyboardTracking(false);
    ApplyHelp(m_internalVoidMinAreaSpin, QStringLiteral("support.internalVoid.minAreaPx"));
    supportForm->addRow("镂空最小面积", m_internalVoidMinAreaSpin);

    m_baseProjectionEnabledCheck =
        new QCheckBox("启用支撑投影铺底", this);
    m_baseProjectionEnabledCheck->setObjectName(
        QStringLiteral("baseProjectionEnabledCheck"));
    ApplyHelp(
        m_baseProjectionEnabledCheck,
        QStringLiteral("support.baseProjection.enabled"));
    supportForm->addRow(m_baseProjectionEnabledCheck);
    m_baseProjectionLayerCountSpin = new QSpinBox(this);
    m_baseProjectionLayerCountSpin->setObjectName(
        QStringLiteral("baseProjectionLayerCountSpin"));
    m_baseProjectionLayerCountSpin->setRange(0, 1000);
    m_baseProjectionLayerCountSpin->setSuffix(" 层");
    m_baseProjectionLayerCountSpin->setKeyboardTracking(false);
    ApplyHelp(
        m_baseProjectionLayerCountSpin,
        QStringLiteral("support.baseProjection.layerCount"));
    supportForm->addRow(
        "铺底层数",
        m_baseProjectionLayerCountSpin);

    auto* previewGroup = new QGroupBox("诊断图输出", this);
    previewGroup->setToolTip(
        "默认关闭重复图片写出。UI 始终可直接读取生产 RGBWSV TIFF 进行预览。");
    auto* previewForm = new QFormLayout(previewGroup);
    previewForm->addRow(m_previewEnabledCheck);
    m_previewIntervalSpin = new QSpinBox(this);
    m_previewIntervalSpin->setObjectName(QStringLiteral("previewIntervalSpin"));
    m_previewIntervalSpin->setRange(1, 100000);
    m_previewIntervalSpin->setKeyboardTracking(false);
    ApplyHelp(m_previewIntervalSpin, QStringLiteral("preview.interval"));
    previewForm->addRow("诊断图间隔", m_previewIntervalSpin);

    m_openVdbEnabledCheck->setObjectName(QStringLiteral("openVdbCandidateCheck"));
    m_openVdbEnabledCheck->setVisible(false);

    layout->addWidget(basicGroup);
    layout->addWidget(materialGroup);
    layout->addWidget(supportGroup);
    layout->addWidget(previewGroup);

    m_normalizedView = new QPlainTextEdit(this);
    m_normalizedView->setReadOnly(true);
    m_normalizedView->setMinimumHeight(180);
    layout->addWidget(m_normalizedView, 1);

    connect(modelBrowseButton, &QPushButton::clicked, this, &QuickConfigPanel::OnBrowseModel);
    connect(outputBrowseButton, &QPushButton::clicked, this, &QuickConfigPanel::OnBrowseOutput);
    connect(m_modelPathEdit, &QLineEdit::editingFinished, this, &QuickConfigPanel::OnModelPathEdited);
    connect(m_outputDirEdit, &QLineEdit::editingFinished, this, &QuickConfigPanel::OnOutputDirEdited);
    connect(m_modelScaleXSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &QuickConfigPanel::OnModelScaleChanged);
    connect(m_modelScaleYSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &QuickConfigPanel::OnModelScaleChanged);
    connect(m_modelScaleZSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &QuickConfigPanel::OnModelScaleChanged);
    connect(resetModelScaleButton, &QPushButton::clicked, this, &QuickConfigPanel::OnResetModelScale);
    connect(m_layerHeightSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &QuickConfigPanel::OnLayerHeightChanged);
    connect(m_outputDpiXSpin, qOverload<int>(&QSpinBox::valueChanged), this, &QuickConfigPanel::OnOutputDpiChanged);
    connect(m_outputDpiYSpin, qOverload<int>(&QSpinBox::valueChanged), this, &QuickConfigPanel::OnOutputDpiChanged);
    connect(m_texturePolicyCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &QuickConfigPanel::OnTexturePolicyChanged);
    connect(m_nonSurfaceRgbPolicyCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &QuickConfigPanel::OnNonSurfaceRgbPolicyChanged);
    connect(m_modelFillMaterialCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &QuickConfigPanel::OnModelFillMaterialChanged);
    connect(m_supportEnabledCheck, &QCheckBox::toggled, this, &QuickConfigPanel::OnSupportEnabledChanged);
    connect(m_supportPlacementCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &QuickConfigPanel::OnSupportPlacementChanged);
    connect(m_internalVoidEnabledCheck, &QCheckBox::toggled, this, &QuickConfigPanel::OnInternalVoidEnabledChanged);
    connect(m_internalVoidMinAreaSpin, qOverload<int>(&QSpinBox::valueChanged), this, &QuickConfigPanel::OnInternalVoidMinAreaChanged);
    connect(
        m_baseProjectionEnabledCheck,
        &QCheckBox::toggled,
        this,
        &QuickConfigPanel::OnBaseProjectionEnabledChanged);
    connect(
        m_baseProjectionLayerCountSpin,
        qOverload<int>(&QSpinBox::valueChanged),
        this,
        &QuickConfigPanel::OnBaseProjectionLayerCountChanged);
    connect(m_whiteEnabledCheck, &QCheckBox::toggled, this, &QuickConfigPanel::OnWhiteEnabledChanged);
    connect(m_varnishEnabledCheck, &QCheckBox::toggled, this, &QuickConfigPanel::OnVarnishEnabledChanged);
    connect(m_varnishTopLayersSpin, qOverload<int>(&QSpinBox::valueChanged), this, &QuickConfigPanel::OnVarnishTopLayersChanged);
    connect(m_surfaceVarnishEnabledCheck, &QCheckBox::toggled, this, &QuickConfigPanel::OnSurfaceVarnishEnabledChanged);
    connect(m_surfaceVarnishThicknessSpin, qOverload<int>(&QSpinBox::valueChanged), this, &QuickConfigPanel::OnSurfaceVarnishThicknessChanged);
    connect(m_outerVarnishEnabledCheck, &QCheckBox::toggled, this, &QuickConfigPanel::OnOuterVarnishEnabledChanged);
    connect(m_outerVarnishThicknessSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &QuickConfigPanel::OnOuterVarnishThicknessChanged);
    connect(m_previewEnabledCheck, &QCheckBox::toggled, this, &QuickConfigPanel::OnPreviewEnabledChanged);
    connect(m_previewIntervalSpin, qOverload<int>(&QSpinBox::valueChanged), this, &QuickConfigPanel::OnPreviewIntervalChanged);
    connect(m_openVdbEnabledCheck, &QCheckBox::toggled, this, &QuickConfigPanel::OnOpenVdbEnabledChanged);
    connect(m_document, &ConfigDocument::changed, this, &QuickConfigPanel::LoadFromDocument);
}

void QuickConfigPanel::LoadFromDocument()
{
    m_loading = true;
    m_modelPathEdit->setText(StringValue({"input", "modelPath"}));
    m_outputDirEdit->setText(StringValue({"output", "packageDir"}));
    const QJsonArray scale = m_document->value({"modelTransform", "scale"}).toArray();
    m_modelScaleXSpin->setValue(
        scale.size() == 3 && scale.at(0).isDouble() ? scale.at(0).toDouble() : 1.0);
    m_modelScaleYSpin->setValue(
        scale.size() == 3 && scale.at(1).isDouble() ? scale.at(1).toDouble() : 1.0);
    m_modelScaleZSpin->setValue(
        scale.size() == 3 && scale.at(2).isDouble() ? scale.at(2).toDouble() : 1.0);
    m_layerHeightSpin->setValue(
        DoubleValue(
            {"output", "layerThicknessMm"},
            slicer_core::kDefaultLayerThicknessMm));
    m_outputDpiXSpin->setValue(
        IntValue({"output", "dpiX"}, slicer_core::kDefaultOutputDpiX));
    m_outputDpiYSpin->setValue(
        IntValue({"output", "dpiY"}, slicer_core::kDefaultOutputDpiY));
    UpdateOutputPixelSizeLabel();
    SetComboValue(m_texturePolicyCombo, StringValue({"texture", "applyMode"}, "top_surface_band"));
    SetComboValue(m_nonSurfaceRgbPolicyCombo, StringValue({"texture", "nonSurfaceRgbPolicy"}, "model_material"));
    SetComboValue(
        m_modelFillMaterialCombo,
        StringValue({"modelFill", "material"}, "rgb"));
    m_supportEnabledCheck->setChecked(BoolValue({"support", "enabled"}, true));
    SetComboValue(m_supportPlacementCombo, StringValue({"support", "placement"}, "lower"));
    m_internalVoidEnabledCheck->setChecked(BoolValue({"support", "internalVoid", "enabled"}, true));
    m_internalVoidMinAreaSpin->setValue(IntValue({"support", "internalVoid", "minAreaPx"}, 16));
    m_baseProjectionEnabledCheck->setChecked(
        BoolValue({"support", "baseProjection", "enabled"}, true));
    m_baseProjectionLayerCountSpin->setValue(
        IntValue({"support", "baseProjection", "layerCount"}, 30));
    const bool materialPolicyEnabled = BoolValue({"materialPolicy", "enabled"}, false);
    const bool whitePolicyEnabled = materialPolicyEnabled
        && BoolValue({"materialPolicy", "white", "enabled"}, false)
        && StringValue({"materialPolicy", "white", "mode"}, "disabled") != QStringLiteral("disabled");
    const bool varnishPolicyEnabled = materialPolicyEnabled
        && BoolValue({"materialPolicy", "varnish", "enabled"}, false)
        && StringValue({"materialPolicy", "varnish", "mode"}, "disabled") != QStringLiteral("disabled");
    m_whiteEnabledCheck->setChecked(whitePolicyEnabled);
    m_varnishEnabledCheck->setChecked(varnishPolicyEnabled);
    m_varnishTopLayersSpin->setValue(IntValue({"materialPolicy", "varnish", "topLayers"}, 0));
    m_surfaceVarnishEnabledCheck->setChecked(BoolValue({"surfaceVarnish", "enabled"}, false));
    m_surfaceVarnishThicknessSpin->setValue(IntValue({"surfaceVarnish", "thicknessPx"}, 0));
    m_outerVarnishEnabledCheck->setChecked(BoolValue({"outerVarnish", "enabled"}, false));
    m_outerVarnishThicknessSpin->setValue(DoubleValue({"outerVarnish", "thicknessMm"}, 0.0));
    const QString previewOutputPolicy =
        StringValue({"preview", "outputPolicy"}, QString{});
    const bool automaticDiagnosticImages =
        previewOutputPolicy.isEmpty()
            ? BoolValue({"preview", "enabled"}, false)
            : previewOutputPolicy
                == QStringLiteral("tiff_native_with_diagnostics");
    m_previewEnabledCheck->setChecked(automaticDiagnosticImages);
    m_previewIntervalSpin->setValue(IntValue({"preview", "interval"}, 1));
    m_openVdbEnabledCheck->setChecked(BoolValue({"experimental", "openvdbPipeline", "enabled"}, false));
    UpdateNormalizedView();
    m_loading = false;
}

void QuickConfigPanel::ApplyProductionCapability(
    const ProductionProfileCapability* profile)
{
    const bool isLegacy = profile == nullptr;
    const bool supportAllowed = isLegacy
        || profile->supportscope == ProductionSupportScope::LowerAndInternalVoid;
    const bool varnishAllowed = isLegacy
        || profile->varnishscope == ProductionVarnishScope::SurfaceAndOuter;

    const QString profileLockReason = isLegacy
        ? QString{}
        : QStringLiteral("当前全局 Production Profile 已锁定该设置；切换回传统切片后可编辑。");
    const QString unsupportedSupportReason =
        QStringLiteral("当前全局受限材料 Profile 不支持 S 支撑输出。");
    const QString unsupportedVarnishReason =
        QStringLiteral("当前全局受限材料 Profile 不支持 V 光油输出。");

    const QList<QWidget*> profileLockedMaterialControls{
        m_texturePolicyCombo,
        m_nonSurfaceRgbPolicyCombo,
        m_modelFillMaterialCombo,
        m_whiteEnabledCheck,
    };
    for (QWidget* control : profileLockedMaterialControls)
    {
        control->setEnabled(isLegacy);
        if (!isLegacy)
        {
            control->setToolTip(profileLockReason);
        }
    }

    const QList<QWidget*> supportControls{
        m_supportEnabledCheck,
        m_supportPlacementCombo,
        m_internalVoidEnabledCheck,
        m_internalVoidMinAreaSpin,
        m_baseProjectionEnabledCheck,
        m_baseProjectionLayerCountSpin,
    };
    for (QWidget* control : supportControls)
    {
        control->setEnabled(isLegacy);
        if (!isLegacy)
        {
            control->setToolTip(
                supportAllowed ? profileLockReason : unsupportedSupportReason);
        }
    }

    const QList<QWidget*> varnishControls{
        m_varnishEnabledCheck,
        m_varnishTopLayersSpin,
        m_surfaceVarnishEnabledCheck,
        m_surfaceVarnishThicknessSpin,
        m_outerVarnishEnabledCheck,
        m_outerVarnishThicknessSpin,
    };
    for (QWidget* control : varnishControls)
    {
        control->setEnabled(isLegacy);
        if (!isLegacy)
        {
            control->setToolTip(
                varnishAllowed ? profileLockReason : unsupportedVarnishReason);
        }
    }

    if (isLegacy)
    {
        ApplyHelp(m_texturePolicyCombo, QStringLiteral("texture.applyMode"));
        ApplyHelp(m_nonSurfaceRgbPolicyCombo, QStringLiteral("texture.nonSurfaceRgbPolicy"));
        ApplyHelp(m_modelFillMaterialCombo, QStringLiteral("modelFill.material"));
        ApplyHelp(m_whiteEnabledCheck, QStringLiteral("materialPolicy.white.enabled"));
        ApplyHelp(m_supportEnabledCheck, QStringLiteral("support.enabled"));
        ApplyHelp(m_supportPlacementCombo, QStringLiteral("support.placement"));
        ApplyHelp(m_internalVoidEnabledCheck, QStringLiteral("support.internalVoid.enabled"));
        ApplyHelp(m_internalVoidMinAreaSpin, QStringLiteral("support.internalVoid.minAreaPx"));
        ApplyHelp(m_varnishEnabledCheck, QStringLiteral("materialPolicy.varnish.enabled"));
        ApplyHelp(m_varnishTopLayersSpin, QStringLiteral("materialPolicy.varnish.topLayers"));
        ApplyHelp(m_surfaceVarnishEnabledCheck, QStringLiteral("surfaceVarnish.enabled"));
        ApplyHelp(m_surfaceVarnishThicknessSpin, QStringLiteral("surfaceVarnish.thicknessPx"));
        ApplyHelp(m_outerVarnishEnabledCheck, QStringLiteral("outerVarnish.enabled"));
        ApplyHelp(m_outerVarnishThicknessSpin, QStringLiteral("outerVarnish.thicknessMm"));
    }
}

void QuickConfigPanel::OnBrowseModel()
{
    const QString path = QFileDialog::getOpenFileName(this, "选择模型文件", QDir::currentPath(), "Model (*.obj *.stl *.3mf)");
    if (path.isEmpty())
    {
        return;
    }
    m_modelPathEdit->setText(path);
    OnModelPathEdited();
}

void QuickConfigPanel::OnBrowseOutput()
{
    const QString path = QFileDialog::getExistingDirectory(this, "选择输出目录", QDir::currentPath());
    if (path.isEmpty())
    {
        return;
    }
    m_outputDirEdit->setText(path);
    OnOutputDirEdited();
}

void QuickConfigPanel::OnModelPathEdited()
{
    if (!m_loading)
    {
        SetValueIfChanged({"input", "modelPath"}, m_modelPathEdit->text());
    }
}

void QuickConfigPanel::OnOutputDirEdited()
{
    if (!m_loading)
    {
        SetValueIfChanged({"output", "packageDir"}, m_outputDirEdit->text());
    }
}

void QuickConfigPanel::OnModelScaleChanged(const double value)
{
    Q_UNUSED(value);
    if (m_loading)
    {
        return;
    }

    SetValueIfChanged(
        {"modelTransform", "scale"},
        QJsonArray{
            m_modelScaleXSpin->value(),
            m_modelScaleYSpin->value(),
            m_modelScaleZSpin->value()});
}

void QuickConfigPanel::OnResetModelScale()
{
    m_loading = true;
    m_modelScaleXSpin->setValue(1.0);
    m_modelScaleYSpin->setValue(1.0);
    m_modelScaleZSpin->setValue(1.0);
    m_loading = false;
    OnModelScaleChanged(1.0);
}

void QuickConfigPanel::OnLayerHeightChanged(const double value)
{
    if (!m_loading)
    {
        SetValueIfChanged({"output", "layerThicknessMm"}, value);
    }
}

void QuickConfigPanel::OnTexturePolicyChanged(const int index)
{
    if (!m_loading)
    {
        const QString value = ComboValue(m_texturePolicyCombo, index);
        if (value.isEmpty())
        {
            return;
        }
        SetValueIfChanged({"texture", "applyMode"}, value);
        if (value == "top_surface_band")
        {
            SetValueIfChanged({"texture", "topSurfaceLayers"}, 50);
        }
    }
}

void QuickConfigPanel::OnOutputDpiChanged(const int value)
{
    Q_UNUSED(value);
    UpdateOutputPixelSizeLabel();
    if (m_loading)
    {
        return;
    }

    SetValueIfChanged({"output", "dpiX"}, m_outputDpiXSpin->value());
    SetValueIfChanged({"output", "dpiY"}, m_outputDpiYSpin->value());
}

void QuickConfigPanel::OnNonSurfaceRgbPolicyChanged(const int index)
{
    if (!m_loading)
    {
        const QString value = ComboValue(m_nonSurfaceRgbPolicyCombo, index);
        if (value.isEmpty())
        {
            return;
        }
        SetValueIfChanged({"texture", "nonSurfaceRgbPolicy"}, value);
    }
}

void QuickConfigPanel::OnModelFillMaterialChanged(const int index)
{
    if (m_loading)
    {
        return;
    }
    const QString value = ComboValue(m_modelFillMaterialCombo, index);
    if (value != QStringLiteral("rgb")
        && value != QStringLiteral("white")
        && value != QStringLiteral("varnish"))
    {
        return;
    }
    SetValueIfChanged({"modelFill", "enabled"}, true);
    SetValueIfChanged({"modelFill", "material"}, value);
    SetValueIfChanged({"modelFill", "emptyAllowedInProduction"}, false);
    SetValueIfChanged({"modelFill", "legacyRgbFallback"}, false);
}

void QuickConfigPanel::OnSupportEnabledChanged(const bool checked)
{
    if (!m_loading)
    {
        SetValueIfChanged({"support", "enabled"}, checked);
        SetValueIfChanged({"materialProcessProfile", "support", "expected"}, checked);
        if (!checked)
        {
            SetValueIfChanged({"support", "mode"}, "none");
            SetValueIfChanged({"support", "internalVoid", "enabled"}, false);
        }
    }
}

void QuickConfigPanel::OnSupportPlacementChanged(const int index)
{
    if (m_loading)
    {
        return;
    }
    const QString placement = ComboValue(m_supportPlacementCombo, index);
    if (placement.isEmpty())
    {
        return;
    }
    QString mode = QStringLiteral("bottom_projection");
    if (placement == QStringLiteral("unsupported_only"))
    {
        mode = QStringLiteral("unsupported_only");
    }
    else if (placement == QStringLiteral("full_vertical_projection"))
    {
        mode = QStringLiteral("full_vertical_projection");
    }
    SetValueIfChanged({"support", "placement"}, placement);
    SetValueIfChanged({"support", "mode"}, mode);
    SetValueIfChanged(
        {"support", "upper", "enabled"},
        placement == QStringLiteral("upper") || placement == QStringLiteral("both"));
    SetValueIfChanged({"support", "upper", "outside"}, "outer_varnish_shell");
}

void QuickConfigPanel::OnInternalVoidEnabledChanged(const bool checked)
{
    if (m_loading)
    {
        return;
    }
    if (checked && !m_supportEnabledCheck->isChecked())
    {
        m_supportEnabledCheck->setChecked(true);
        SetValueIfChanged({"support", "enabled"}, true);
    }
    SetValueIfChanged({"support", "internalVoid", "enabled"}, checked);
    SetValueIfChanged({"support", "internalVoid", "fillRule"}, "all_internal_voids");
}

void QuickConfigPanel::OnInternalVoidMinAreaChanged(const int value)
{
    if (!m_loading)
    {
        SetValueIfChanged({"support", "internalVoid", "minAreaPx"}, value);
    }
}

void QuickConfigPanel::OnBaseProjectionEnabledChanged(const bool checked)
{
    if (m_loading)
    {
        return;
    }
    if (checked && !m_supportEnabledCheck->isChecked())
    {
        m_supportEnabledCheck->setChecked(true);
        SetValueIfChanged({"support", "enabled"}, true);
    }
    SetValueIfChanged(
        {"support", "baseProjection", "enabled"},
        checked);
    SetValueIfChanged(
        {"support", "baseProjection", "source"},
        "max_support_footprint");
}

void QuickConfigPanel::OnBaseProjectionLayerCountChanged(
    const int value)
{
    if (!m_loading)
    {
        SetValueIfChanged(
            {"support", "baseProjection", "layerCount"},
            value);
    }
}

void QuickConfigPanel::OnWhiteEnabledChanged(const bool checked)
{
    if (!m_loading)
    {
        if (checked)
        {
            SetValueIfChanged({"materialPolicy", "enabled"}, true);
        }
        SetValueIfChanged({"materialPolicy", "white", "enabled"}, checked);
        SetValueIfChanged(
            {"materialPolicy", "white", "mode"},
            checked ? QStringLiteral("all_model") : QStringLiteral("disabled"));
        SetValueIfChanged({"materialPolicy", "white", "value"}, 0);
        SetValueIfChanged({"materialProcessProfile", "white", "enabled"}, checked);
        SetValueIfChanged(
            {"materialProcessProfile", "white", "mode"},
            checked ? QStringLiteral("all_model") : QStringLiteral("disabled"));
        SetValueIfChanged({"materialProcessProfile", "white", "coverage"}, QStringLiteral("all_model"));
        SetValueIfChanged({"materialProcessProfile", "white", "value"}, 0);
        SetValueIfChanged({"materialProcessProfile", "validation", "requireWhitePixels"}, checked);
    }
}

void QuickConfigPanel::OnVarnishEnabledChanged(const bool checked)
{
    if (!m_loading)
    {
        if (checked)
        {
            SetValueIfChanged({"materialPolicy", "enabled"}, true);
        }
        SetValueIfChanged({"materialPolicy", "varnish", "enabled"}, checked);
        SetValueIfChanged(
            {"materialPolicy", "varnish", "mode"},
            checked ? QStringLiteral("top_n_layers") : QStringLiteral("disabled"));
        SetValueIfChanged({"materialPolicy", "varnish", "value"}, 0);
        SetValueIfChanged({"materialProcessProfile", "varnish", "enabled"}, checked);
        SetValueIfChanged(
            {"materialProcessProfile", "varnish", "mode"},
            checked ? QStringLiteral("top_n_layers") : QStringLiteral("disabled"));
        SetValueIfChanged({"materialProcessProfile", "varnish", "value"}, 0);
        SetValueIfChanged({"materialProcessProfile", "validation", "requireVarnishPixels"}, checked);
    }
}

void QuickConfigPanel::OnVarnishTopLayersChanged(const int value)
{
    if (!m_loading)
    {
        SetValueIfChanged({"materialPolicy", "varnish", "topLayers"}, value);
        SetValueIfChanged({"materialProcessProfile", "varnish", "topLayers"}, value);
    }
}

void QuickConfigPanel::OnSurfaceVarnishEnabledChanged(const bool checked)
{
    if (m_loading)
    {
        return;
    }

    SetValueIfChanged({"surfaceVarnish", "enabled"}, checked);
    SetValueIfChanged({"surfaceVarnish", "outerSurface"}, true);
    SetValueIfChanged({"surfaceVarnish", "innerSurface"}, true);
    SetValueIfChanged({"surfaceVarnish", "value"}, 0);
    SetValueIfChanged({"surfaceVarnish", "source"}, "explicit");
    if (checked && m_surfaceVarnishThicknessSpin->value() <= 0)
    {
        m_surfaceVarnishThicknessSpin->setValue(1);
        SetValueIfChanged({"surfaceVarnish", "thicknessPx"}, 1);
    }
    if (!checked)
    {
        m_surfaceVarnishThicknessSpin->setValue(0);
        SetValueIfChanged({"surfaceVarnish", "thicknessPx"}, 0);
    }
}

void QuickConfigPanel::OnSurfaceVarnishThicknessChanged(const int value)
{
    if (m_loading)
    {
        return;
    }

    SetValueIfChanged({"surfaceVarnish", "thicknessPx"}, value);
    SetValueIfChanged({"surfaceVarnish", "enabled"}, value > 0);
    SetValueIfChanged({"surfaceVarnish", "outerSurface"}, true);
    SetValueIfChanged({"surfaceVarnish", "innerSurface"}, true);
    SetValueIfChanged({"surfaceVarnish", "value"}, 0);
    SetValueIfChanged({"surfaceVarnish", "source"}, "explicit");
}

void QuickConfigPanel::OnOuterVarnishEnabledChanged(const bool checked)
{
    if (m_loading)
    {
        return;
    }

    SetValueIfChanged({"outerVarnish", "enabled"}, checked);
    SetValueIfChanged({"outerVarnish", "thicknessStepMm"}, 0.01);
    SetValueIfChanged({"outerVarnish", "pixelPitchUm"}, 42.3);
    SetValueIfChanged({"outerVarnish", "allowXYExpansion"}, true);
    SetValueIfChanged({"outerVarnish", "conflictPolicy"}, "varnish_shell_wins");
    SetValueIfChanged({"outerVarnish", "value"}, 0);
    if (checked && m_outerVarnishThicknessSpin->value() <= 0.0)
    {
        m_outerVarnishThicknessSpin->setValue(0.01);
        SetValueIfChanged({"outerVarnish", "thicknessMm"}, 0.01);
    }
    if (!checked)
    {
        m_outerVarnishThicknessSpin->setValue(0.0);
        SetValueIfChanged({"outerVarnish", "thicknessMm"}, 0.0);
    }
}

void QuickConfigPanel::OnOuterVarnishThicknessChanged(const double value)
{
    if (m_loading)
    {
        return;
    }

    SetValueIfChanged({"outerVarnish", "thicknessMm"}, value);
    SetValueIfChanged({"outerVarnish", "enabled"}, value > 0.0);
    SetValueIfChanged({"outerVarnish", "thicknessStepMm"}, 0.01);
    SetValueIfChanged({"outerVarnish", "pixelPitchUm"}, 42.3);
    SetValueIfChanged({"outerVarnish", "allowXYExpansion"}, true);
    SetValueIfChanged({"outerVarnish", "conflictPolicy"}, "varnish_shell_wins");
    SetValueIfChanged({"outerVarnish", "value"}, 0);
}

void QuickConfigPanel::OnPreviewEnabledChanged(const bool checked)
{
    if (!m_loading)
    {
        SetValueIfChanged(
            {"preview", "outputPolicy"},
            checked
                ? QStringLiteral("tiff_native_with_diagnostics")
                : QStringLiteral("tiff_native"));
        SetValueIfChanged({"preview", "enabled"}, checked);
    }
}

void QuickConfigPanel::OnPreviewIntervalChanged(const int value)
{
    if (!m_loading)
    {
        SetValueIfChanged({"preview", "interval"}, value);
    }
}

void QuickConfigPanel::OnOpenVdbEnabledChanged(const bool checked)
{
    if (m_loading)
    {
        return;
    }

    SetValueIfChanged({"experimental", "openvdbPipeline", "enabled"}, checked);
    SetValueIfChanged({"experimental", "openvdbPipeline", "writeProductionRgbwsv"}, false);
    if (checked)
    {
        SetValueIfChanged({"experimental", "openvdbPipeline", "engine"}, "openvdb");
        SetValueIfChanged({"experimental", "openvdbPipeline", "admissionMode"}, "diagnostic_only");
        SetValueIfChanged({"experimental", "openvdbPipeline", "failurePolicy"}, "diagnostic_only");
        SetValueIfChanged({"experimental", "openvdbPipeline", "allowNonProductionOutput"}, true);
    }
}

void QuickConfigPanel::SetValueIfChanged(const QStringList& path, const QJsonValue& value)
{
    if (m_document->value(path) == value)
    {
        return;
    }
    m_document->setValue(path, value);
}

QString QuickConfigPanel::StringValue(const QStringList& path, const QString& fallback) const
{
    const QString value = m_document->value(path).toString();
    return value.isEmpty() ? fallback : value;
}

bool QuickConfigPanel::BoolValue(const QStringList& path, const bool fallback) const
{
    const QJsonValue value = m_document->value(path);
    return value.isBool() ? value.toBool() : fallback;
}

int QuickConfigPanel::IntValue(const QStringList& path, const int fallback) const
{
    const QJsonValue value = m_document->value(path);
    return value.isDouble() ? value.toInt() : fallback;
}

double QuickConfigPanel::DoubleValue(const QStringList& path, const double fallback) const
{
    const QJsonValue value = m_document->value(path);
    return value.isDouble() ? value.toDouble() : fallback;
}

void QuickConfigPanel::UpdateOutputPixelSizeLabel()
{
    constexpr double millimetersPerInch{25.4};
    const double pixelSizeX =
        millimetersPerInch / static_cast<double>(m_outputDpiXSpin->value());
    const double pixelSizeY =
        millimetersPerInch / static_cast<double>(m_outputDpiYSpin->value());
    m_outputPixelSizeLabel->setText(
        QStringLiteral("X %1 mm/px；Y %2 mm/px")
            .arg(pixelSizeX, 0, 'f', 6)
            .arg(pixelSizeY, 0, 'f', 6));
}

void QuickConfigPanel::UpdateNormalizedView()
{
    m_normalizedView->setPlainText(QString::fromUtf8(m_document->document().toJson(QJsonDocument::Indented)));
}
