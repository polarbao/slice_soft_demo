#pragma once

#include <QJsonObject>
#include <QSize>
#include <QString>

struct PreviewPhysicalScale
{
    int dpix{0};
    int dpiy{0};
    double pixelsizexmm{0.0};
    double pixelsizeymm{0.0};
    bool available{false};
    QString warning;
};

class PreviewPhysicalScaleResolver final
{
public:
    /**
     * @brief Resolve physical pixel metadata from a package grid object.
     * @param grid Manifest or slice-report grid object.
     * @return Valid physical scale or an explicit square-pixel fallback warning.
     */
    static PreviewPhysicalScale Resolve(const QJsonObject& grid);

    /**
     * @brief Merge a candidate scale without replacing an already valid scale.
     * @param current Current preferred scale.
     * @param candidate Candidate scale from a lower-priority package document.
     * @return Merged scale.
     */
    static PreviewPhysicalScale Merge(
        const PreviewPhysicalScale& current,
        const PreviewPhysicalScale& candidate);

    /**
     * @brief Convert raster dimensions to display dimensions using physical pixel size.
     * @param rasterSize Source image size in pixels.
     * @param scale Resolved physical scale.
     * @return Display size with physical X/Y aspect correction.
     */
    static QSize DisplaySize(
        const QSize& rasterSize,
        const PreviewPhysicalScale& scale);

    /**
     * @brief Build user-visible DPI and physical pixel metadata.
     * @param scale Resolved physical scale.
     * @return Chinese status summary or explicit fallback warning.
     */
    static QString Summary(const PreviewPhysicalScale& scale);
};
