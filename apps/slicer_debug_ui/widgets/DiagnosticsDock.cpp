#include "DiagnosticsDock.h"

#include "ChannelChartPanel.h"
#include "LogPanel.h"
#include "MaterialClosurePanel.h"
#include "ReportPanel.h"

#include <QTabWidget>

DiagnosticsDock::DiagnosticsDock(QWidget* parent)
    : QDockWidget(QStringLiteral("诊断区域"), parent)
{
    setObjectName(QStringLiteral("diagnosticsDock"));
    setAllowedAreas(Qt::BottomDockWidgetArea);
    setFeatures(QDockWidget::DockWidgetClosable);

    m_tabs = new QTabWidget(this);
    m_tabs->setObjectName(QStringLiteral("diagnosticsTabs"));
    m_tabs->setDocumentMode(true);
    m_reportView = new ReportPanel(m_tabs);
    m_materialClosureView = new MaterialClosurePanel(m_tabs);
    m_chartView = new ChannelChartPanel(m_tabs);
    m_logView = new LogPanel(m_tabs);
    m_tabs->addTab(m_reportView, QStringLiteral("报告"));
    m_tabs->addTab(m_materialClosureView, QStringLiteral("材料闭环"));
    m_tabs->addTab(m_chartView, QStringLiteral("曲线"));
    m_tabs->addTab(m_logView, QStringLiteral("日志"));
    setWidget(m_tabs);

    hide();
}

void DiagnosticsDock::LoadPackage(const PackageSummary& package)
{
    m_reportView->loadPackage(package);
    m_materialClosureView->LoadPackage(package);
    m_chartView->loadPackage(package);
}

ReportPanel* DiagnosticsDock::ReportView() const
{
    return m_reportView;
}

MaterialClosurePanel* DiagnosticsDock::MaterialClosureView() const
{
    return m_materialClosureView;
}

ChannelChartPanel* DiagnosticsDock::ChartView() const
{
    return m_chartView;
}

LogPanel* DiagnosticsDock::LogView() const
{
    return m_logView;
}

void DiagnosticsDock::SetExpanded(const bool expanded)
{
    setVisible(expanded);
    if (expanded)
    {
        raise();
    }
}

bool DiagnosticsDock::IsExpanded() const
{
    return !isHidden();
}

QStringList DiagnosticsDock::TabTitles() const
{
    QStringList titles;
    for (int index = 0; index < m_tabs->count(); ++index)
    {
        titles.push_back(m_tabs->tabText(index));
    }
    return titles;
}
