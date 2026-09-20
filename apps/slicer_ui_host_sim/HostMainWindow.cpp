#include "HostMainWindow.h"

#include "HostTextureWhitePreflightService.h"
#include "HostVersionInfo.h"
#include "HostResponsiveForms.h"
#include "HostWheelPolicy.h"
#include "HostInspectorPages.h"
#include "MoveOptimizationPolicy.h"
#include "SceneInteractionController.h"
#include "ViewWorkspaceWidget.h"
#include "render/CpuRasterBackend.h"
#include "render/SceneRenderPolicy.h"
#include "render/TopViewRenderPolicy.h"
#include "settings/ViewPresentationSettings.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFormLayout>
#include <QFontDatabase>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QScrollArea>
#include <QStandardPaths>
#include <QStringList>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace
{
QString DefaultSessionConfigPath()
{
    QString root = QStandardPaths::writableLocation(
        QStandardPaths::AppConfigLocation);
    if (root.isEmpty())
    {
        root = QCoreApplication::applicationDirPath();
    }
    return QDir(root).filePath(QStringLiteral("session_config.json"));
}
}

HostMainWindow::HostMainWindow(
    const QString& modulePath,
    QWidget* parent)
    : QMainWindow(parent)
{
    m_viewSettings = std::make_unique<ViewPresentationSettings>(
        DefaultSessionConfigPath());
    m_importWorkflow = std::make_unique<HostModelImportWorkflow>(m_client);
    m_sliceJobController = std::make_unique<HostSliceJobController>(m_client);
    m_ripJobController = std::make_unique<HostRipJobController>();
    m_ripModuleDirectory = HostRipJobController::DefaultModuleDirectory();
    m_packageReviewController =
        std::make_unique<HostPackageReviewController>(m_client);
    m_textureWhitePreflightService =
        std::make_unique<HostTextureWhitePreflightService>();
    m_profileCatalog = std::make_unique<ReferenceHostProfileCatalog>();
    QString settingsError;
    m_viewSettings->Load(&settingsError);
    BuildInterface();
    RestoreWorkspaceState();
    if (!settingsError.isEmpty())
    {
        m_workspace->ShowViewError(settingsError);
    }
    LoadModule(modulePath);
}

