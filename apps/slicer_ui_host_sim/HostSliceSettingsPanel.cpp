#include "HostSliceSettingsPanel.h"

#include "HostMaterialSettingsPanel.h"
#include "HostProcessPresetCatalog.h"
#include "HostSupportSettingsPanel.h"
#include "HostTextureSettingsPanel.h"

#include <QCoreApplication>
#include <QAbstractItemView>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStandardPaths>
#include <QVBoxLayout>

namespace
{
QString LegacyCompatibleApplicationRoot()
{
    const QDir applicationDirectory(
        QCoreApplication::applicationDirPath());
    if (QFileInfo::exists(applicationDirectory.filePath(
            QStringLiteral("samples/scenarios/slicer_scenarios.json"))))
    {
        return applicationDirectory.absolutePath();
    }
    return QDir::currentPath();
}

QString DefaultOutputDirectory()
{
    const QString sessionName = QStringLiteral("host_")
        + QDateTime::currentDateTime().toString(
            QStringLiteral("yyyyMMdd_HHmmss_zzz"));
    return QDir(LegacyCompatibleApplicationRoot()).filePath(
        QStringLiteral("output/ui_sessions/%1/package").arg(sessionName));
}

QString LegacyDefaultOutputDirectory()
{
    QString root = QStandardPaths::writableLocation(
        QStandardPaths::DocumentsLocation);
    if (root.isEmpty())
    {
        root = QCoreApplication::applicationDirPath();
    }
    return QDir(root).filePath(
        QStringLiteral("SliceSoftHostOutput/package"));
}

int GeometrySamplingIndex(
    const QComboBox* combo,
    const HostGeometrySamplingStrategy strategy)
{
    return combo->findData(
        HostEffectiveProfileBuilder::GeometrySamplingStrategyId(strategy));
}

bool PathsEqual(const QString& left, const QString& right)
{
    return QDir::cleanPath(QFileInfo(left).absoluteFilePath()).compare(
        QDir::cleanPath(QFileInfo(right).absoluteFilePath()),
        Qt::CaseInsensitive) == 0;
}

void ConfigureVolumeSpin(QDoubleSpinBox* spin, const double value)
{
    spin->setRange(1.0, 10000.0);
    spin->setDecimals(3);
    spin->setSingleStep(1.0);
    spin->setSuffix(QStringLiteral(" mm"));
    spin->setValue(value);
}
}

HostSliceSettingsPanel::HostSliceSettingsPanel(QWidget* parent)
    : QWidget(parent)
{
    m_defaultOutputDirectory = DefaultOutputDirectory();
    BuildInterface();
    RefreshPreview();
}

