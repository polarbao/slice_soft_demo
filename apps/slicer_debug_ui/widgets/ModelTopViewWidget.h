#pragma once

#include "../models/SceneDocument.h"
#include "../models/SceneSelectionModel.h"

#include <QPointF>
#include <QWidget>

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
    void DrawStatus(QPainter& painter) const;

    SceneDocument* m_document{nullptr};
    SceneSelectionModel* m_selectionModel{nullptr};
};
