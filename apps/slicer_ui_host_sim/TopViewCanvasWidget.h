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
 * @brief 显示支持本地平移与缩放的宿主俯视图像。
 *
 * 该控件绝不调用切片模块，只对已保留的 QImage 执行显示变换；
 * QImage 必须由权威 ViewData 快照生成。
 */
class TopViewCanvasWidget final : public QWidget
{
public:
    /** @brief 创建一个空的俯视画布。 */
    explicit TopViewCanvasWidget(QWidget* parent = nullptr);

    /**
     * @brief 替换保留的俯视图像并重置导航。
     * @param image 从 ViewData 渲染的完整的仅显示图像。
     */
    void SetImage(const QImage& image);

    /**
     * @brief 替换像素，同时保留局部平移和缩放。
     * @param image 具有当前视口大小的完整仅显示图像。
     */
    void UpdateImage(const QImage& image);

    /** @brief 清除保留图像并返回到空状态。 */
    void ClearImage();

    /** @brief 恢复适合图像的缩放和平移居中位置。 */
    void ResetView();

    /**
     * @brief 以指定画布位置为中心应用宿主本地缩放。
     * @param factor 正相对缩放倍数。
     * @param anchor 视觉上保持固定的画布坐标。
     */
    void ZoomAt(double factor, const QPointF& anchor);

    /**
     * @brief 应用宿主本地平移增量。
     * @param delta 画布空间移动（以像素为单位）。
     */
    void PanBy(const QPointF& delta);

    /** @brief 当保留完整图像时返回 true。 */
    [[nodiscard]] bool HasImage() const;

    /** @brief 返回当前的相对缩放系数。 */
    [[nodiscard]] double ZoomFactor() const;

    /** @brief 返回当前画布空间平移偏移量。 */
    [[nodiscard]] QPointF PanOffset() const;

    /**
     * @brief 安装宿主本地模型拖动回调。
     * @param begin 使用渲染图像像素调用并返回拖动是否开始。
     * @param update 为渲染图像像素中的每个本地指针移动而调用。
     * @param finish 当接受的拖动被释放时调用一次。
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
