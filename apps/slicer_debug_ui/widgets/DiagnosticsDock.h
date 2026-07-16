#pragma once

#include "../services/PackageLoader.h"

#include <QDockWidget>
#include <QStringList>

class ChannelChartPanel;
class LogPanel;
class MaterialClosurePanel;
class ReportPanel;
class QTabWidget;

/**
 * @brief Hosts report, chart, and log diagnostics in a collapsible bottom dock.
 */
class DiagnosticsDock final : public QDockWidget
{
    Q_OBJECT

public:
    /**
     * @brief Create the diagnostics dock and its existing diagnostic panels.
     * @param parent Qt parent widget, normally MainWindow.
     */
    explicit DiagnosticsDock(QWidget* parent = nullptr);

    /**
     * @brief Load one output package into report and chart views.
     * @param package Package summary produced by PackageLoader.
     */
    void LoadPackage(const PackageSummary& package);

    /**
     * @brief Return the report panel owned by the dock.
     * @return Non-owning report panel pointer.
     */
    ReportPanel* ReportView() const;

    /**
     * @brief Return the material-closure diagnostics panel owned by the dock.
     * @return Non-owning material-closure panel pointer.
     */
    MaterialClosurePanel* MaterialClosureView() const;

    /**
     * @brief Return the channel chart panel owned by the dock.
     * @return Non-owning channel chart panel pointer.
     */
    ChannelChartPanel* ChartView() const;

    /**
     * @brief Return the process log panel owned by the dock.
     * @return Non-owning log panel pointer.
     */
    LogPanel* LogView() const;

    /**
     * @brief Expand or collapse the diagnostics region without destroying content.
     * @param expanded true to show the dock; false to hide it.
     */
    void SetExpanded(bool expanded);

    /**
     * @brief Return whether the diagnostics region is explicitly expanded.
     * @return true when the dock is not hidden.
     */
    bool IsExpanded() const;

    /**
     * @brief Return diagnostic tab titles for UI smoke tests.
     * @return Ordered Chinese tab titles.
     */
    QStringList TabTitles() const;

private:
    QTabWidget* m_tabs{nullptr};
    ReportPanel* m_reportView{nullptr};
    MaterialClosurePanel* m_materialClosureView{nullptr};
    ChannelChartPanel* m_chartView{nullptr};
    LogPanel* m_logView{nullptr};
};
