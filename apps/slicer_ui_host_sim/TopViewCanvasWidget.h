#pragma once

#include <QImage>
#include <QPoint>
#include <QPointF>
#include <QWidget>

#include <functional>

class QMouseEvent;
class QPaintEvent;
class QWheelEvent;

/**
 * @brief Presents one host-local top-view image with local pan and zoom.
 *
 * The widget never calls the slicer module. It only transforms a retained
 * QImage produced from an authoritative ViewData snapshot.
 */
class TopViewCanvasWidget final : public QWidget
{
public:
    /** @brief Creates an empty top-view canvas. */
    explicit TopViewCanvasWidget(QWidget* parent = nullptr);

    /**
     * @brief Replaces the retained top-view image and resets navigation.
     * @param image Complete display-only image rendered from ViewData.
     */
    void SetImage(const QImage& image);

    /**
     * @brief Replaces pixels while preserving local pan and zoom.
     * @param image Complete display-only image with the current viewport size.
     */
    void UpdateImage(const QImage& image);

    /** @brief Clears the retained image and returns to the empty state. */
    void ClearImage();

    /** @brief Restores the image-fit zoom and centered pan position. */
    void ResetView();

    /**
     * @brief Applies a host-local zoom around one canvas position.
     * @param factor Positive relative zoom multiplier.
     * @param anchor Canvas coordinate that remains visually fixed.
     */
    void ZoomAt(double factor, const QPointF& anchor);

    /**
     * @brief Applies a host-local pan delta.
     * @param delta Canvas-space movement in pixels.
     */
    void PanBy(const QPointF& delta);

    /** @brief Returns true when a complete image is retained. */
    [[nodiscard]] bool HasImage() const;

    /** @brief Returns the current relative zoom factor. */
    [[nodiscard]] double ZoomFactor() const;

    /** @brief Returns the current canvas-space pan offset. */
    [[nodiscard]] QPointF PanOffset() const;

    /**
     * @brief Installs host-local model drag callbacks.
     * @param begin Called with rendered-image pixels and returns whether drag starts.
     * @param update Called for each local pointer move in rendered-image pixels.
     * @param finish Called once when the accepted drag is released.
     */
    void SetModelDragCallbacks(
        std::function<bool(const QPointF&)> begin,
        std::function<void(const QPointF&)> update,
        std::function<void()> finish);

protected:
    void paintEvent(QPaintEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    QPointF MapCanvasToImage(const QPointF& canvasPoint) const;

    QImage m_image;
    QPointF m_panOffset;
    QPoint m_lastMousePosition;
    double m_zoomFactor{1.0};
    std::function<bool(const QPointF&)> m_dragBeginCallback;
    std::function<void(const QPointF&)> m_dragUpdateCallback;
    std::function<void()> m_dragFinishCallback;
    bool m_draggingModel{false};
    bool m_panning{false};
};
