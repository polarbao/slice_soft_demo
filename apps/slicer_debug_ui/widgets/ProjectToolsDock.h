#pragma once

#include <QDockWidget>

/**
 * @brief Hosts project paths and compatibility tools in a collapsible left dock.
 */
class ProjectToolsDock final : public QDockWidget
{
    Q_OBJECT

public:
    /**
     * @brief Create the project tools dock around the existing project panel.
     * @param content Existing project and compatibility tools widget.
     * @param parent Qt parent widget, normally MainWindow.
     */
    explicit ProjectToolsDock(
        QWidget* content,
        QWidget* parent = nullptr);

    /**
     * @brief Expand or collapse the project tools region.
     * @param expanded true to show the dock; false to hide it.
     */
    void SetExpanded(bool expanded);

    /**
     * @brief Return whether the project tools region is expanded.
     * @return true when the dock is visible.
     */
    bool IsExpanded() const;
};
