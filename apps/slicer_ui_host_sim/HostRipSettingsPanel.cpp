#include "HostRipSettingsPanel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFormLayout>
#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStyle>
#include <QVBoxLayout>

HostRipSettingsPanel::HostRipSettingsPanel(QWidget* parent)
    : QWidget(parent)
{
    BuildInterface();
    SetSettings(HostRipSettingsStore::Defaults());
}

void HostRipSettingsPanel::BuildInterface()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(8);

    auto* processingGroup = new QGroupBox(
        QStringLiteral("RIP 配置"), this);
    auto* form = new QFormLayout(processingGroup);
    m_autoCheck = new QCheckBox(QStringLiteral("切片完成后自动处理"), processingGroup);
    m_autoCheck->setObjectName(QStringLiteral("hostRipAutoAfterSliceCheck"));
    m_intentCombo = new QComboBox(processingGroup);
    m_intentCombo->setObjectName(QStringLiteral("hostRipIntentCombo"));
    m_intentCombo->addItem(QStringLiteral("感知"), 0);
    m_intentCombo->addItem(QStringLiteral("相对色度"), 1);
    m_intentCombo->addItem(QStringLiteral("饱和度"), 2);
    m_intentCombo->addItem(QStringLiteral("绝对色度"), 3);
    m_transparentCombo = new QComboBox(processingGroup);
    m_transparentCombo->setObjectName(QStringLiteral("hostRipTransparentModeCombo"));
    m_transparentCombo->addItem(QStringLiteral("透明"), 0);
    m_transparentCombo->addItem(QStringLiteral("不透"), 1);
    m_transparentCombo->addItem(QStringLiteral("肤色"), 2);
    m_transparentCombo->addItem(QStringLiteral("白色 30"), 3);
    m_transparentCombo->addItem(QStringLiteral("白色 50"), 4);
    m_colorModeCombo = new QComboBox(processingGroup);
    m_colorModeCombo->setObjectName(QStringLiteral("hostRipColorModeCombo"));
    m_colorModeCombo->addItem(QStringLiteral("默认模式"), 0);
    m_inputIccCombo = new QComboBox(processingGroup);
    m_inputIccCombo->setObjectName(QStringLiteral("hostRipInputIccCombo"));
    m_inputIccCombo->addItem(
        QStringLiteral("CIE RGB"), QStringLiteral("CmykFiles/CIERGB.icc"));
    m_outputIccCombo = new QComboBox(processingGroup);
    m_outputIccCombo->setObjectName(QStringLiteral("hostRipOutputIccCombo"));
    m_outputIccCombo->addItem(
        QStringLiteral("CMYK"), QStringLiteral("CmykFiles/CMYK.icc"));
    m_outputIccCombo->addItem(
        QStringLiteral("Japan Color 2001 Coated"),
        QStringLiteral("CmykFiles/JapanColor2001Coated.icc"));
    m_continueCheck = new QCheckBox(
        QStringLiteral("单层失败后继续"), processingGroup);
    m_continueCheck->setObjectName(QStringLiteral("hostRipContinueCheck"));
    m_grayBitsCombo = new QComboBox(processingGroup);
    m_grayBitsCombo->setObjectName(QStringLiteral("hostRipGrayBitsCombo"));
    m_grayBitsCombo->addItem(QStringLiteral("2 bit（S2 参考阈值）"), 2);
    m_grayBitsCombo->addItem(QStringLiteral("1 bit（S2 参考阈值）"), 1);
    m_outputValidationCombo = new QComboBox(processingGroup);
    m_outputValidationCombo->setObjectName(
        QStringLiteral("hostRipOutputValidationCombo"));
    m_outputValidationCombo->addItem(
        QStringLiteral("严格 S2（可发布）"), QStringLiteral("strict_s2"));
    m_outputValidationCombo->addItem(
        QStringLiteral("诊断保存（不可打印）"),
        QStringLiteral("diagnostic_unvalidated"));
    m_timeoutSpin = new QSpinBox(processingGroup);
    m_timeoutSpin->setObjectName(QStringLiteral("hostRipTimeoutSpin"));
    m_timeoutSpin->setRange(1, 86400);
    m_timeoutSpin->setSuffix(QStringLiteral(" 秒"));
    form->addRow(m_autoCheck);
    form->addRow(QStringLiteral("渲染意图"), m_intentCombo);
    form->addRow(QStringLiteral("RIP 颜色模式"), m_transparentCombo);
    form->addRow(QStringLiteral("纹理/浮雕模式"), m_colorModeCombo);
    form->addRow(QStringLiteral("输入 ICC"), m_inputIccCombo);
    form->addRow(QStringLiteral("输出 ICC"), m_outputIccCombo);
    form->addRow(QStringLiteral("输出验证"), m_outputValidationCombo);
    form->addRow(QStringLiteral("设备灰阶"), m_grayBitsCombo);
    form->addRow(QStringLiteral("超时"), m_timeoutSpin);
    form->addRow(m_continueCheck);
    layout->addWidget(processingGroup);

    auto* pathsGroup = new QGroupBox(QStringLiteral("路径"), this);
    auto* pathsForm = new QFormLayout(pathsGroup);
    m_modulePathEdit = new QLineEdit(pathsGroup);
    m_modulePathEdit->setObjectName(QStringLiteral("hostRipModulePath"));
    m_modulePathEdit->setReadOnly(true);
    m_inputPathEdit = new QLineEdit(pathsGroup);
    m_inputPathEdit->setObjectName(QStringLiteral("hostRipInputPath"));
    m_inputPathEdit->setReadOnly(true);
    m_outputPathEdit = new QLineEdit(pathsGroup);
    m_outputPathEdit->setObjectName(QStringLiteral("hostRipOutputPath"));
    m_outputPathEdit->setReadOnly(true);
    pathsForm->addRow(QStringLiteral("模块"), m_modulePathEdit);
    pathsForm->addRow(QStringLiteral("切片"), m_inputPathEdit);
    pathsForm->addRow(QStringLiteral("输出"), m_outputPathEdit);
    layout->addWidget(pathsGroup);

    m_runtimeStatusLabel = new QLabel(this);
    m_runtimeStatusLabel->setObjectName(QStringLiteral("hostRipRuntimeStatus"));
    m_runtimeStatusLabel->setWordWrap(true);
    layout->addWidget(m_runtimeStatusLabel);
    m_jobStatusLabel = new QLabel(QStringLiteral("RIP 未运行"), this);
    m_jobStatusLabel->setObjectName(QStringLiteral("hostRipJobStatus"));
    m_jobStatusLabel->setWordWrap(true);
    m_jobStatusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_jobStatusLabel);

    auto* actions = new QHBoxLayout();
    m_runButton = new QPushButton(QStringLiteral("运行 RIP"), this);
    m_runButton->setObjectName(QStringLiteral("hostRipRunButton"));
    m_runButton->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    m_cancelButton = new QPushButton(QStringLiteral("取消"), this);
    m_cancelButton->setObjectName(QStringLiteral("hostRipCancelButton"));
    m_cancelButton->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
    m_openButton = new QPushButton(QStringLiteral("打开输出"), this);
    m_openButton->setObjectName(QStringLiteral("hostRipOpenButton"));
    m_openButton->setIcon(style()->standardIcon(QStyle::SP_DirOpenIcon));
    actions->addWidget(m_runButton);
    actions->addWidget(m_cancelButton);
    actions->addWidget(m_openButton);
    layout->addLayout(actions);
    layout->addStretch(1);

    const auto settingsEdited = [this]()
    {
        UpdateOutputPath();
        emit SigSettingsChanged();
        RefreshControls();
    };
    connect(m_autoCheck, &QCheckBox::toggled, this, settingsEdited);
    for (QComboBox* combo : {
             m_intentCombo,
             m_transparentCombo,
             m_colorModeCombo,
             m_inputIccCombo,
             m_outputIccCombo,
             m_outputValidationCombo,
             m_grayBitsCombo})
    {
        connect(
            combo,
            qOverload<int>(&QComboBox::currentIndexChanged),
            this,
            settingsEdited);
    }
    connect(m_continueCheck, &QCheckBox::toggled, this, settingsEdited);
    connect(
        m_timeoutSpin,
        qOverload<int>(&QSpinBox::valueChanged),
        this,
        settingsEdited);
    connect(m_runButton, &QPushButton::clicked, this, &HostRipSettingsPanel::SigRunRequested);
    connect(m_cancelButton, &QPushButton::clicked, this, &HostRipSettingsPanel::SigCancelRequested);
    connect(m_openButton, &QPushButton::clicked, this, [this]()
    {
        emit SigOpenOutputRequested(m_outputDirectory);
    });
}

