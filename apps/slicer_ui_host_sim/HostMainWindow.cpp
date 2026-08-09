#include "HostMainWindow.h"

#include "ViewWorkspaceWidget.h"
#include "render/CpuRasterBackend.h"
#include "render/SceneRenderPolicy.h"
#include "render/TopViewRenderPolicy.h"
#include "settings/ViewPresentationSettings.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFontDatabase>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QStandardPaths>
#include <QStringList>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
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
    m_packageReviewController =
        std::make_unique<HostPackageReviewController>(m_client);
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
    setWindowTitle(QStringLiteral("SliceSoft 打印宿主参考实现"));
    resize(1080, 720);

    auto* centralWidget = new QWidget(this);
    auto* layout = new QVBoxLayout(centralWidget);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(10);

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
    importPanel->setMaximumWidth(420);
    auto* importLayout = new QVBoxLayout(importPanel);
    m_inspectorTabs = new QTabWidget(importPanel);
    m_inspectorTabs->setObjectName(QStringLiteral("hostSceneInspectorTabs"));
    auto* modelPage = new QWidget(m_inspectorTabs);
    auto* modelLayout = new QVBoxLayout(modelPage);
    modelLayout->setContentsMargins(4, 4, 4, 4);
    m_modelListPanel = new HostModelListPanel(modelPage);
    modelLayout->addWidget(m_modelListPanel, 1);

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
    m_inspectorTabs->addTab(m_profilePanel, QStringLiteral("Profile"));

    m_sliceSettingsPanel = new HostSliceSettingsPanel(m_inspectorTabs);
    m_inspectorTabs->addTab(m_sliceSettingsPanel, QStringLiteral("切片设置"));

    m_sliceJobPanel = new HostSliceJobPanel(m_inspectorTabs);
    m_inspectorTabs->addTab(m_sliceJobPanel, QStringLiteral("切片作业"));

    m_transformLayoutPanel = new HostTransformLayoutPanel(m_inspectorTabs);
    m_inspectorTabs->addTab(
        m_transformLayoutPanel, QStringLiteral("变换与排版"));
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
    m_workspaceTabs->addTab(settingsPage, QStringLiteral("设置"));

    auto* diagnosticPage = new QWidget(m_workspaceTabs);
    auto* diagnosticLayout = new QVBoxLayout(diagnosticPage);
    m_moduleInfoView = new QPlainTextEdit(diagnosticPage);
    m_moduleInfoView->setObjectName(QStringLiteral("moduleInfoView"));
    m_moduleInfoView->setReadOnly(true);
    m_moduleInfoView->setFont(QFontDatabase::systemFont(
        QFontDatabase::FixedFont));
    diagnosticLayout->addWidget(m_moduleInfoView);
    m_workspaceTabs->addTab(diagnosticPage, QStringLiteral("模块诊断"));

    layout->addWidget(m_statusLabel);
    layout->addWidget(m_pathLabel);
    layout->addWidget(m_workspaceTabs, 1);
    setCentralWidget(centralWidget);

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
        &HostSliceJobController::SigCompleted,
        this,
        &HostMainWindow::OnSliceJobCompleted);
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
        &HostTransformLayoutPanel::SigLayoutRequested,
        this,
        &HostMainWindow::OnLayoutRequested);
}

void HostMainWindow::LoadModule(const QString& modulePath)
{
    m_pathLabel->setText(QStringLiteral("模块：%1").arg(modulePath));

    QString error;
    if (!m_client.Open(modulePath, QByteArrayLiteral("{}"), &error))
    {
        m_statusLabel->setText(QStringLiteral("模块不可用"));
        m_moduleInfoView->setPlainText(error);
        return;
    }

    QByteArray selfTestReport;
    if (!m_client.SelfTest(&selfTestReport, &error))
    {
        m_statusLabel->setText(QStringLiteral("模块自检失败"));
        m_moduleInfoView->setPlainText(error);
        return;
    }

    ConfigureProfiles();

    m_statusLabel->setText(
        QStringLiteral("模块已就绪 · SPI v%1 · Profile %2 · ABI 调用 %3 次")
            .arg(PM_SPI_VERSION)
            .arg(m_selectedProfileId)
            .arg(m_client.CallCount()));
    SetSceneCommandsEnabled(true);
    RefreshSliceJobReadiness();
    m_moduleInfoView->setPlainText(
        QStringLiteral("模块信息\n%1\n\n自检报告\n%2")
            .arg(
                QString::fromUtf8(m_client.ModuleInfo()),
                QString::fromUtf8(selfTestReport)));
}

