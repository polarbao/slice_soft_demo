#include "HostChannelChartWidget.h"

#include <QPainter>
#include <QPaintEvent>
#include <QPainterPath>

#include <algorithm>
#include <array>

namespace
{
constexpr std::array<const char*, 6> ChannelNames{
    "R", "G", "B", "W", "S", "V"};

const std::array<QColor, 6> ChannelColors{
    QColor{210, 50, 50},
    QColor{35, 145, 75},
    QColor{45, 95, 210},
    QColor{40, 175, 205},
    QColor{70, 190, 70},
    QColor{125, 125, 125}};
}

HostChannelChartWidget::HostChannelChartWidget(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("hostPackageChannelChart"));
    setMinimumHeight(180);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void HostChannelChartWidget::SetLayers(
    const QVector<hostlayerdescriptor>& layers)
{
    m_layers = layers;
    update();
}

void HostChannelChartWidget::paintEvent(QPaintEvent* event)
{
    QWidget::paintEvent(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), palette().base());

    const QRectF plot = QRectF(rect()).adjusted(42.0, 28.0, -16.0, -28.0);
    painter.setPen(QPen(palette().mid().color(), 1.0));
    painter.drawRect(plot);

    for (std::size_t index = 0; index < ChannelNames.size(); ++index)
    {
        const int x = 12 + static_cast<int>(index) * 54;
        painter.setPen(QPen(ChannelColors[index], 2.0));
        painter.drawLine(x, 13, x + 15, 13);
        painter.setPen(palette().text().color());
        painter.drawText(x + 20, 17, QString::fromLatin1(ChannelNames[index]));
    }

    if (m_layers.isEmpty() || plot.width() <= 1.0 || plot.height() <= 1.0)
    {
        painter.drawText(plot, Qt::AlignCenter, QStringLiteral("尚无逐层通道统计"));
        return;
    }

    quint64 maximum{1};
    for (const hostlayerdescriptor& layer : m_layers)
    {
        for (const quint64 value : layer.printpixels.values)
        {
            maximum = (std::max)(maximum, value);
        }
    }

    const int pointCount = m_layers.size();
    for (std::size_t channelIndex = 0;
         channelIndex < ChannelNames.size();
         ++channelIndex)
    {
        QPainterPath path;
        for (int layerIndex = 0; layerIndex < pointCount; ++layerIndex)
        {
            const qreal x = pointCount == 1
                ? plot.left()
                : plot.left() + plot.width()
                    * static_cast<qreal>(layerIndex)
                    / static_cast<qreal>(pointCount - 1);
            const qreal ratio = static_cast<qreal>(
                m_layers.at(layerIndex).printpixels.values[channelIndex])
                / static_cast<qreal>(maximum);
            const qreal y = plot.bottom() - ratio * plot.height();
            if (layerIndex == 0)
            {
                path.moveTo(x, y);
            }
            else
            {
                path.lineTo(x, y);
            }
        }
        painter.setPen(QPen(ChannelColors[channelIndex], 1.5));
        painter.drawPath(path);
    }

    painter.setPen(palette().text().color());
    painter.drawText(
        QRectF(0.0, plot.top(), 38.0, 20.0),
        Qt::AlignRight | Qt::AlignVCenter,
        QString::number(maximum));
    painter.drawText(
        QRectF(plot.left(), plot.bottom() + 4.0, plot.width(), 20.0),
        Qt::AlignCenter,
        QStringLiteral("生产层 0 - %1").arg(pointCount - 1));
}
