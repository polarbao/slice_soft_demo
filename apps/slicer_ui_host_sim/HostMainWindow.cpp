#include "HostMainWindow.h"

#include "ViewWorkspaceWidget.h"
#include "settings/ViewPresentationSettings.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFormLayout>
#include <QFontDatabase>
#include <QGroupBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QStandardPaths>
#include <QTabWidget>
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
    QString settingsError;
    m_viewSettings->Load(&settingsError);
    BuildInterface();
    if (!settingsError.isEmpty())
    {
        m_workspace->ShowViewError(settingsError);
    }
    LoadModule(modulePath);
}

HostMainWindow::~HostMainWindow() = default;

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

    auto* tabs = new QTabWidget(centralWidget);
    tabs->setObjectName(QStringLiteral("hostWorkspaceTabs"));

    m_workspace = new ViewWorkspaceWidget(tabs);
    m_workspace->setObjectName(QStringLiteral("dualViewWorkspace"));
    m_workspace->SetMode(m_viewSettings->DefaultViewMode());
    tabs->addTab(m_workspace, QStringLiteral("工作区"));

    auto* settingsPage = new QWidget(tabs);
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
    tabs->addTab(settingsPage, QStringLiteral("设置"));

    auto* diagnosticPage = new QWidget(tabs);
    auto* diagnosticLayout = new QVBoxLayout(diagnosticPage);
    m_moduleInfoView = new QPlainTextEdit(diagnosticPage);
    m_moduleInfoView->setObjectName(QStringLiteral("moduleInfoView"));
    m_moduleInfoView->setReadOnly(true);
    m_moduleInfoView->setFont(QFontDatabase::systemFont(
        QFontDatabase::FixedFont));
    diagnosticLayout->addWidget(m_moduleInfoView);
    tabs->addTab(diagnosticPage, QStringLiteral("模块诊断"));

    layout->addWidget(m_statusLabel);
    layout->addWidget(m_pathLabel);
    layout->addWidget(tabs, 1);
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

    m_statusLabel->setText(
        QStringLiteral("模块已就绪 · SPI v%1 · ABI 调用 %2 次")
            .arg(PM_SPI_VERSION)
            .arg(m_client.CallCount()));
    m_moduleInfoView->setPlainText(
        QStringLiteral("模块信息\n%1\n\n自检报告\n%2")
            .arg(
                QString::fromUtf8(m_client.ModuleInfo()),
                QString::fromUtf8(selfTestReport)));
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
    QString error;
    if (!m_viewSettings->Save(&error))
    {
        m_workspace->ShowViewError(error);
    }
}