void HostSliceSettingsPanel::BuildInterface()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    auto* processGroup = new QGroupBox(QStringLiteral("切片参数"), this);
    auto* processForm = new QFormLayout(processGroup);
    m_profileLabel = new QLabel(QStringLiteral("未选择"), processGroup);
    m_profileLabel->setObjectName(QStringLiteral("hostSliceProfileLabel"));

    m_processPresetCombo = new QComboBox(processGroup);
    m_processPresetCombo->setObjectName(
        QStringLiteral("hostProcessPresetCombo"));
    m_processPresetCombo->setToolTip(QStringLiteral(
        "快速套用旧版常用生产工艺。套用后仍可在下方材料、纹理和支撑段继续细调。"));
    m_processPresetCombo->addItem(
        QStringLiteral("自定义｜保留当前参数"),
        QStringLiteral("custom"));
    m_processPresetCombo->setItemData(
        0,
        QStringLiteral("不套用预设，保留当前材料、纹理和支撑参数。"),
        Qt::ToolTipRole);
    for (const hostprocesspreset& preset
         : HostProcessPresetCatalog::Presets())
    {
        m_processPresetCombo->addItem(preset.displayname, preset.id);
        m_processPresetCombo->setItemData(
            m_processPresetCombo->count() - 1,
            preset.description,
            Qt::ToolTipRole);
    }
    m_processPresetCombo->view()->setMinimumWidth(560);

    auto* dpiRow = new QWidget(processGroup);
    auto* dpiLayout = new QHBoxLayout(dpiRow);
    dpiLayout->setContentsMargins(0, 0, 0, 0);
    m_dpiXSpin = new QSpinBox(dpiRow);
    m_dpiXSpin->setObjectName(QStringLiteral("hostSliceDpiXSpin"));
    m_dpiXSpin->setRange(72, 2400);
    m_dpiXSpin->setValue(635);
    m_dpiXSpin->setSuffix(QStringLiteral(" dpi X"));
    m_dpiYSpin = new QSpinBox(dpiRow);
    m_dpiYSpin->setObjectName(QStringLiteral("hostSliceDpiYSpin"));
    m_dpiYSpin->setRange(72, 2400);
    m_dpiYSpin->setValue(600);
    m_dpiYSpin->setSuffix(QStringLiteral(" dpi Y"));
    dpiLayout->addWidget(m_dpiXSpin);
    dpiLayout->addWidget(m_dpiYSpin);

    m_layerThicknessSpin = new QDoubleSpinBox(processGroup);
    m_layerThicknessSpin->setObjectName(
        QStringLiteral("hostSliceLayerThicknessSpin"));
    m_layerThicknessSpin->setRange(0.001, 10.0);
    m_layerThicknessSpin->setDecimals(3);
    m_layerThicknessSpin->setSingleStep(0.001);
    m_layerThicknessSpin->setSuffix(QStringLiteral(" mm"));
    m_layerThicknessSpin->setValue(0.038);

    m_geometrySamplingCombo = new QComboBox(processGroup);
    m_geometrySamplingCombo->setObjectName(
        QStringLiteral("hostGeometrySamplingCombo"));
    m_geometrySamplingCombo->addItem(
        QStringLiteral("生产默认｜S0 Legacy 中心采样"),
        HostEffectiveProfileBuilder::GeometrySamplingStrategyId(
            HostGeometrySamplingStrategy::LegacyCenterSample));
    m_geometrySamplingCombo->setItemData(
        0,
        QStringLiteral("现有生产基线；适用于全部已支持 Profile。"),
        Qt::ToolTipRole);
    m_geometrySamplingCombo->addItem(
        QStringLiteral("诊断候选｜S3 层体积 2×2（至少 2/4）"),
        HostEffectiveProfileBuilder::GeometrySamplingStrategyId(
            HostGeometrySamplingStrategy::
                LayerSlabSupersample2x2AtLeastTwoCandidate));
    m_geometrySamplingCombo->setItemData(
        1,
        QStringLiteral(
            "仅限 relief_heightfield 纹理 Profile；不会自动替换生产默认。"),
        Qt::ToolTipRole);

    auto* outputRow = new QWidget(processGroup);
    auto* outputLayout = new QHBoxLayout(outputRow);
    outputLayout->setContentsMargins(0, 0, 0, 0);
    m_outputEdit = new QLineEdit(
        QDir::toNativeSeparators(m_defaultOutputDirectory), outputRow);
    m_outputEdit->setObjectName(QStringLiteral("hostSliceOutputEdit"));
    m_outputBrowseButton = new QPushButton(
        QStringLiteral("浏览…"), outputRow);
    m_outputBrowseButton->setObjectName(
        QStringLiteral("hostSliceOutputBrowseButton"));
    outputLayout->addWidget(m_outputEdit, 1);
    outputLayout->addWidget(m_outputBrowseButton);

    processForm->addRow(QStringLiteral("当前 Profile"), m_profileLabel);
    processForm->addRow(
        QStringLiteral("常用工艺预设"), m_processPresetCombo);
    processForm->addRow(QStringLiteral("输出分辨率"), dpiRow);
    processForm->addRow(QStringLiteral("层厚"), m_layerThicknessSpin);
    processForm->addRow(
        QStringLiteral("几何采样"), m_geometrySamplingCombo);
    processForm->addRow(QStringLiteral("输出目录"), outputRow);
    layout->addWidget(processGroup);

    auto* materialGroup = new QGroupBox(
        QStringLiteral("宿主 Profile 材料工艺段"), this);
    auto* materialLayout = new QVBoxLayout(materialGroup);
    materialLayout->setContentsMargins(8, 8, 8, 8);
    m_materialPanel = new HostMaterialSettingsPanel(materialGroup);
    materialLayout->addWidget(m_materialPanel);
    layout->addWidget(materialGroup);

    auto* textureGroup = new QGroupBox(
        QStringLiteral("宿主 Profile 生产纹理段"), this);
    auto* textureLayout = new QVBoxLayout(textureGroup);
    textureLayout->setContentsMargins(8, 8, 8, 8);
    m_texturePanel = new HostTextureSettingsPanel(textureGroup);
    textureLayout->addWidget(m_texturePanel);
    layout->addWidget(textureGroup);

    auto* volumeGroup = new QGroupBox(
        QStringLiteral("宿主设备构建体积"), this);
    volumeGroup->setToolTip(QStringLiteral(
        "buildVolume 归打印宿主/设备 Profile，切片模块不会提供默认值"));
    auto* volumeForm = new QFormLayout(volumeGroup);
    m_buildWidthSpin = new QDoubleSpinBox(volumeGroup);
    m_buildWidthSpin->setObjectName(
        QStringLiteral("hostBuildVolumeWidthSpin"));
    ConfigureVolumeSpin(m_buildWidthSpin, 230.0);
    m_buildHeightSpin = new QDoubleSpinBox(volumeGroup);
    m_buildHeightSpin->setObjectName(
        QStringLiteral("hostBuildVolumeHeightSpin"));
    ConfigureVolumeSpin(m_buildHeightSpin, 100.0);
    m_buildZSpin = new QDoubleSpinBox(volumeGroup);
    m_buildZSpin->setObjectName(
        QStringLiteral("hostBuildVolumeZSpin"));
    ConfigureVolumeSpin(m_buildZSpin, 60.0);
    volumeForm->addRow(QStringLiteral("X 宽度"), m_buildWidthSpin);
    volumeForm->addRow(QStringLiteral("Y 高度"), m_buildHeightSpin);
    volumeForm->addRow(QStringLiteral("Z 上限"), m_buildZSpin);
    layout->addWidget(volumeGroup);

    auto* supportGroup = new QGroupBox(
        QStringLiteral("宿主 Profile 支撑段"), this);
    auto* supportLayout = new QVBoxLayout(supportGroup);
    supportLayout->setContentsMargins(8, 8, 8, 8);
    m_supportPanel = new HostSupportSettingsPanel(supportGroup);
    supportLayout->addWidget(m_supportPanel);
    layout->addWidget(supportGroup);

    m_validationLabel = new QLabel(this);
    m_validationLabel->setObjectName(
        QStringLiteral("hostSliceValidationLabel"));
    m_validationLabel->setWordWrap(true);
    layout->addWidget(m_validationLabel);

    m_textureWhitePreflightLabel = new QLabel(
        QStringLiteral("纹理白区预检：等待导入带纹理模型。"), this);
    m_textureWhitePreflightLabel->setObjectName(
        QStringLiteral("hostTextureWhitePreflightLabel"));
    m_textureWhitePreflightLabel->setWordWrap(true);
    layout->addWidget(m_textureWhitePreflightLabel);

    m_profilePreview = new QPlainTextEdit(this);
    m_profilePreview->setObjectName(
        QStringLiteral("hostEffectiveProfilePreview"));
    m_profilePreview->setReadOnly(true);
    m_profilePreview->setPlaceholderText(
        QStringLiteral("导入模型后显示未来切片提交使用的有效 Profile。"));
    layout->addWidget(m_profilePreview, 1);

    connect(
        m_processPresetCombo,
        qOverload<int>(&QComboBox::currentIndexChanged),
        this,
        &HostSliceSettingsPanel::OnProcessPresetChanged);
    connect(
        m_outputBrowseButton,
        &QPushButton::clicked,
        this,
        &HostSliceSettingsPanel::OnBrowseOutput);
    connect(
        m_dpiXSpin,
        qOverload<int>(&QSpinBox::valueChanged),
        this,
        &HostSliceSettingsPanel::OnSettingsEdited);
    connect(
        m_dpiYSpin,
        qOverload<int>(&QSpinBox::valueChanged),
        this,
        &HostSliceSettingsPanel::OnSettingsEdited);
    connect(
        m_layerThicknessSpin,
        qOverload<double>(&QDoubleSpinBox::valueChanged),
        this,
        &HostSliceSettingsPanel::OnSettingsEdited);
    connect(
        m_geometrySamplingCombo,
        qOverload<int>(&QComboBox::currentIndexChanged),
        this,
        &HostSliceSettingsPanel::OnSettingsEdited);
    connect(
        m_outputEdit,
        &QLineEdit::textChanged,
        this,
        &HostSliceSettingsPanel::OnSettingsEdited);
    for (QDoubleSpinBox* spin : {
             m_buildWidthSpin, m_buildHeightSpin, m_buildZSpin})
    {
        connect(
            spin,
            qOverload<double>(&QDoubleSpinBox::valueChanged),
            this,
            &HostSliceSettingsPanel::OnSettingsEdited);
    }
    connect(
        m_materialPanel,
        &HostMaterialSettingsPanel::SigSettingsChanged,
        this,
        &HostSliceSettingsPanel::OnProcessSettingsEdited);
    connect(
        m_texturePanel,
        &HostTextureSettingsPanel::SigSettingsChanged,
        this,
        &HostSliceSettingsPanel::OnProcessSettingsEdited);
    connect(
        m_supportPanel,
        &HostSupportSettingsPanel::SigSettingsChanged,
        this,
        &HostSliceSettingsPanel::OnProcessSettingsEdited);
}