void HostMainWindow::BuildInterface()
{
    setWindowTitle(HostVersionInfo::ApplicationTitle());
    resize(1080, 720);

    auto* centralWidget = new QWidget(this);
    auto* layout = new QVBoxLayout(centralWidget);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(10);

    m_versionLabel = new QLabel(centralWidget);
    m_versionLabel->setObjectName(QStringLiteral("applicationVersionLabel"));
    m_versionLabel->setText(
        QStringLiteral("SliceSoft %1 · 切片库加载中")
            .arg(HostVersionInfo::ApplicationVersion()));
    m_versionLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    m_statusLabel = new QLabel(centralWidget);
    m_statusLabel->setObjectName(QStringLiteral("moduleStatusLabel"));
    m_statusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    m_pathLabel = new QLabel(centralWidget);
    m_pathLabel->setObjectName(QStringLiteral("modulePathLabel"));
    m_pathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_pathLabel->setWordWrap(true);

    m_workspaceTabs = new QTabWidget(centralWidget);
    m_workspaceTabs->setObjectName(QStringLiteral("hostWorkspaceTabs"));

    auto* workspacePage = new QWidget(m_workspaceTabs);
    auto* workspaceLayout = new QHBoxLayout(workspacePage);
    workspaceLayout->setContentsMargins(0, 0, 0, 0);
    workspaceLayout->setSpacing(8);
    m_workspaceSplitter = new QSplitter(Qt::Horizontal, workspacePage);
    m_workspaceSplitter->setObjectName(QStringLiteral("workspaceSplitter"));

    m_workspace = new ViewWorkspaceWidget(m_workspaceSplitter);
    m_workspace->setObjectName(QStringLiteral("dualViewWorkspace"));
    InitializeViewWorkspace();

    auto* importPanel = new QGroupBox(
        QStringLiteral("模型与导入预检"), m_workspaceSplitter);
    importPanel->setObjectName(QStringLiteral("hostModelImportPanel"));
    importPanel->setMinimumWidth(300);
    importPanel->setMaximumWidth(720);
    auto* importLayout = new QVBoxLayout(importPanel);
    m_inspectorTabs = new QTabWidget(importPanel);
    m_inspectorTabs->setObjectName(QStringLiteral("hostSceneInspectorTabs"));
    auto* modelPage = new QWidget(m_inspectorTabs);
    auto* modelLayout = new QVBoxLayout(modelPage);
    modelLayout->setContentsMargins(4, 4, 4, 4);
    m_modelListPanel = new HostModelListPanel(modelPage);
    modelLayout->addWidget(m_modelListPanel, 1);

    AttachSceneResetButton(modelPage, modelLayout);

    m_importSummaryLabel = new QLabel(
        QStringLiteral("尚未导入模型。"), modelPage);
    m_importSummaryLabel->setObjectName(
        QStringLiteral("hostImportSummaryLabel"));
    m_importSummaryLabel->setWordWrap(true);
    m_importSummaryLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    modelLayout->addWidget(m_importSummaryLabel);

    m_preflightTable = new QTableWidget(modelPage);
    m_preflightTable->setObjectName(
        QStringLiteral("hostImportPreflightTable"));
    m_preflightTable->setColumnCount(3);
    m_preflightTable->setHorizontalHeaderLabels(QStringList{
        QStringLiteral("级别"),
        QStringLiteral("问题 / 数量"),
        QStringLiteral("详情")});
    m_preflightTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_preflightTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_preflightTable->verticalHeader()->setVisible(false);
    m_preflightTable->horizontalHeader()->setSectionResizeMode(
        0, QHeaderView::ResizeToContents);
    m_preflightTable->horizontalHeader()->setSectionResizeMode(
        1, QHeaderView::ResizeToContents);
    m_preflightTable->horizontalHeader()->setSectionResizeMode(
        2, QHeaderView::Stretch);
    m_preflightTable->setMinimumHeight(150);
    modelLayout->addWidget(m_preflightTable, 2);
    m_inspectorTabs->addTab(modelPage, QStringLiteral("模型"));

    m_profilePanel = new HostProfilePanel(m_inspectorTabs);
    m_inspectorTabs->addTab(m_profilePanel, QStringLiteral("工艺配置"));

    m_sliceSettingsPanel = new HostSliceSettingsPanel(m_inspectorTabs);
    m_inspectorTabs->addTab(m_sliceSettingsPanel, QStringLiteral("切片设置"));

    m_sliceJobPanel = new HostSliceJobPanel(m_inspectorTabs);
    m_inspectorTabs->addTab(m_sliceJobPanel, QStringLiteral("切片作业"));

    m_transformLayoutPanel = new HostTransformLayoutPanel(m_inspectorTabs);
    // 「导入与排版」已迁往「模型」页，这一页只剩实例变换，标题随之收窄。
    m_inspectorTabs->addTab(m_transformLayoutPanel, QStringLiteral("变换"));

    m_ripSettingsPanel = new HostRipSettingsPanel(m_inspectorTabs);
    m_ripSettingsPanel->setObjectName(QStringLiteral("hostRipSettingsPanel"));
    m_ripSettingsPanel->SetModuleDirectory(m_ripModuleDirectory);
    m_inspectorTabs->addTab(m_ripSettingsPanel, QStringLiteral("RIP 设置"));
    ConfigureHostForms(m_inspectorTabs);
    // 变换面板只剩 1 个 group，但仍走 GroupHostInspectorSections：
    // 实测去掉它之后本页会超出 hostux 的零滚动判据 14px——
    // sections 这个 QTabWidget 的最小尺寸比它包住的 QGroupBox 小，
    // 去掉包装等于把 group 的最小高度直接暴露给滚动区。
    //
    // ⚠ 标题数必须与直接子 QGroupBox 数【相等】，否则该函数静默 return，
    //   连它顺带做的 WrapLongRows 也一并跳过，而这不会有任何报错。
    GroupHostInspectorSections(m_transformLayoutPanel,{QStringLiteral("变换")});
    GroupHostInspectorSections(m_ripSettingsPanel,{QStringLiteral("参数"),QStringLiteral("路径"),QStringLiteral("手动 RIP")});
    ConfigureHostInspectorPages(m_inspectorTabs);
    importLayout->addWidget(m_inspectorTabs, 1);

    m_workspaceSplitter->addWidget(m_workspace);
    m_workspaceSplitter->addWidget(importPanel);
    m_workspaceSplitter->setStretchFactor(0, 1);
    m_workspaceSplitter->setStretchFactor(1, 0);
    workspaceLayout->addWidget(m_workspaceSplitter);
    m_workspaceTabs->addTab(workspacePage, QStringLiteral("工作区"));

    m_packageReviewPanel = new HostPackageReviewPanel(m_workspaceTabs);
    m_workspaceTabs->addTab(m_packageReviewPanel, QStringLiteral("结果"));

    auto* settingsPage = new QWidget(m_workspaceTabs);
    auto* settingsLayout = new QVBoxLayout(settingsPage);
    settingsLayout->setContentsMargins(16, 16, 16, 16);
    auto* displayGroup = new QGroupBox(
        QStringLiteral("显示设置"), settingsPage);
    auto* form = new QFormLayout(displayGroup);
    m_defaultViewCombo = new QComboBox(displayGroup);
    m_defaultViewCombo->setObjectName(QStringLiteral("defaultViewModeCombo"));
    m_defaultViewCombo->addItem(QStringLiteral("俯视"), QStringLiteral("top"));
    m_defaultViewCombo->addItem(QStringLiteral("3D"), QStringLiteral("three_d"));
    m_defaultViewCombo->setCurrentIndex(
        m_viewSettings->DefaultViewMode() == HostViewMode::Top ? 0 : 1);
    m_defaultViewCombo->setToolTip(QStringLiteral(
        "仅决定下次进入工作区的默认视图，不改变场景或切片数据"));
    m_projectionCombo = new QComboBox(displayGroup);
    m_projectionCombo->setObjectName(QStringLiteral("threeDProjectionCombo"));
    m_projectionCombo->addItem(
        QStringLiteral("正交"), QStringLiteral("orthographic"));
    m_projectionCombo->addItem(
        QStringLiteral("透视"), QStringLiteral("perspective"));
    m_projectionCombo->setCurrentIndex(
        m_viewSettings->ThreeDProjection()
                == slicer::render::Projection::Orthographic ? 0 : 1);
    m_projectionCombo->setToolTip(QStringLiteral(
        "俯视固定正交；该选项只控制 3D 视图显示"));
    form->addRow(QStringLiteral("默认视图"), m_defaultViewCombo);
    form->addRow(QStringLiteral("3D 投影"), m_projectionCombo);
    auto* contractLabel = new QLabel(
        QStringLiteral(
            "网格：1 mm 小格 / 10 mm 大格，范围来自 buildVolume。\n"
            "白色纹理对比、网格和选中高亮均只影响显示。"),
        displayGroup);
    contractLabel->setWordWrap(true);
    form->addRow(QStringLiteral("显示合同"), contractLabel);
    settingsLayout->addWidget(displayGroup);
    settingsLayout->addStretch(1);
    // 这一页只有「显示设置」一个分组（默认视图 / 3D 投影），不改变切片数据。
    // 原名「设置」与 inspector 的「切片设置」区分度不足，而用户找切片参数时
    // 的自然第一落点恰是顶层「设置」——改名拉开区分度。
    m_workspaceTabs->addTab(settingsPage, QStringLiteral("显示"));

    auto* diagnosticPage = new QWidget(m_workspaceTabs);
    auto* diagnosticLayout = new QVBoxLayout(diagnosticPage);
    m_moduleInfoView = new QPlainTextEdit(diagnosticPage);
    m_moduleInfoView->setObjectName(QStringLiteral("moduleInfoView"));
    m_moduleInfoView->setReadOnly(true);
    m_moduleInfoView->setFont(QFontDatabase::systemFont(
        QFontDatabase::FixedFont));
    diagnosticLayout->addWidget(m_moduleInfoView);
    m_workspaceTabs->addTab(diagnosticPage, QStringLiteral("模块诊断"));

    layout->addWidget(m_versionLabel);
    layout->addWidget(m_statusLabel);
    layout->addWidget(m_pathLabel);
    layout->addWidget(m_workspaceTabs, 1);
    setCentralWidget(centralWidget);
    new HostWheelPolicy(this);
    connect(m_ripJobController.get(), &HostRipJobController::SigProgress,
        m_ripSettingsPanel, &HostRipSettingsPanel::ShowProgress);

    connect(m_defaultViewCombo,
            qOverload<int>(&QComboBox::currentIndexChanged),
            this,
            [this](int)
            {
                SaveViewSettings();
            });
    connect(m_projectionCombo,
            qOverload<int>(&QComboBox::currentIndexChanged),
            this,
            [this](int)
            {
                SaveViewSettings();
            });
    connect(
        m_modelListPanel,
        &HostModelListPanel::SigAddRequested,
        this,
        &HostMainWindow::OnImportModel);
    connect(
        m_modelListPanel,
        &HostModelListPanel::SigRemoveRequested,
        this,
        &HostMainWindow::OnRemoveModels);
    connect(
        m_modelListPanel,
        &HostModelListPanel::SigSelectionChanged,
        this,
        &HostMainWindow::OnModelSelectionChanged);
    connect(
        m_profilePanel,
        &HostProfilePanel::SigProfileChanged,
        this,
        &HostMainWindow::OnProfileChanged);
    connect(
        m_sliceSettingsPanel,
        &HostSliceSettingsPanel::SigSettingsChanged,
        this,
        &HostMainWindow::OnSliceSettingsChanged);
    connect(
        m_textureWhitePreflightService.get(),
        &HostTextureWhitePreflightService::SigPreflightFinished,
        this,
        &HostMainWindow::OnTextureWhitePreflightFinished);
    connect(
        m_textureWhitePreflightService.get(),
        &HostTextureWhitePreflightService::SigPreflightDiscarded,
        this,
        &HostMainWindow::OnTextureWhitePreflightDiscarded);
    connect(
        m_sliceJobPanel,
        &HostSliceJobPanel::SigStartRequested,
        this,
        &HostMainWindow::OnStartSlice);
    connect(
        m_sliceJobPanel,
        &HostSliceJobPanel::SigCancelRequested,
        this,
        &HostMainWindow::OnCancelSlice);
    connect(
        m_sliceJobController.get(),
        &HostSliceJobController::SigProgressChanged,
        this,
        &HostMainWindow::OnSliceJobProgress);
    connect(
        m_sliceJobController.get(),
        &HostSliceJobController::SigTimingProgress,
        m_sliceJobPanel,
        &HostSliceJobPanel::UpdateLiveTiming);
    connect(
        m_sliceJobController.get(),
        &HostSliceJobController::SigCompleted,
        this,
        &HostMainWindow::OnSliceJobCompleted);
    connect(
        m_ripSettingsPanel,
        &HostRipSettingsPanel::SigSettingsChanged,
        this,
        &HostMainWindow::OnRipSettingsChanged);
    connect(
        m_ripSettingsPanel,
        &HostRipSettingsPanel::SigRunRequested,
        this,
        &HostMainWindow::OnRunRip);
    connect(
        m_ripSettingsPanel,
        &HostRipSettingsPanel::SigManualPathsChanged,
        this,
        &HostMainWindow::OnRipManualPathsChanged);
    connect(
        m_ripSettingsPanel,
        &HostRipSettingsPanel::SigManualRunRequested,
        this,
        &HostMainWindow::OnRunManualRip);
    connect(
        m_ripSettingsPanel,
        &HostRipSettingsPanel::SigCancelRequested,
        this,
        &HostMainWindow::OnCancelRip);
    connect(
        m_ripSettingsPanel,
        &HostRipSettingsPanel::SigOpenOutputRequested,
        this,
        &HostMainWindow::OnOpenRipOutputRequested);
    connect(
        m_ripJobController.get(),
        &HostRipJobController::SigStateChanged,
        this,
        &HostMainWindow::OnRipStateChanged);
    connect(
        m_ripJobController.get(),
        &HostRipJobController::SigCompleted,
        this,
        &HostMainWindow::OnRipCompleted);
    connect(
        m_packageReviewPanel,
        &HostPackageReviewPanel::SigLayerPreviewRequested,
        this,
        &HostMainWindow::OnResultLayerRequested);
    connect(
        m_packageReviewPanel,
        &HostPackageReviewPanel::SigReportRequested,
        this,
        &HostMainWindow::OnResultReportRequested);
    connect(
        m_packageReviewPanel,
        &HostPackageReviewPanel::SigOpenPackageDirectoryRequested,
        this,
        &HostMainWindow::OnOpenPackageDirectoryRequested);
    connect(
        m_transformLayoutPanel,
        &HostTransformLayoutPanel::SigTransformRequested,
        this,
        &HostMainWindow::OnTransformRequested);
    connect(
        m_transformLayoutPanel,
        &HostTransformLayoutPanel::SigLandOnBuildPlateRequested,
        this,
        &HostMainWindow::OnLandOnBuildPlateRequested);
    connect(
        m_modelListPanel,
        &HostModelListPanel::SigLayoutRequested,
        this,
        &HostMainWindow::OnLayoutRequested);
    RefreshRipRuntimeStatus();
}

