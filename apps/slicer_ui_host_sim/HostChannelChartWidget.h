#pragma once

#include "HostPackageReviewController.h"

#include <QStringList>
#include <QWidget>

/** @brief 用于生产层的紧凑协议通道打印像素图表。 */
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

    /** @brief 设置由已验证 manifest 声明的冻结通道顺序。 */
    void SetChannels(const QStringList& channels);

protected:
    /** @brief 绘制坐标轴、图例与当前协议通道曲线。 */
    void paintEvent(QPaintEvent* event) override;

private:
    QVector<hostlayerdescriptor> m_layers;
    QStringList m_channels{
        QStringLiteral("R"), QStringLiteral("G"), QStringLiteral("B"),
        QStringLiteral("W"), QStringLiteral("S"), QStringLiteral("V")};
};
