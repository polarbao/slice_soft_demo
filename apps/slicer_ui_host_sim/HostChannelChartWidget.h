#pragma once

#include "HostPackageReviewController.h"

#include <QWidget>

/** @brief 用于生产层的紧凑六通道打印像素图表。 */
class HostChannelChartWidget final : public QWidget
{
public:
    /**
     * @brief 创建空图表控件。
     * @param parent 可选的 Qt 父控件。
     */
    explicit HostChannelChartWidget(QWidget* parent = nullptr);

    /**
     * @brief 使用已验证的生产描述符替换图表数据。
     * @param layers 按 layerIndex 升序排列的层描述符。
     * @return 本函数无返回值。
     */
    void SetLayers(const QVector<hostlayerdescriptor>& layers);

protected:
    /** @brief 绘制坐标轴、图例与六条通道曲线。 */
    void paintEvent(QPaintEvent* event) override;

private:
    QVector<hostlayerdescriptor> m_layers;
};
