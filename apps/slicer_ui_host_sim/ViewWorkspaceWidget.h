#pragma once

#include "camera/ViewModeSwitch.h"

#include <QWidget>

class QLabel;
class QStackedWidget;
class QToolButton;

/**
 * @brief Central host workspace with immediate top/three-dimensional switching.
 */
class ViewWorkspaceWidget final : public QWidget
{
public:
    /** @brief Creates a display-only dual-view workspace. */
    explicit ViewWorkspaceWidget(QWidget* parent = nullptr);

    /** @brief Selects a view without changing scene or job state. */
    void SetMode(HostViewMode mode);

    /** @brief Returns the currently visible presentation mode. */
    [[nodiscard]] HostViewMode Mode() const;

    /** @brief Shows an explicit fail-closed view diagnostic. */
    void ShowViewError(const QString& message);

private:
    void ApplyMode();

    ViewModeSwitch m_switch;
    QToolButton* m_topButton{nullptr};
    QToolButton* m_threeDButton{nullptr};
    QStackedWidget* m_canvasStack{nullptr};
    QLabel* m_errorLabel{nullptr};
};
