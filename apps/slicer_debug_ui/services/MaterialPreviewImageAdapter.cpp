#include "MaterialPreviewImageAdapter.h"

#include <cstring>

namespace
{

slicer_core::PreviewColor WithRgb(
    const slicer_core::PreviewColor& base,
    const QColor& color)
{
    slicer_core::PreviewColor result = base;
    result.red = static_cast<std::uint8_t>(color.red());
    result.green = static_cast<std::uint8_t>(color.green());
    result.blue = static_cast<std::uint8_t>(color.blue());
    return result;
}

}  // namespace

QStringList MaterialPreviewImageAdapter::ModeIds()
{
    return {
        QStringLiteral("rgb"),
        QStringLiteral("red"),
        QStringLiteral("green"),
        QStringLiteral("blue"),
        QStringLiteral("white"),
        QStringLiteral("support"),
        QStringLiteral("varnish"),
        QStringLiteral("rgb_white"),
        QStringLiteral("rgb_support"),
        QStringLiteral("rgb_varnish"),
        QStringLiteral("rgb_support_white_varnish"),
        QStringLiteral("occupancy"),
        QStringLiteral("empty"),
    };
}

std::optional<slicer_core::MaterialPreviewMode>
MaterialPreviewImageAdapter::ModeFromId(const QString& modeId)
{
    using slicer_core::MaterialPreviewMode;

    if (modeId == QStringLiteral("red"))
    {
        return MaterialPreviewMode::Red;
    }
    if (modeId == QStringLiteral("green"))
    {
        return MaterialPreviewMode::Green;
    }
    if (modeId == QStringLiteral("blue"))
    {
        return MaterialPreviewMode::Blue;
    }
    if (modeId == QStringLiteral("white"))
    {
        return MaterialPreviewMode::White;
    }
    if (modeId == QStringLiteral("support"))
    {
        return MaterialPreviewMode::Support;
    }
    if (modeId == QStringLiteral("varnish"))
    {
        return MaterialPreviewMode::Varnish;
    }
    if (modeId == QStringLiteral("rgb"))
    {
        return MaterialPreviewMode::Rgb;
    }
    if (modeId == QStringLiteral("rgb_white"))
    {
        return MaterialPreviewMode::RgbWhite;
    }
    if (modeId == QStringLiteral("rgb_support"))
    {
        return MaterialPreviewMode::RgbSupport;
    }
    if (modeId == QStringLiteral("rgb_varnish"))
    {
        return MaterialPreviewMode::RgbVarnish;
    }
    if (modeId == QStringLiteral("rgb_support_white_varnish"))
    {
        return MaterialPreviewMode::RgbSupportWhiteVarnish;
    }
    if (modeId == QStringLiteral("occupancy"))
    {
        return MaterialPreviewMode::Occupancy;
    }
    if (modeId == QStringLiteral("empty"))
    {
        return MaterialPreviewMode::Empty;
    }
    return std::nullopt;
}

QString MaterialPreviewImageAdapter::DisplayName(
    const QString& modeId)
{
    if (modeId == QStringLiteral("rgb"))
    {
        return QStringLiteral("RGB 真彩");
    }
    if (modeId == QStringLiteral("red"))
    {
        return QStringLiteral("R 通道");
    }
    if (modeId == QStringLiteral("green"))
    {
        return QStringLiteral("G 通道");
    }
    if (modeId == QStringLiteral("blue"))
    {
        return QStringLiteral("B 通道");
    }
    if (modeId == QStringLiteral("white"))
    {
        return QStringLiteral("W 白墨");
    }
    if (modeId == QStringLiteral("support"))
    {
        return QStringLiteral("S 支撑");
    }
    if (modeId == QStringLiteral("varnish"))
    {
        return QStringLiteral("V 光油");
    }
    if (modeId == QStringLiteral("rgb_white"))
    {
        return QStringLiteral("RGB + W");
    }
    if (modeId == QStringLiteral("rgb_support"))
    {
        return QStringLiteral("RGB + S");
    }
    if (modeId == QStringLiteral("rgb_varnish"))
    {
        return QStringLiteral("RGB + V");
    }
    if (modeId == QStringLiteral("rgb_support_white_varnish"))
    {
        return QStringLiteral("RGB + S + W + V");
    }
    if (modeId == QStringLiteral("occupancy"))
    {
        return QStringLiteral("材料占用");
    }
    if (modeId == QStringLiteral("empty"))
    {
        return QStringLiteral("真实空白");
    }
    return modeId;
}

slicer_core::MaterialPreviewPalette
MaterialPreviewImageAdapter::BuildPalette(
    const QMap<QString, QColor>& pseudoColors)
{
    slicer_core::MaterialPreviewPalette palette;
    palette.empty = WithRgb(
        palette.empty,
        pseudoColors.value(
            QStringLiteral("empty"),
            QColor(255, 255, 255)));
    palette.white = WithRgb(
        palette.white,
        pseudoColors.value(
            QStringLiteral("white"),
            QColor(0, 170, 255)));
    palette.support = WithRgb(
        palette.support,
        pseudoColors.value(
            QStringLiteral("support"),
            QColor(0, 255, 0)));
    palette.varnish = WithRgb(
        palette.varnish,
        pseudoColors.value(
            QStringLiteral("varnish"),
            QColor(127, 127, 127)));
    palette.occupancy = WithRgb(
        palette.occupancy,
        pseudoColors.value(
            QStringLiteral("occupancy"),
            QColor(80, 80, 80)));
    return palette;
}

QImage MaterialPreviewImageAdapter::ToDisplayImage(
    const slicer_core::MaterialPreviewResult& result)
{
    if (result.width == 0U
        || result.height == 0U
        || result.rgba.size()
            != static_cast<std::size_t>(result.width)
                * static_cast<std::size_t>(result.height) * 4U)
    {
        return {};
    }

    QImage rawImage(
        static_cast<int>(result.width),
        static_cast<int>(result.height),
        QImage::Format_RGBA8888);
    const std::size_t rowBytes =
        static_cast<std::size_t>(result.width) * 4U;
    for (std::uint32_t y = 0U; y < result.height; ++y)
    {
        std::memcpy(
            rawImage.scanLine(static_cast<int>(y)),
            result.rgba.data()
                + static_cast<std::size_t>(y) * rowBytes,
            rowBytes);
    }
    return rawImage.mirrored(false, true);
}
