#include "PreviewPhysicalScale.h"

#include <QJsonArray>

#include <cmath>

namespace
{

constexpr double kMillimetersPerInch = 25.4;
constexpr double kConsistencyToleranceMm = 1e-9;

double ReadArrayValue(
    const QJsonObject& object,
    const QString& key,
    const int index)
{
    const QJsonArray values = object.value(key).toArray();
    return index >= 0 && index < values.size()
        ? values.at(index).toDouble(0.0)
        : 0.0;
}

bool IsPositiveFinite(const double value)
{
    return std::isfinite(value) && value > 0.0;
}

}  // namespace

PreviewPhysicalScale PreviewPhysicalScaleResolver::Resolve(
    const QJsonObject& grid)
{
    PreviewPhysicalScale scale;
    scale.dpix = grid.value(QStringLiteral("dpiX")).toInt(0);
    scale.dpiy = grid.value(QStringLiteral("dpiY")).toInt(0);
    scale.pixelsizexmm = grid.value(QStringLiteral("pixelSizeXmm"))
                            .toDouble(ReadArrayValue(
                                grid,
                                QStringLiteral("pixelSizeMm"),
                                0));
    scale.pixelsizeymm = grid.value(QStringLiteral("pixelSizeYmm"))
                            .toDouble(ReadArrayValue(
                                grid,
                                QStringLiteral("pixelSizeMm"),
                                1));

    if (scale.dpix > 0 && !IsPositiveFinite(scale.pixelsizexmm))
    {
        scale.pixelsizexmm = kMillimetersPerInch
            / static_cast<double>(scale.dpix);
    }
    if (scale.dpiy > 0 && !IsPositiveFinite(scale.pixelsizeymm))
    {
        scale.pixelsizeymm = kMillimetersPerInch
            / static_cast<double>(scale.dpiy);
    }

    if (!IsPositiveFinite(scale.pixelsizexmm)
        || !IsPositiveFinite(scale.pixelsizeymm))
    {
        scale.warning =
            QStringLiteral("缺少 grid 物理像素元数据，按方形像素显示");
        return scale;
    }

    if (scale.dpix > 0
        && std::abs(
               scale.pixelsizexmm
               - kMillimetersPerInch / static_cast<double>(scale.dpix))
            > kConsistencyToleranceMm)
    {
        scale.warning =
            QStringLiteral("grid X DPI 与物理像素不一致，按方形像素显示");
        return scale;
    }
    if (scale.dpiy > 0
        && std::abs(
               scale.pixelsizeymm
               - kMillimetersPerInch / static_cast<double>(scale.dpiy))
            > kConsistencyToleranceMm)
    {
        scale.warning =
            QStringLiteral("grid Y DPI 与物理像素不一致，按方形像素显示");
        return scale;
    }

    scale.available = true;
    return scale;
}

PreviewPhysicalScale PreviewPhysicalScaleResolver::Merge(
    const PreviewPhysicalScale& current,
    const PreviewPhysicalScale& candidate)
{
    if (current.available)
    {
        return current;
    }
    if (candidate.available)
    {
        return candidate;
    }
    if (!current.warning.isEmpty())
    {
        return current;
    }
    return candidate;
}

QSize PreviewPhysicalScaleResolver::DisplaySize(
    const QSize& rasterSize,
    const PreviewPhysicalScale& scale)
{
    if (!rasterSize.isValid() || !scale.available)
    {
        return rasterSize;
    }

    const int physicalWidth = qMax(
        1,
        qRound(
            static_cast<double>(rasterSize.width())
            * scale.pixelsizexmm / scale.pixelsizeymm));
    return QSize(physicalWidth, rasterSize.height());
}

QString PreviewPhysicalScaleResolver::Summary(
    const PreviewPhysicalScale& scale)
{
    if (!scale.available)
    {
        return scale.warning.isEmpty()
            ? QStringLiteral("缺少 grid 物理像素元数据，按方形像素显示")
            : scale.warning;
    }

    QString text = QStringLiteral("像素=%1x%2 mm")
                       .arg(scale.pixelsizexmm, 0, 'f', 6)
                       .arg(scale.pixelsizeymm, 0, 'f', 6);
    if (scale.dpix > 0 && scale.dpiy > 0)
    {
        text.prepend(
            QStringLiteral("DPI=%1x%2  ")
                .arg(scale.dpix)
                .arg(scale.dpiy));
    }
    return text;
}