void HostMainWindow::LoadModule(const QString& modulePath)
{
    m_pathLabel->setText(QStringLiteral("模块：%1").arg(modulePath));

    QString error;
    if (!m_client.Open(modulePath, QByteArrayLiteral("{}"), &error))
    {
        m_versionLabel->setText(
            QStringLiteral("SliceSoft %1 · 切片库不可用")
                .arg(HostVersionInfo::ApplicationVersion()));
        m_statusLabel->setText(
            QStringLiteral("SliceSoft %1 · 切片库不可用")
                .arg(HostVersionInfo::ApplicationVersion()));
        m_moduleInfoView->setPlainText(
            QStringLiteral("%1\n\n切片库\n  状态：不可用\n  原因：%2")
                .arg(HostVersionInfo::ApplicationDiagnosticText(), error));
        return;
    }

    QByteArray selfTestReport;
    if (!m_client.SelfTest(&selfTestReport, &error))
    {
        m_versionLabel->setText(
            QStringLiteral("SliceSoft %1 · 切片库自检失败")
                .arg(HostVersionInfo::ApplicationVersion()));
        m_statusLabel->setText(
            QStringLiteral("SliceSoft %1 · 切片库自检失败")
                .arg(HostVersionInfo::ApplicationVersion()));
        m_moduleInfoView->setPlainText(
            QStringLiteral("%1\n\n切片库自检失败\n%2")
                .arg(HostVersionInfo::ApplicationDiagnosticText(), error));
        return;
    }

    ConfigureProfiles();

    const QByteArray moduleInfo = m_client.ModuleInfo();
    const QString slicerVersion =
        HostVersionInfo::SlicerVersionFromModuleInfo(moduleInfo);
    m_versionLabel->setText(
        QStringLiteral("SliceSoft %1 · 切片库 %2 · SPI v%3")
            .arg(HostVersionInfo::ApplicationVersion())
            .arg(slicerVersion.isEmpty() ? QStringLiteral("未知") : slicerVersion)
            .arg(PM_SPI_VERSION));
    m_statusLabel->setText(
        QStringLiteral(
            "SliceSoft %1 · 切片库 %2 · SPI v%3 · Profile %4 · ABI 调用 %5 次")
            .arg(HostVersionInfo::ApplicationVersion())
            .arg(slicerVersion.isEmpty() ? QStringLiteral("未知") : slicerVersion)
            .arg(PM_SPI_VERSION)
            .arg(m_selectedProfileId)
            .arg(m_client.CallCount()));
    SetSceneCommandsEnabled(true);
    RefreshSliceJobReadiness();
    m_moduleInfoView->setPlainText(
        QStringLiteral("%1\n\n切片库模块信息\n%2\n\n自检报告\n%3")
            .arg(
                HostVersionInfo::ApplicationDiagnosticText(),
                QString::fromUtf8(moduleInfo),
                QString::fromUtf8(selfTestReport)));
}

