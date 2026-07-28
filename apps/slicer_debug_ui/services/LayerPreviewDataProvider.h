#pragma once

#include "PackageLoader.h"
#include "PreviewPhysicalScale.h"

#include <QColor>
#include <QJsonObject>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>

struct LayerPreviewFrame
{
    QString path;
    QString channel;
    QString kind;
    int layerindex{-1};
    int printpixels{0};
    int displaynonzeropixels{0};
    int nonzeropixels{0};
    int maxvalue{0};
};

struct LayerPreviewLayerStats
{
    int layerindex{-1};
    double zmm{0.0};
    int rgbprintpixels{0};
    int whiteprintpixels{0};
    int supportprintpixels{0};
    int varnishprintpixels{0};
    int modelprintpixels{0};
    int supportcomponentcount{0};
    int smallcomponentcount{0};
    int tinycomponentcount{0};
    int texturesurfacepixels{0};
    int modelfillpixels{0};
    int internalvoidsupportpixels{0};
    int outervarnishpixels{0};
    int outersurfacevarnishpixels{0};
    int innersurfacevarnishpixels{0};
    int uppersurfacesupportpixels{0};
    QString supporttypesummary;
    QStringList fillwarnings;
};

struct LayerPreviewSemanticPolicy
{
    QString modelfillmaterial;
    QString modelfillscope;
    QString supportplacement;
    QString supportsource;
    QString semanticpriority;
    bool internalvoidsupportenabled{false};
    bool outervarnishenabled{false};
    int outervarnishthicknesspx{0};
    double outervarnishthicknessmm{0.0};
    bool surfacevarnishenabled{false};
    int surfacevarnishthicknesspx{0};
};

struct LayerPreviewPackage
{
    QString packagedir;
    int layercount{0};
    int widthpx{0};
    int heightpx{0};
    double layerheightmm{0.0};
    QVector<int> layerindices;
    QStringList channels;
    QMap<int, QMap<QString, LayerPreviewFrame>> frames;
    QMap<int, LayerPreviewLayerStats> layerstats;
    QMap<QString, QColor> pseudocolors;
    LayerPreviewSemanticPolicy semanticpolicy;
    PreviewPhysicalScale physicalscale;
};

class LayerPreviewDataProvider
{
public:
    /**
     * @brief Load layer preview data from an output package.
     * @param package Package summary produced by PackageLoader.
     * @return Derived UI-only layer preview package.
     */
    LayerPreviewPackage Load(const PackageSummary& package);

    /**
     * @brief Load only manifest, report, palette, and semantic metadata.
     * @param package Package summary produced by PackageLoader.
     * @return UI metadata without reading preview reports or preview images.
     */
    LayerPreviewPackage LoadProductionMetadata(
        const PackageSummary& package);

    /**
     * @brief Return the last non-fatal loading error or warning.
     * @return Human-readable error text. Empty when no issue was detected.
     */
    QString ErrorString() const;

    /**
     * @brief Normalize report and file channel names to UI channel ids.
     * @param channel Channel value from preview report.
     * @param type Optional preview type field.
     * @return UI channel id such as rgb, texture_rgb, white, support, varnish.
     */
    static QString NormalizeChannel(const QString& channel, const QString& type = QString());

    /**
     * @brief Convert a UI channel id to a Chinese display label.
     * @param channel UI channel id.
     * @return User-visible display name.
     */
    static QString DisplayName(const QString& channel);

private:
    QJsonObject LoadJsonObject(const QString& path);
    void ReadManifest(const PackageSummary& package, LayerPreviewPackage* preview);
    void ReadPreviewReport(const PackageSummary& package, LayerPreviewPackage* preview);
    void ReadSliceReport(const PackageSummary& package, LayerPreviewPackage* preview);
    void ReadFallbackPreviewFiles(const PackageSummary& package, LayerPreviewPackage* preview);
    void EnsureLayerIndices(LayerPreviewPackage* preview) const;
    void EnsureChannelOrder(LayerPreviewPackage* preview) const;
    void AddFrame(LayerPreviewPackage* preview, const LayerPreviewFrame& frame) const;
    LayerPreviewPackage CreateBasePackage(
        const PackageSummary& package) const;

    static QColor ColorFromArray(const QJsonObject& object, const QString& key, const QColor& fallback);
    static QString ClassifyPath(const QString& path);
    static int ParseLayerIndex(const QString& path);

    QString m_error;
};
