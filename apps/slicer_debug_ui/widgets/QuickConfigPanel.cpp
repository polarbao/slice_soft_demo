#include "QuickConfigPanel.h"

#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
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

}  // namespace

QuickConfigPanel::QuickConfigPanel(ConfigDocument* document, QWidget* parent)
    : QWidget(parent)
    , m_document(document)
{
    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout();

    m_modelPathEdit = MakePathEdit(this);
    auto* modelBrowseButton = MakeButton("...", this);
    auto* modelPathRow = new QHBoxLayout();
    modelPathRow->addWidget(m_modelPathEdit, 1);
    modelPathRow->addWidget(modelBrowseButton);
    form->addRow("模型文件", modelPathRow);

    m_outputDirEdit = MakePathEdit(this);
    auto* outputBrowseButton = MakeButton("...", this);
    auto* outputDirRow = new QHBoxLayout();
    outputDirRow->addWidget(m_outputDirEdit, 1);
    outputDirRow->addWidget(outputBrowseButton);
    form->addRow("输出目录", outputDirRow);

    m_layerHeightSpin = new QDoubleSpinBox(this);
    m_layerHeightSpin->setRange(0.001, 1.0);
    m_layerHeightSpin->setDecimals(4);
    m_layerHeightSpin->setSingleStep(0.005);
    m_layerHeightSpin->setSuffix(" mm");
    form->addRow("层高", m_layerHeightSpin);

    m_texturePolicyCombo = new QComboBox(this);
    m_texturePolicyCombo->setEditable(true);
    m_texturePolicyCombo->addItems({"top_surface_band", "solid_volume_from_top_surface", "solid_volume", "surface_shell_from_sdf", "disabled"});
    form->addRow("纹理策略", m_texturePolicyCombo);

    m_supportEnabledCheck = new QCheckBox("启用支撑", this);
    m_whiteEnabledCheck = new QCheckBox("启用白墨", this);
    m_varnishEnabledCheck = new QCheckBox("启用光油", this);
    m_previewEnabledCheck = new QCheckBox("生成预览", this);
    m_openVdbEnabledCheck = new QCheckBox("启用 OpenVDB 实验管线", this);
    form->addRow(m_supportEnabledCheck);
    form->addRow(m_whiteEnabledCheck);
    form->addRow(m_varnishEnabledCheck);

    m_varnishTopLayersSpin = new QSpinBox(this);
    m_varnishTopLayersSpin->setRange(0, 100000);
    form->addRow("光油顶部层数", m_varnishTopLayersSpin);

    form->addRow(m_previewEnabledCheck);
    m_previewIntervalSpin = new QSpinBox(this);
    m_previewIntervalSpin->setRange(1, 100000);
    form->addRow("预览间隔", m_previewIntervalSpin);
    form->addRow(m_openVdbEnabledCheck);
    layout->addLayout(form);

    m_normalizedView = new QPlainTextEdit(this);
    m_normalizedView->setReadOnly(true);
    m_normalizedView->setMinimumHeight(180);
    layout->addWidget(m_normalizedView, 1);

    connect(modelBrowseButton, &QPushButton::clicked, this, &QuickConfigPanel::OnBrowseModel);
    connect(outputBrowseButton, &QPushButton::clicked, this, &QuickConfigPanel::OnBrowseOutput);
    connect(m_modelPathEdit, &QLineEdit::editingFinished, this, &QuickConfigPanel::OnModelPathEdited);
    connect(m_outputDirEdit, &QLineEdit::editingFinished, this, &QuickConfigPanel::OnOutputDirEdited);
    connect(m_layerHeightSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &QuickConfigPanel::OnLayerHeightChanged);
    connect(m_texturePolicyCombo, &QComboBox::currentTextChanged, this, &QuickConfigPanel::OnTexturePolicyChanged);
    connect(m_supportEnabledCheck, &QCheckBox::toggled, this, &QuickConfigPanel::OnSupportEnabledChanged);
    connect(m_whiteEnabledCheck, &QCheckBox::toggled, this, &QuickConfigPanel::OnWhiteEnabledChanged);
    connect(m_varnishEnabledCheck, &QCheckBox::toggled, this, &QuickConfigPanel::OnVarnishEnabledChanged);
    connect(m_varnishTopLayersSpin, qOverload<int>(&QSpinBox::valueChanged), this, &QuickConfigPanel::OnVarnishTopLayersChanged);
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
    m_texturePolicyCombo->setCurrentText(StringValue({"texture", "applyMode"}, "top_surface_band"));
    m_supportEnabledCheck->setChecked(BoolValue({"support", "enabled"}, true));
    m_whiteEnabledCheck->setChecked(BoolValue({"materialPolicy", "white", "enabled"}, false));
    m_varnishEnabledCheck->setChecked(BoolValue({"materialPolicy", "varnish", "enabled"}, false));
    m_varnishTopLayersSpin->setValue(IntValue({"materialPolicy", "varnish", "topLayers"}, 0));
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

void QuickConfigPanel::OnTexturePolicyChanged(const QString& value)
{
    if (!m_loading)
    {
        SetValueIfChanged({"texture", "applyMode"}, value);
        if (value == "top_surface_band")
        {
            SetValueIfChanged({"texture", "topSurfaceLayers"}, 50);
        }
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