void HostMainWindow::OnRemoveModels(const QStringList& instanceIds)
{
    SetSceneCommandsEnabled(false);
    QString error;
    const bool removed = m_importWorkflow->RemoveInstances(
        instanceIds, &error);
    SetSceneCommandsEnabled(m_client.IsOpen());
    if (!removed)
    {
        ShowImportError(error);
        return;
    }
    m_modelListPanel->RemoveInstances(instanceIds);
    RefreshSliceSettings();
    m_transformLayoutPanel->SetSceneState(
        m_importWorkflow->InstanceCount(),
        m_importWorkflow->SceneRevision());
    m_importSummaryLabel->setText(
        QStringLiteral("已删除 %1 个模型实例；场景 revision=%2")
            .arg(instanceIds.size())
            .arg(m_importWorkflow->SceneRevision()));
    m_statusLabel->setText(
        QStringLiteral("模型实例已删除 · ABI 调用 %1 次")
            .arg(m_client.CallCount()));
    RefreshSceneViews();
}

void HostMainWindow::OnModelSelectionChanged(
    const QStringList& instanceIds)
{
    m_workspace->SetSelectedInstances(instanceIds);
    m_transformLayoutPanel->SetSelectedInstances(instanceIds);
    if (m_topViewPolicy) m_topViewPolicy->SetSelectedInstances(instanceIds);
    RenderTransientTopView();
    RenderThreeDView();
}

void HostMainWindow::SaveViewSettings()
{
    m_viewSettings->SetDefaultViewMode(
        m_defaultViewCombo->currentData().toString()
                == QStringLiteral("three_d")
            ? HostViewMode::ThreeD : HostViewMode::Top);
    m_viewSettings->SetThreeDProjection(
        m_projectionCombo->currentData().toString()
                == QStringLiteral("perspective")
            ? slicer::render::Projection::Perspective
            : slicer::render::Projection::Orthographic);
    m_workspace->SetThreeDProjection(m_viewSettings->ThreeDProjection());
    QString error;
    if (!m_viewSettings->Save(&error))
    {
        m_workspace->ShowViewError(error);
    }
}