void HostSliceSettingsPanel::SetSelectedProfileId(
    const QString& profileId,
    const bool supportsSlice)
{
    m_profileId = profileId;
    m_profileSupportsSlice = supportsSlice;
    m_profileLabel->setText(
        profileId.isEmpty() ? QStringLiteral("未选择") : profileId);
    RefreshPreview();
}

void HostSliceSettingsPanel::SetModelPath(const QString& modelPath)
{
    m_modelPath = modelPath;
    RefreshPreview();
}

void HostSliceSettingsPanel::SetSceneAuthority(
    const bool bound,
    const QString& profileId,
    const hostbuildvolume& buildVolume)
{
    m_sceneBound = bound;
    m_sceneProfileId = profileId;
    m_sceneBuildVolume = buildVolume;
    RefreshPreview();
}

void HostSliceSettingsPanel::SetPersistentSettings(
    const hostslicesettings& settings)
{
    const QSignalBlocker dpiXBlocker(m_dpiXSpin);
    const QSignalBlocker dpiYBlocker(m_dpiYSpin);
    const QSignalBlocker layerBlocker(m_layerThicknessSpin);
    const QSignalBlocker outputBlocker(m_outputEdit);
    const QSignalBlocker widthBlocker(m_buildWidthSpin);
    const QSignalBlocker heightBlocker(m_buildHeightSpin);
    const QSignalBlocker zBlocker(m_buildZSpin);
    const QSignalBlocker presetBlocker(m_processPresetCombo);
    const QSignalBlocker samplingBlocker(m_geometrySamplingCombo);
    m_dpiXSpin->setValue(settings.dpix);
    m_dpiYSpin->setValue(settings.dpiy);
    m_layerThicknessSpin->setValue(settings.layerthicknessmm);
    const QString persistedOutput = settings.outputdirectory.trimmed();
    m_outputEdit->setText(
        persistedOutput.isEmpty()
            || PathsEqual(
                persistedOutput,
                LegacyDefaultOutputDirectory())
            ? QDir::toNativeSeparators(m_defaultOutputDirectory)
            : persistedOutput);
    m_processPresetCombo->setCurrentIndex(0);
    const int samplingIndex = GeometrySamplingIndex(
        m_geometrySamplingCombo, settings.geometrysamplingstrategy);
    m_geometrySamplingCombo->setCurrentIndex(
        samplingIndex >= 0 ? samplingIndex : 0);
    m_materialPanel->SetSettings(
        settings.materialstrategy, settings.materialprocess);
    m_texturePanel->SetSettings(settings.texture);
    m_buildWidthSpin->setValue(settings.buildvolume.widthmm);
    m_buildHeightSpin->setValue(settings.buildvolume.heightmm);
    m_buildZSpin->setValue(settings.buildvolume.zlimitmm);
    m_supportPanel->SetSettings(settings.support);
    RefreshPreview();
}

