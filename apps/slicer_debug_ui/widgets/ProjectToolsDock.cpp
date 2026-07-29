#include "ProjectToolsDock.h"

ProjectToolsDock::ProjectToolsDock(
    QWidget* content,
    QWidget* parent)
    : QDockWidget(
          QStringLiteral("项目与高级工具"),
          parent)
{
    setObjectName(QStringLiteral("projectToolsDock"));
    setAllowedAreas(Qt::LeftDockWidgetArea);
    setFeatures(
        QDockWidget::DockWidgetClosable
        | QDockWidget::DockWidgetMovable
        | QDockWidget::DockWidgetFloatable);
    setMinimumWidth(280);
    setMaximumWidth(440);
    setWidget(content);
    hide();
}

void ProjectToolsDock::SetExpanded(const bool expanded)
{
    setVisible(expanded);
    if (expanded)
    {
        raise();
    }
}

bool ProjectToolsDock::IsExpanded() const
{
    return !isHidden();
}
