#pragma once

#include "camera/CameraController.h"
#include "camera/ViewModeSwitch.h"

#include <QImage>
#include <QSize>
#include <QStringList>
#include <QWidget>

class QLabel;
class QComboBox;
class QStackedWidget;
class QToolButton;
class ThreeDCanvasWidget;
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

    /** @brief Updates transient top-view pixels without resetting navigation. */
    void UpdateTopImage(const QImage& image);

    /** @brief Clears the top-view image after an explicit refresh failure. */
    void ClearTopImage();

    /** @brief Returns a stable render target size for top-view generation. */
    [[nodiscard]] QSize TopRenderSize() const;

    /** @brief Returns the retained top-view canvas for host-local navigation. */
    [[nodiscard]] TopViewCanvasWidget* TopCanvas() const;

    /**
     * @brief Replaces the visible three-dimensional frame.
     * @param image Complete host-local renderer output.
     */
    void SetThreeDImage(const QImage& image);

    /** @brief Clears the three-dimensional image after a closed failure. */
    void ClearThreeDImage();

    /**
     * @brief Fits the three-dimensional camera to authoritative world bounds.
     * @param bounds Scene bounds in millimetres.
     */
    void SetThreeDSceneBounds(const CameraBounds& bounds);

    /** @brief Returns a stable three-dimensional raster target size. */
    [[nodiscard]] QSize ThreeDRenderSize() const;

    /** @brief Returns the retained three-dimensional camera canvas. */
    [[nodiscard]] ThreeDCanvasWidget* ThreeDCanvas() const;

    /** @brief Applies the host display setting to the local 3D camera. */
    void SetThreeDProjection(slicer::render::Projection projection);

private:
    void ApplyMode();

    ViewModeSwitch m_switch;
    QToolButton* m_topButton{nullptr};
    QToolButton* m_threeDButton{nullptr};
    QWidget* m_threeDControls{nullptr};
    QComboBox* m_presetCombo{nullptr};
    QComboBox* m_projectionCombo{nullptr};
    QLabel* m_selectionLabel{nullptr};
    QStackedWidget* m_canvasStack{nullptr};
    TopViewCanvasWidget* m_topCanvas{nullptr};
    ThreeDCanvasWidget* m_threeDCanvas{nullptr};
    QLabel* m_errorLabel{nullptr};
};
