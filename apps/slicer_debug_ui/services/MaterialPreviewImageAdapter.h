#pragma once

#include "slicer_core/preview/MaterialPreviewComposer.h"

#include <QColor>
#include <QImage>
#include <QMap>
#include <QString>
#include <QStringList>

#include <optional>

/**
 * @brief Convert core RGBWSV preview DTOs to Qt display objects.
 */
class MaterialPreviewImageAdapter final
{
public:
    /**
     * @brief Return all production preview mode ids in UI order.
     * @return Stable UI mode ids.
     */
    static QStringList ModeIds();

    /**
     * @brief Convert a stable UI id to a core preview mode.
     * @param modeId Stable UI mode id.
     * @return Core preview mode, or empty for an unknown id.
     */
    static std::optional<slicer_core::MaterialPreviewMode>
    ModeFromId(const QString& modeId);

    /**
     * @brief Return the Chinese name of a stable UI preview mode.
     * @param modeId Stable UI mode id.
     * @return User-visible mode name.
     */
    static QString DisplayName(const QString& modeId);

    /**
     * @brief Build a display-only core palette from UI pseudo colors.
     * @param pseudoColors UI-only material colors.
     * @return Core material preview palette.
     */
    static slicer_core::MaterialPreviewPalette BuildPalette(
        const QMap<QString, QColor>& pseudoColors);

    /**
     * @brief Deep-copy one core RGBA result into Qt display coordinates.
     * @param result Core preview result in raw TIFF coordinates.
     * @return Display image with exactly one vertical coordinate flip.
     */
    static QImage ToDisplayImage(
        const slicer_core::MaterialPreviewResult& result);
};
