#pragma once

#include "../models/SceneDocument.h"
#include "../models/SceneSelectionModel.h"

#include <QPointF>
#include <QImage>
#include <QHash>
#include <QWidget>

#include <cstddef>
#include <optional>

class ModelTopViewWidget final : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Create a read-only +Z model top-view canvas.
     * @param document Scene document rendered by the widget.
     * @param selectionModel Shared scene selection state.
     * @param parent QWidget owner.
     */
    explicit ModelTopViewWidget(
        SceneDocument* document,
        SceneSelectionModel* selectionModel,
        QWidget* parent = nullptr);

    /**
     * @brief Reset view-only camera state to fit current geometry.
     */
    void FitToView();

    /**
     * @brief Report whether current geometry can be painted.
     * @return True for ready or blocked scene geometry.
     */
    bool HasRenderableGeometry() const;

    /**
     * @brief Limit the scene prefix shown while a batch awaits final layout.
     * @param itemLimit Maximum item count, or no value to show the full scene.
     */
    void SetPresentationItemLimit(
        std::optional<std::size_t> itemLimit);

    /**
     * @brief Return the number of scene items exposed to presentation.
     * @return Full scene count or the active presentation prefix count.
     */
    std::size_t PresentationItemCount() const;

signals:
    void SigInstanceSelected(const QString& instanceId);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private slots:
    void OnDocumentChanged();
    void OnSelectionChanged(const QString& instanceId);

private:
    struct Camera
    {
        double scale{1.0};
        double centerxmm{0.0};
        double centerymm{0.0};
        QRectF viewport;
    };

    Camera BuildCamera() const;
    QPointF WorldToScreen(
        const slicer_core::SceneViewPoint& point,
        const Camera& camera) const;
    slicer_core::SceneViewPoint ScreenToWorld(
        const QPointF& point,
        const Camera& camera) const;
    void DrawGrid(QPainter& painter, const Camera& camera) const;
    void DrawGeometry(QPainter& painter, const Camera& camera) const;
    QImage SurfaceImage(
        const slicer_core::SceneViewSurfacePreview& preview) const;
    void DrawStatus(QPainter& painter) const;

    SceneDocument* m_document{nullptr};
    SceneSelectionModel* m_selectionModel{nullptr};
    std::optional<std::size_t> m_presentationItemLimit;
    mutable QHash<QString, QImage> m_surfaceCache;
};