hostripsettings HostRipSettingsPanel::Settings() const
{
    hostripsettings settings;
    settings.autoafterslice = m_autoCheck->isChecked();
    settings.renderintent = m_intentCombo->currentData().toInt();
    settings.transparentmode = m_transparentCombo->currentData().toInt();
    settings.colormode = m_colorModeCombo->currentData().toInt();
    settings.inputicc = m_inputIccCombo->currentData().toString();
    settings.outputicc = m_outputIccCombo->currentData().toString();
    settings.continueonerror = m_continueCheck->isChecked();
    settings.devicegraybits = m_grayBitsCombo->currentData().toInt();
    settings.outputvalidationmode =
        m_outputValidationCombo->currentData().toString();
    settings.timeoutseconds = m_timeoutSpin->value();
    return settings;
}

void HostRipSettingsPanel::SetSettings(const hostripsettings& settings)
{
    const QSignalBlocker autoBlocker(m_autoCheck);
    const QSignalBlocker intentBlocker(m_intentCombo);
    const QSignalBlocker transparentBlocker(m_transparentCombo);
    const QSignalBlocker colorModeBlocker(m_colorModeCombo);
    const QSignalBlocker inputIccBlocker(m_inputIccCombo);
    const QSignalBlocker outputIccBlocker(m_outputIccCombo);
    const QSignalBlocker continueBlocker(m_continueCheck);
    const QSignalBlocker grayBitsBlocker(m_grayBitsCombo);
    const QSignalBlocker outputValidationBlocker(m_outputValidationCombo);
    const QSignalBlocker timeoutBlocker(m_timeoutSpin);
    m_autoCheck->setChecked(settings.autoafterslice);
    m_intentCombo->setCurrentIndex(
        m_intentCombo->findData(settings.renderintent));
    m_transparentCombo->setCurrentIndex(
        m_transparentCombo->findData(settings.transparentmode));
    m_colorModeCombo->setCurrentIndex(
        m_colorModeCombo->findData(settings.colormode));
    m_inputIccCombo->setCurrentIndex(
        m_inputIccCombo->findData(settings.inputicc));
    m_outputIccCombo->setCurrentIndex(
        m_outputIccCombo->findData(settings.outputicc));
    m_continueCheck->setChecked(settings.continueonerror);
    m_grayBitsCombo->setCurrentIndex(
        m_grayBitsCombo->findData(settings.devicegraybits));
    m_outputValidationCombo->setCurrentIndex(
        m_outputValidationCombo->findData(settings.outputvalidationmode));
    m_timeoutSpin->setValue(settings.timeoutseconds);
    UpdateOutputPath();
    RefreshControls();
}

