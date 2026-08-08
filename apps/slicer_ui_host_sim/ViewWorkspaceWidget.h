#pragma once

#include "camera/ViewModeSwitch.h"

#include <QImage>
#include <QSize>
#include <QStringList>
#include <QWidget>

class QLabel;
class QStackedWidget;
class QToolButton;
class TopViewCanvasWidget;

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

    /**
     * @brief Updates host-local model selection without a module call.
     * @param instanceIds Stable identities selected in the model list.
     */
    void SetSelectedInstances(const QStringList& instanceIds);

    /**
     * @brief Replaces the visible top-view frame with a rendered image.
     * @param image Complete display-only top ViewData rendering.
     */
    void SetTopImage(const QImage& image);

    /** @brief Clears the top-view image after an explicit refresh failure. */
    void ClearTopImage();

    /** @brief Returns a stable render target size for top-view generation. */
    [[nodiscard]] QSize TopRenderSize() const;

    /** @brief Returns the retained top-view canvas for host-local navigation. */
    [[nodiscard]] TopViewCanvasWidget* TopCanvas() const;

private:
    void ApplyMode();

    ViewModeSwitch m_switch;
    QToolButton* m_topButton{nullptr};
    QToolButton* m_threeDButton{nullptr};
    QLabel* m_selectionLabel{nullptr};
    QStackedWidget* m_canvasStack{nullptr};
    TopViewCanvasWidget* m_topCanvas{nullptr};
    QLabel* m_errorLabel{nullptr};
};