void HostMainWindow::OnImportModel()
{
    const QString modelPath = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("选择要导入的 OBJ 或 3MF 模型"),
        QDir::homePath(),
        QStringLiteral(
            "支持的模型 (*.obj *.3mf);;OBJ 模型 (*.obj);;3MF 模型 (*.3mf)"));
    if (modelPath.isEmpty())
    {
        return;
    }

    QString contextError;
    if (!ApplyPendingSceneContext(&contextError))
    {
        ShowImportError(contextError);
        return;
    }

    SetSceneCommandsEnabled(false);
    m_importSummaryLabel->setText(QStringLiteral("正在导入并执行快速预检…"));
    QCoreApplication::processEvents();

    hostmodelimportresult result;
    QString error;
    const bool imported = m_importWorkflow->ImportModel(
        modelPath, &result, &error);
    SetSceneCommandsEnabled(m_client.IsOpen());
    if (!imported)
    {
        ShowImportError(error);
        return;
    }
    RefreshSliceSettings();
    ShowImportResult(result);
    RefreshTopView();
    RefreshThreeDView();
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
}

void HostMainWindow::OnModelSelectionChanged(
    const QStringList& instanceIds)
{
    m_workspace->SetSelectedInstances(instanceIds);
    m_transformLayoutPanel->SetSelectedInstances(instanceIds);
}

void HostMainWindow::ShowImportResult(const hostmodelimportresult& result)
{
    const QFileInfo source(result.sourcepath);
    const QString admissionText =
        result.admission == QStringLiteral("passed")
        ? QStringLiteral("通过")
        : result.admission == QStringLiteral("manual_repair_required")
            ? QStringLiteral("需要人工修复")
            : QStringLiteral("阻断");
    m_modelListPanel->AddModel(result);
    m_transformLayoutPanel->SetSceneState(
        m_importWorkflow->InstanceCount(),
        m_importWorkflow->SceneRevision());
    m_importSummaryLabel->setText(
        QStringLiteral(
            "%1\nOBJ/3MF 元数据：%2 三角形，%3 顶点，"
            "%4 × %5 × %6 mm，UV=%7，法线=%8\n"
            "快速预检：%9；场景 revision=%10")
            .arg(source.fileName())
            .arg(result.trianglecount)
            .arg(result.vertexcount)
            .arg(result.widthmm, 0, 'f', 2)
            .arg(result.heightmm, 0, 'f', 2)
            .arg(result.depthmm, 0, 'f', 2)
            .arg(result.hasuv ? QStringLiteral("有") : QStringLiteral("无"))
            .arg(result.hasnormals ? QStringLiteral("有") : QStringLiteral("无"))
            .arg(admissionText)
            .arg(m_importWorkflow->SceneRevision()));

    m_preflightTable->setRowCount(result.issues.size());
    for (int rowIndex = 0; rowIndex < result.issues.size(); ++rowIndex)
    {
        const hostpreflightissue& issue = result.issues.at(rowIndex);
        m_preflightTable->setItem(
            rowIndex, 0, new QTableWidgetItem(issue.severity));
        m_preflightTable->setItem(
            rowIndex,
            1,
            new QTableWidgetItem(QStringLiteral("%1 (%2)")
                .arg(issue.code)
                .arg(issue.count)));
        m_preflightTable->setItem(
            rowIndex, 2, new QTableWidgetItem(issue.detail));
    }
    m_preflightTable->resizeRowsToContents();
    m_statusLabel->setText(
        QStringLiteral("模型已导入 · %1 · ABI 调用 %2 次")
            .arg(admissionText)
            .arg(m_client.CallCount()));
}

void HostMainWindow::ShowImportError(const QString& error)
{
    const QString detail = error.isEmpty()
        ? QStringLiteral("模型导入流程失败，模块未返回详细原因。")
        : error;
    m_importSummaryLabel->setText(QStringLiteral("导入失败：%1").arg(detail));
    m_statusLabel->setText(QStringLiteral("模型导入失败"));
    QMessageBox::critical(this, QStringLiteral("模型导入失败"), detail);
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
