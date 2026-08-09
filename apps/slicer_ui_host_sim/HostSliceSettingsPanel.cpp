#include "HostSliceSettingsPanel.h"

#include "HostMaterialSettingsPanel.h"
#include "HostSupportSettingsPanel.h"

#include <QCoreApplication>
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
QString DefaultOutputDirectory()
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

    auto* outputRow = new QWidget(processGroup);
    auto* outputLayout = new QHBoxLayout(outputRow);
    outputLayout->setContentsMargins(0, 0, 0, 0);
    m_outputEdit = new QLineEdit(
        QDir::toNativeSeparators(DefaultOutputDirectory()), outputRow);
    m_outputEdit->setObjectName(QStringLiteral("hostSliceOutputEdit"));
    m_outputBrowseButton = new QPushButton(
        QStringLiteral("浏览…"), outputRow);
    m_outputBrowseButton->setObjectName(
        QStringLiteral("hostSliceOutputBrowseButton"));
    outputLayout->addWidget(m_outputEdit, 1);
    outputLayout->addWidget(m_outputBrowseButton);

    processForm->addRow(QStringLiteral("当前 Profile"), m_profileLabel);
    processForm->addRow(QStringLiteral("输出分辨率"), dpiRow);
    processForm->addRow(QStringLiteral("层厚"), m_layerThicknessSpin);
    processForm->addRow(QStringLiteral("输出目录"), outputRow);
    layout->addWidget(processGroup);

    auto* materialGroup = new QGroupBox(
        QStringLiteral("宿主 Profile 材料工艺段"), this);
    auto* materialLayout = new QVBoxLayout(materialGroup);
    materialLayout->setContentsMargins(8, 8, 8, 8);
    m_materialPanel = new HostMaterialSettingsPanel(materialGroup);
    materialLayout->addWidget(m_materialPanel);
    layout->addWidget(materialGroup);

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

    m_profilePreview = new QPlainTextEdit(this);
    m_profilePreview->setObjectName(
        QStringLiteral("hostEffectiveProfilePreview"));
    m_profilePreview->setReadOnly(true);
    m_profilePreview->setPlaceholderText(
        QStringLiteral("导入模型后显示未来切片提交使用的有效 Profile。"));
    layout->addWidget(m_profilePreview, 1);

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
        &HostSliceSettingsPanel::OnSettingsEdited);
    connect(
        m_supportPanel,
        &HostSupportSettingsPanel::SigSettingsChanged,
        this,
        &HostSliceSettingsPanel::OnSettingsEdited);
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
    m_dpiXSpin->setValue(settings.dpix);
    m_dpiYSpin->setValue(settings.dpiy);
    m_layerThicknessSpin->setValue(settings.layerthicknessmm);
    m_outputEdit->setText(settings.outputdirectory);
    m_materialPanel->SetSettings(
        settings.materialstrategy, settings.materialprocess);
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
    settings.materialstrategy = m_materialPanel->Strategy();
    settings.materialprocess = m_materialPanel->Settings();
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
