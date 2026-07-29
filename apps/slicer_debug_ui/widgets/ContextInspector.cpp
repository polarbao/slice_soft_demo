#include "ContextInspector.h"

#include <QLabel>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>

ContextInspector::ContextInspector(
    QWidget* scenePage,
    QWidget* transformPage,
    QWidget* layoutPage,
    QWidget* preflightPage,
    QWidget* parent)
    : QWidget(parent),
      m_scenePage(scenePage)
{
    setObjectName(QStringLiteral("contextInspector"));
    setMinimumWidth(240);
    setMaximumWidth(420);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    m_tabs = new QTabWidget(this);
    m_tabs->setObjectName(
        QStringLiteral("contextInspectorTabs"));
    m_tabs->setDocumentMode(true);

    auto* sliceSettingsPage = new QWidget(m_tabs);
    sliceSettingsPage->setObjectName(
        QStringLiteral("contextSliceSettingsPage"));
    auto* sliceSettingsLayout =
        new QVBoxLayout(sliceSettingsPage);
    m_modeLabel = new QLabel(sliceSettingsPage);
    m_modeLabel->setObjectName(
        QStringLiteral("contextSliceModeLabel"));
    m_profileLabel = new QLabel(sliceSettingsPage);
    m_profileLabel->setObjectName(
        QStringLiteral("contextSliceProfileLabel"));
    m_profileLabel->setWordWrap(true);
    m_availabilityLabel = new QLabel(sliceSettingsPage);
    m_availabilityLabel->setObjectName(
        QStringLiteral("contextSliceAvailabilityLabel"));
    m_availabilityLabel->setWordWrap(true);
    m_openConfigButton = new QPushButton(
        QStringLiteral("打开完整配置"),
        sliceSettingsPage);
    m_openConfigButton->setObjectName(
        QStringLiteral("contextInspectorOpenConfigButton"));
    m_openConfigButton->setToolTip(
        QStringLiteral("切换到中央“配置”页编辑完整切片参数"));
    sliceSettingsLayout->addWidget(m_modeLabel);
    sliceSettingsLayout->addWidget(m_profileLabel);
    sliceSettingsLayout->addWidget(m_availabilityLabel);
    sliceSettingsLayout->addWidget(m_openConfigButton);
    m_diagnosticSettingsPanel =
        new DiagnosticSettingsPanel(
            sliceSettingsPage);
    sliceSettingsLayout->addWidget(
        m_diagnosticSettingsPanel,
        1);

    m_tabs->addTab(scenePage, QStringLiteral("场景"));
    m_tabs->addTab(transformPage, QStringLiteral("变换"));
    m_tabs->addTab(layoutPage, QStringLiteral("排版"));
    m_tabs->addTab(
        sliceSettingsPage,
        QStringLiteral("切片设置"));
    m_tabs->addTab(preflightPage, QStringLiteral("预检"));
    layout->addWidget(m_tabs);

    connect(
        m_openConfigButton,
        &QPushButton::clicked,
        this,
        &ContextInspector::SigOpenConfigRequested);
    connect(
        m_diagnosticSettingsPanel,
        &DiagnosticSettingsPanel::
            SigTextureSurfaceWidthChanged,
        this,
        &ContextInspector::
            SigDiagnosticTextureSurfaceWidthChanged);
    connect(
        m_diagnosticSettingsPanel,
        &DiagnosticSettingsPanel::
            SigModelFillMaterialChanged,
        this,
        &ContextInspector::
            SigDiagnosticModelFillMaterialChanged);
    SetSliceSettingsSummary(
        QStringLiteral("传统切片"),
        QStringLiteral("自定义"),
        QStringLiteral("请先导入模型。"));
}

void ContextInspector::SetSliceSettingsSummary(
    const QString& modeLabel,
    const QString& profileLabel,
    const QString& availability)
{
    m_modeLabel->setText(
        QStringLiteral("切片模式：") + modeLabel);
    m_profileLabel->setText(
        QStringLiteral("Profile：") + profileLabel);
    m_availabilityLabel->setText(
        QStringLiteral("当前状态：") + availability);
}

void ContextInspector::SetDiagnosticRequestedSettings(
    const double widthMm,
    const QString& modelFillMaterial)
{
    m_diagnosticSettingsPanel->SetRequestedSettings(
        widthMm,
        modelFillMaterial);
}

void ContextInspector::SetDiagnosticPresentation(
    const DiagnosticSettingsPresentation& presentation)
{
    m_diagnosticSettingsPanel->SetPresentation(
        presentation);
}

void ContextInspector::ShowScenePage()
{
    m_tabs->setCurrentWidget(m_scenePage);
}

QStringList ContextInspector::PageTitles() const
{
    QStringList titles;
    for (int index = 0; index < m_tabs->count(); ++index)
    {
        titles.push_back(m_tabs->tabText(index));
    }
    return titles;
}

int ContextInspector::PageCount() const
{
    return m_tabs->count();
}

int ContextInspector::CurrentPageIndex() const
{
    return m_tabs->currentIndex();
}

bool ContextInspector::SetCurrentPageIndex(
    const int index)
{
    if (index < 0 || index >= m_tabs->count())
    {
        return false;
    }
    m_tabs->setCurrentIndex(index);
    return true;
}
