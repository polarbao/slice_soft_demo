#include "QuickConfigPanel.h"

#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonDocument>
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

}  // namespace

QuickConfigPanel::QuickConfigPanel(ConfigDocument* document, QWidget* parent)
    : QWidget(parent)
    , m_document(document)
{
    auto* layout = new QVBoxLayout(this);
    auto* basicGroup = new QGroupBox("基础", this);
    basicGroup->setToolTip("基础输入输出：选择模型、输出目录和层高。");
    auto* basicForm = new QFormLayout(basicGroup);

    m_modelPathEdit = MakePathEdit(this);
    m_modelPathEdit->setToolTip("待切片模型路径。OBJ 贴图通常放在 OBJ/MTL 同级目录。");
    auto* modelBrowseButton = MakeButton("...", this);
    modelBrowseButton->setToolTip("选择 OBJ、STL 或 3MF 模型文件。");
    auto* modelPathRow = new QHBoxLayout();
    modelPathRow->addWidget(m_modelPathEdit, 1);
    modelPathRow->addWidget(modelBrowseButton);
    basicForm->addRow("模型文件", modelPathRow);

    m_outputDirEdit = MakePathEdit(this);
    m_outputDirEdit->setToolTip("输出 RGBWSV package 的目录。");
    auto* outputBrowseButton = MakeButton("...", this);
    outputBrowseButton->setToolTip("选择输出目录。");
    auto* outputDirRow = new QHBoxLayout();
    outputDirRow->addWidget(m_outputDirEdit, 1);
    outputDirRow->addWidget(outputBrowseButton);
    basicForm->addRow("输出目录", outputDirRow);

    m_layerHeightSpin = new QDoubleSpinBox(this);
    m_layerHeightSpin->setRange(0.001, 1.0);
    m_layerHeightSpin->setDecimals(4);
    m_layerHeightSpin->setSingleStep(0.005);
    m_layerHeightSpin->setSuffix(" mm");
    m_layerHeightSpin->setToolTip("切片层厚，直接影响层数、输出文件数量和切片耗时。");
    basicForm->addRow("层高", m_layerHeightSpin);

    auto* materialGroup = new QGroupBox("材料", this);
    materialGroup->setToolTip("常用材料设置：控制 RGB 纹理、非表面 RGB、白墨和光油。");
    auto* materialForm = new QFormLayout(materialGroup);
    m_texturePolicyCombo = new QComboBox(this);
    AddComboOption(m_texturePolicyCombo, "顶面纹理带", "top_surface_band");
    AddComboOption(m_texturePolicyCombo, "顶面纹理投影到实体", "solid_volume_from_top_surface");
    AddComboOption(m_texturePolicyCombo, "实体填充", "solid_volume");
    AddComboOption(m_texturePolicyCombo, "SDF 表面壳层", "surface_shell_from_sdf");
    AddComboOption(m_texturePolicyCombo, "禁用纹理", "disabled");
    m_texturePolicyCombo->setToolTip("控制贴图颜色如何写入模型：通常全彩 OBJ 选“顶面纹理带”；OpenVDB 壳层实验选“SDF 表面壳层”。");
    materialForm->addRow("纹理策略", m_texturePolicyCombo);

    m_nonSurfaceRgbPolicyCombo = new QComboBox(this);
    AddComboOption(m_nonSurfaceRgbPolicyCombo, "使用模型材料", "model_material");
    AddComboOption(m_nonSurfaceRgbPolicyCombo, "视为空白", "empty");
    AddComboOption(m_nonSurfaceRgbPolicyCombo, "使用备用 RGB", "fallback_rgb");
    AddComboOption(m_nonSurfaceRgbPolicyCombo, "交给材料策略", "material_policy");
    m_nonSurfaceRgbPolicyCombo->setToolTip("控制非表面纹理带的模型实体区域如何写 RGB：可作为模型填充、空白、备用色或交给材料策略处理。");
    materialForm->addRow("非表面 RGB", m_nonSurfaceRgbPolicyCombo);

    m_supportEnabledCheck = new QCheckBox("启用支撑", this);
    m_whiteEnabledCheck = new QCheckBox("启用白墨", this);
    m_varnishEnabledCheck = new QCheckBox("启用顶部光油策略", this);
    m_previewEnabledCheck = new QCheckBox("生成预览", this);
    m_openVdbEnabledCheck = new QCheckBox("启用 OpenVDB 实验管线", this);
    m_supportEnabledCheck->setToolTip("启用后生成 S 通道支撑材料，具体形态在“支撑”页设置。");
    m_whiteEnabledCheck->setToolTip("启用后按材料策略写入 W 通道白墨。");
    m_varnishEnabledCheck->setToolTip("旧材料策略光油：按顶部 N 层或材料策略写 V 通道；不等同于外侧光油壳层。");
    m_previewEnabledCheck->setToolTip("启用后按间隔输出 preview PNG/PPM，便于 UI 预览，但会增加保存耗时。");
    m_openVdbEnabledCheck->setToolTip("实验开关：当前用于 OpenVDB 诊断/候选流程，不代表默认生产切片路径。");
    materialForm->addRow(m_whiteEnabledCheck);
    materialForm->addRow(m_varnishEnabledCheck);

    m_varnishTopLayersSpin = new QSpinBox(this);
    m_varnishTopLayersSpin->setRange(0, 100000);
    m_varnishTopLayersSpin->setToolTip("光油覆盖顶部 N 层；0 表示不按顶部层数生成光油。");
    materialForm->addRow("光油顶部层数", m_varnishTopLayersSpin);

    m_surfaceVarnishEnabledCheck = new QCheckBox("启用表面光油", this);
    m_surfaceVarnishEnabledCheck->setToolTip("写在模型表面或内表面像素上的 V 通道光油；不会扩张模型 XY 尺寸。");
    materialForm->addRow(m_surfaceVarnishEnabledCheck);

    m_surfaceVarnishThicknessSpin = new QSpinBox(this);
    m_surfaceVarnishThicknessSpin->setRange(0, 100);
    m_surfaceVarnishThicknessSpin->setSuffix(" px");
    m_surfaceVarnishThicknessSpin->setToolTip("表面光油像素厚度；0 表示关闭表面光油。");
    materialForm->addRow("表面光油厚度", m_surfaceVarnishThicknessSpin);

    m_outerVarnishEnabledCheck = new QCheckBox("启用外侧光油壳层", this);
    m_outerVarnishEnabledCheck->setToolTip("在模型外轮廓之外扩张生成 V 通道光油壳层；默认关闭。");
    materialForm->addRow(m_outerVarnishEnabledCheck);

    m_outerVarnishThicknessSpin = new QDoubleSpinBox(this);
    m_outerVarnishThicknessSpin->setRange(0.0, 10.0);
    m_outerVarnishThicknessSpin->setDecimals(2);
    m_outerVarnishThicknessSpin->setSingleStep(0.01);
    m_outerVarnishThicknessSpin->setSuffix(" mm");
    m_outerVarnishThicknessSpin->setToolTip("外侧光油壳层厚度，按 42.3um/px 或配置中的 pixelPitchUm 换算为像素扩张；0 表示不生成。");
    materialForm->addRow("外侧光油厚度", m_outerVarnishThicknessSpin);

    auto* supportGroup = new QGroupBox("支撑", this);
    supportGroup->setToolTip("支撑总开关；细节请切换到“支撑”页。");
    auto* supportForm = new QFormLayout(supportGroup);
    supportForm->addRow(m_supportEnabledCheck);

    auto* previewGroup = new QGroupBox("预览", this);
    previewGroup->setToolTip("控制调试预览图片输出。生产 TIFF 输出不依赖 preview。");
    auto* previewForm = new QFormLayout(previewGroup);
    previewForm->addRow(m_previewEnabledCheck);
    m_previewIntervalSpin = new QSpinBox(this);
    m_previewIntervalSpin->setRange(1, 100000);
    m_previewIntervalSpin->setToolTip("每隔多少层保存一次 preview 图片。值越小图片越多，保存耗时越高。");
    previewForm->addRow("预览间隔", m_previewIntervalSpin);

    auto* experimentalGroup = new QGroupBox("实验", this);
    experimentalGroup->setToolTip("实验能力入口。OpenVDB 仍是候选/诊断路径，正式生产输出以非 OpenVDB 路径为准。");
    auto* experimentalForm = new QFormLayout(experimentalGroup);
    experimentalForm->addRow(m_openVdbEnabledCheck);

    layout->addWidget(basicGroup);
    layout->addWidget(materialGroup);
    layout->addWidget(supportGroup);
    layout->addWidget(previewGroup);
    layout->addWidget(experimentalGroup);

    m_normalizedView = new QPlainTextEdit(this);
    m_normalizedView->setReadOnly(true);
    m_normalizedView->setMinimumHeight(180);
    layout->addWidget(m_normalizedView, 1);

    connect(modelBrowseButton, &QPushButton::clicked, this, &QuickConfigPanel::OnBrowseModel);
    connect(outputBrowseButton, &QPushButton::clicked, this, &QuickConfigPanel::OnBrowseOutput);
    connect(m_modelPathEdit, &QLineEdit::editingFinished, this, &QuickConfigPanel::OnModelPathEdited);
    connect(m_outputDirEdit, &QLineEdit::editingFinished, this, &QuickConfigPanel::OnOutputDirEdited);
    connect(m_layerHeightSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &QuickConfigPanel::OnLayerHeightChanged);
    connect(m_texturePolicyCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &QuickConfigPanel::OnTexturePolicyChanged);
    connect(m_nonSurfaceRgbPolicyCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &QuickConfigPanel::OnNonSurfaceRgbPolicyChanged);
    connect(m_supportEnabledCheck, &QCheckBox::toggled, this, &QuickConfigPanel::OnSupportEnabledChanged);
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
    m_layerHeightSpin->setValue(DoubleValue({"output", "layerThicknessMm"}, 0.01));
    SetComboValue(m_texturePolicyCombo, StringValue({"texture", "applyMode"}, "top_surface_band"));
    SetComboValue(m_nonSurfaceRgbPolicyCombo, StringValue({"texture", "nonSurfaceRgbPolicy"}, "model_material"));
    m_supportEnabledCheck->setChecked(BoolValue({"support", "enabled"}, true));
    m_whiteEnabledCheck->setChecked(BoolValue({"materialPolicy", "white", "enabled"}, false));
    m_varnishEnabledCheck->setChecked(BoolValue({"materialPolicy", "varnish", "enabled"}, false));
    m_varnishTopLayersSpin->setValue(IntValue({"materialPolicy", "varnish", "topLayers"}, 0));
    m_surfaceVarnishEnabledCheck->setChecked(BoolValue({"surfaceVarnish", "enabled"}, false));
    m_surfaceVarnishThicknessSpin->setValue(IntValue({"surfaceVarnish", "thicknessPx"}, 0));
    m_outerVarnishEnabledCheck->setChecked(BoolValue({"outerVarnish", "enabled"}, false));
    m_outerVarnishThicknessSpin->setValue(DoubleValue({"outerVarnish", "thicknessMm"}, 0.0));
    m_previewEnabledCheck->setChecked(BoolValue({"preview", "enabled"}, false));
    m_previewIntervalSpin->setValue(IntValue({"preview", "interval"}, 1));
    m_openVdbEnabledCheck->setChecked(BoolValue({"experimental", "openvdbPipeline", "enabled"}, false));
    UpdateNormalizedView();
    m_loading = false;
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

void QuickConfigPanel::OnSupportEnabledChanged(const bool checked)
{
    if (!m_loading)
    {
        SetValueIfChanged({"support", "enabled"}, checked);
        SetValueIfChanged({"materialProcessProfile", "support", "expected"}, checked);
    }
}

void QuickConfigPanel::OnWhiteEnabledChanged(const bool checked)
{
    if (!m_loading)
    {
        SetValueIfChanged({"materialPolicy", "white", "enabled"}, checked);
        SetValueIfChanged({"materialProcessProfile", "white", "enabled"}, checked);
    }
}

void QuickConfigPanel::OnVarnishEnabledChanged(const bool checked)
{
    if (!m_loading)
    {
        SetValueIfChanged({"materialPolicy", "varnish", "enabled"}, checked);
        SetValueIfChanged({"materialProcessProfile", "varnish", "enabled"}, checked);
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

void QuickConfigPanel::UpdateNormalizedView()
{
    m_normalizedView->setPlainText(QString::fromUtf8(m_document->document().toJson(QJsonDocument::Indented)));
}