void HostRipSettingsPanel::SetModuleDirectory(const QString& directory)
{
    m_modulePathEdit->setText(directory);
}

void HostRipSettingsPanel::SetPackageDirectory(const QString& directory)
{
    m_packageDirectory = directory;
    m_inputPathEdit->setText(
        directory.isEmpty() ? QString{} : directory + QStringLiteral("/layers"));
    UpdateOutputPath();
    m_requestValid = false;
    RefreshControls();
}

void HostRipSettingsPanel::UpdateOutputPath()
{
    m_outputDirectory = m_packageDirectory.isEmpty()
        ? QString{}
        : QDir(m_packageDirectory).filePath(
            HostRipSettingsStore::EffectiveOutputDirectoryName(Settings()));
    m_outputExists = !m_outputDirectory.isEmpty()
        && QFileInfo(m_outputDirectory).isDir();
    m_outputPathEdit->setText(m_outputDirectory);
}

QString HostRipSettingsPanel::PackageDirectory() const
{
    return m_packageDirectory;
}

void HostRipSettingsPanel::SetRuntimeStatus(
    const bool valid,
    const QString& message)
{
    m_runtimeValid = valid;
    m_runtimeStatusLabel->setText(message);
    RefreshControls();
}

void HostRipSettingsPanel::SetRequestStatus(
    const bool valid,
    const QString& message)
{
    m_requestValid = valid;
    if (!m_jobActive)
    {
        m_jobStatusLabel->setText(message);
    }
    RefreshControls();
}

void HostRipSettingsPanel::SetJobActive(const bool active)
{
    m_jobActive = active;
    RefreshControls();
}

void HostRipSettingsPanel::ShowJobState(
    const QString& state,
    const QString& message)
{
    m_jobStatusLabel->setText(
        QStringLiteral("%1 · %2").arg(state, message));
}

void HostRipSettingsPanel::ShowCompletion(
    const bool success,
    const bool cancelled,
    const QString& message,
    const QString& outputDirectory)
{
    m_jobActive = false;
    if (success)
    {
        m_outputDirectory = outputDirectory;
        m_outputExists = QFileInfo(m_outputDirectory).isDir();
    }
    m_jobStatusLabel->setText(
        cancelled ? QStringLiteral("RIP 已取消 · %1").arg(message)
        : success ? QStringLiteral("RIP 完成 · %1").arg(message)
                  : QStringLiteral("RIP 失败 · %1").arg(message));
    RefreshControls();
}

void HostRipSettingsPanel::RefreshControls()
{
    const bool editable = !m_jobActive;
    m_autoCheck->setEnabled(editable);
    m_intentCombo->setEnabled(editable);
    m_transparentCombo->setEnabled(editable);
    m_inputIccCombo->setEnabled(editable);
    m_outputIccCombo->setEnabled(editable);
    m_continueCheck->setEnabled(editable);
    m_outputValidationCombo->setEnabled(editable);
    m_grayBitsCombo->setEnabled(editable);
    m_timeoutSpin->setEnabled(editable);
    m_runButton->setEnabled(
        editable && m_runtimeValid && !m_packageDirectory.isEmpty()
            && m_requestValid && !m_outputExists);
    m_cancelButton->setEnabled(m_jobActive);
    m_openButton->setEnabled(
        !m_jobActive && m_outputExists);
}
