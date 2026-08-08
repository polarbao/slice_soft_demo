#pragma once

#include "HostPackageReviewController.h"

#include <QWidget>

/** @brief Compact six-channel print-pixel chart for production layers. */
class HostChannelChartWidget final : public QWidget
{
public:
    /**
     * @brief Creates an empty chart widget.
     * @param parent Optional Qt parent widget.
     */
    explicit HostChannelChartWidget(QWidget* parent = nullptr);

    /**
     * @brief Replaces chart data with verified production descriptors.
     * @param layers Layer descriptors in ascending layerIndex order.
     * @return This function does not return a value.
     */
    void SetLayers(const QVector<hostlayerdescriptor>& layers);

protected:
    /** @brief Draws axes, legend and six channel curves. */
    void paintEvent(QPaintEvent* event) override;

private:
    QVector<hostlayerdescriptor> m_layers;
};