hostslicesettings HostSliceSettingsPanel::Settings() const
{
    hostslicesettings settings;
    settings.profileid = m_profileId;
    settings.modelpath = m_modelPath;
    settings.modelformat = QFileInfo(m_modelPath).suffix().toLower();
    settings.outputdirectory = m_outputEdit->text().trimmed();
    settings.dpix = m_dpiXSpin->value();
    settings.dpiy = m_dpiYSpin->value();
    settings.layerthicknessmm = m_layerThicknessSpin->value();
    const QString samplingId = m_geometrySamplingCombo->currentData().toString();
    settings.geometrysamplingstrategy = samplingId
            == HostEffectiveProfileBuilder::GeometrySamplingStrategyId(
                HostGeometrySamplingStrategy::
                    LayerSlabSupersample2x2AtLeastTwoCandidate)
        ? HostGeometrySamplingStrategy::
            LayerSlabSupersample2x2AtLeastTwoCandidate
        : HostGeometrySamplingStrategy::LegacyCenterSample;
    settings.materialstrategy = m_materialPanel->Strategy();
    settings.materialprocess = m_materialPanel->Settings();
    settings.texture = m_texturePanel->Settings();
    settings.buildvolume.widthmm = m_buildWidthSpin->value();
    settings.buildvolume.heightmm = m_buildHeightSpin->value();
    settings.buildvolume.zlimitmm = m_buildZSpin->value();
    settings.buildvolume.origin = QStringLiteral("lower_left");
    settings.buildvolume.xdirection = QStringLiteral("positive");
    settings.buildvolume.ydirection = QStringLiteral("positive");
    settings.support = m_supportPanel->Settings();
    return settings;
}

bool HostSliceSettingsPanel::IsReady() const
{
    return !m_effectiveProfile.profile.isEmpty();
}

hosteffectiveprofile HostSliceSettingsPanel::EffectiveProfile() const
{
    return m_effectiveProfile;
}

void HostSliceSettingsPanel::SetTextureWhitePreflightStatus(
    const QString& message,
    const bool warning)
{
    m_textureWhitePreflightLabel->setText(message);
    m_textureWhitePreflightLabel->setStyleSheet(
        warning ? QStringLiteral("color: #9c4f00;") : QString{});
}

void HostSliceSettingsPanel::OnBrowseOutput()
{
    const QString directory = QFileDialog::getExistingDirectory(
        this,
        QStringLiteral("选择切片包输出目录"),
        m_outputEdit->text());
    if (!directory.isEmpty())
    {
        m_outputEdit->setText(QDir::toNativeSeparators(directory));
    }
}

void HostSliceSettingsPanel::OnProcessPresetChanged(const int index)
{
    if (index <= 0)
    {
        return;
    }

    const QString presetId = m_processPresetCombo->itemData(index).toString();
    hostprocesspreset preset;
    if (!HostProcessPresetCatalog::Resolve(presetId, &preset))
    {
        return;
    }

    m_applyingProcessPreset = true;
    m_materialPanel->SetSettings(
        preset.materialstrategy, preset.materialprocess);
    m_texturePanel->SetSettings(preset.texture);
    m_supportPanel->SetSettings(preset.support);
    m_applyingProcessPreset = false;
    OnSettingsEdited();
}

void HostSliceSettingsPanel::OnProcessSettingsEdited()
{
    if (!m_applyingProcessPreset)
    {
        const QSignalBlocker presetBlocker(m_processPresetCombo);
        m_processPresetCombo->setCurrentIndex(0);
    }
    OnSettingsEdited();
}

void HostSliceSettingsPanel::OnSettingsEdited()
{
    RefreshPreview();
    emit SigSettingsChanged();
}

bool HostSliceSettingsPanel::ValidateSceneBinding(QString* error) const
{
    if (!m_sceneBound)
    {
        return true;
    }
    const hostslicesettings settings = Settings();
    if (settings.profileid != m_sceneProfileId
        || !HostEffectiveProfileBuilder::BuildVolumesEqual(
            settings.buildvolume, m_sceneBuildVolume))
    {
        if (error != nullptr)
        {
            *error = QStringLiteral(
                "Profile 或设备构建体积已与当前场景绑定值不同；请新建场景后生效。");
        }
        return false;
    }
    return true;
}

void HostSliceSettingsPanel::RefreshPreview()
{
    m_effectiveProfile = {};
    if (!m_profileSupportsSlice)
    {
        m_validationLabel->setText(QStringLiteral(
            "当前 Profile 不包含 slice.rgbwsv，仅可用于包检查。"));
        m_profilePreview->clear();
        return;
    }
    if (m_modelPath.isEmpty())
    {
        m_validationLabel->setText(
            QStringLiteral("参数草稿已就绪；等待导入 OBJ/3MF/STL 模型。"));
        m_profilePreview->clear();
        return;
    }

    QString error;
    if (!ValidateSceneBinding(&error)
        || !HostEffectiveProfileBuilder::Build(
            Settings(), &m_effectiveProfile, &error))
    {
        m_validationLabel->setText(
            QStringLiteral("配置不可提交：%1").arg(error));
        m_profilePreview->clear();
        return;
    }
    m_validationLabel->setText(QStringLiteral(
        "有效 Profile 已通过宿主校验；参数编辑期间未调用切片模块。"));
    m_profilePreview->setPlainText(QString::fromUtf8(
        QJsonDocument(m_effectiveProfile.profile).toJson(
            QJsonDocument::Indented)));
}
