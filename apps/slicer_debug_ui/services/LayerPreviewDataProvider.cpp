#include "LayerPreviewDataProvider.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>

namespace
{

int ReadInt(const QJsonObject& object, const QString& key)
{
    return object.value(key).toInt(0);
}

QStringList ReadStringArray(const QJsonArray& array)
{
    QStringList result;
    for (const QJsonValue& value : array)
    {
        result.push_back(value.toString());
    }
    return result;
}

}  // namespace

LayerPreviewPackage LayerPreviewDataProvider::Load(const PackageSummary& package)
{
    m_error.clear();

    LayerPreviewPackage preview;
    preview.packagedir = package.package_dir;
    preview.pseudocolors.insert("empty", QColor(255, 255, 255));
    preview.pseudocolors.insert("support", QColor(0, 255, 0));
    preview.pseudocolors.insert("white", QColor(0, 170, 255));
    preview.pseudocolors.insert("varnish", QColor(127, 127, 127));
    preview.pseudocolors.insert("occupancy", QColor(80, 80, 80));
    preview.pseudocolors.insert("diagnostic", QColor(255, 180, 0));

    ReadManifest(package, &preview);
    ReadSliceReport(package, &preview);
    ReadPreviewReport(package, &preview);
    if (preview.frames.isEmpty())
    {
        ReadFallbackPreviewFiles(package, &preview);
    }

    EnsureLayerIndices(&preview);
    EnsureChannelOrder(&preview);
    return preview;
}

QString LayerPreviewDataProvider::ErrorString() const
{
    return m_error;
}

QString LayerPreviewDataProvider::NormalizeChannel(const QString& channel, const QString& type)
{
    const QString normalized = channel.trimmed().toLower();
    const QString normalizedType = type.trimmed().toLower();
    if (normalized == "texture_rgb" || normalizedType == "texture_rgb")
    {
        return "texture_rgb";
    }
    if (normalized == "rgb" || normalized == "model_rgb" || normalized == "true_rgb" || normalizedType == "model_rgb")
    {
        return "rgb";
    }
    if (normalized == "w" || normalized == "white" || normalizedType == "white_w")
    {
        return "white";
    }
    if (normalized == "s" || normalized == "support" || normalizedType == "support_s")
    {
        return "support";
    }
    if (normalized == "v" || normalized == "varnish" || normalizedType == "varnish_v")
    {
        return "varnish";
    }
    if (normalized == "occupancy")
    {
        return "occupancy";
    }
    if (normalized == "diagnostic")
    {
        return "diagnostic";
    }
    return normalized.isEmpty() ? NormalizeChannel(ClassifyPath(type)) : normalized;
}

QString LayerPreviewDataProvider::DisplayName(const QString& channel)
{
    if (channel == "texture_rgb")
    {
        return "纹理 RGB";
    }
    if (channel == "rgb")
    {
        return "RGB";
    }
    if (channel == "white")
    {
        return "白墨";
    }
    if (channel == "support")
    {
        return "支撑";
    }
    if (channel == "varnish")
    {
        return "光油";
    }
    if (channel == "occupancy")
    {
        return "占用";
    }
    if (channel == "diagnostic")
    {
        return "诊断";
    }
    return channel;
}

QJsonObject LayerPreviewDataProvider::LoadJsonObject(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return {};
    }

    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        m_error = QFileInfo(path).fileName() + " 解析失败：" + parseError.errorString();
        return {};
    }
    return document.object();
}

void LayerPreviewDataProvider::ReadManifest(const PackageSummary& package, LayerPreviewPackage* preview)
{
    const QJsonObject root = LoadJsonObject(package.manifest_path);
    if (root.isEmpty())
    {
        return;
    }

    const QJsonObject grid = root.value("grid").toObject();
    preview->layercount = ReadInt(grid, "layerCount");
    preview->widthpx = ReadInt(grid, "widthPx");
    preview->heightpx = ReadInt(grid, "heightPx");
    preview->layerheightmm = grid.value("layerThicknessMm").toDouble(0.0);

    const QJsonArray layers = root.value("layers").toArray(root.value("tiff").toObject().value("layers").toArray());
    for (const QJsonValue& value : layers)
    {
        const QJsonObject object = value.toObject();
        const int layerIndex = object.value("index").toInt(object.value("layerIndex").toInt(-1));
        if (layerIndex < 0)
        {
            continue;
        }
        if (!preview->layerindices.contains(layerIndex))
        {
            preview->layerindices.push_back(layerIndex);
        }
        LayerPreviewLayerStats stats = preview->layerstats.value(layerIndex);
        stats.layerindex = layerIndex;
        stats.zmm = object.value("zMm").toDouble(stats.zmm);
        preview->layerstats.insert(layerIndex, stats);
    }
}

void LayerPreviewDataProvider::ReadPreviewReport(const PackageSummary& package, LayerPreviewPackage* preview)
{
    const QString reportPath = QDir(package.package_dir).filePath("reports/preview_report.json");
    const QJsonObject root = LoadJsonObject(reportPath);
    if (root.isEmpty())
    {
        return;
    }

    const QJsonObject pseudoColors = root.value("pseudoColors").toObject();
    preview->pseudocolors.insert("empty", ColorFromArray(pseudoColors, "empty", preview->pseudocolors.value("empty")));
    preview->pseudocolors.insert("support", ColorFromArray(pseudoColors, "support", preview->pseudocolors.value("support")));
    preview->pseudocolors.insert("white", ColorFromArray(pseudoColors, "white", preview->pseudocolors.value("white")));
    preview->pseudocolors.insert("varnish", ColorFromArray(pseudoColors, "varnish", preview->pseudocolors.value("varnish")));

    QSet<QString> seen;
    for (const QString& key : QStringList{"files", "generated", "previewFiles"})
    {
        const QJsonArray files = root.value(key).toArray();
        for (const QJsonValue& value : files)
        {
            if (!value.isObject() && !value.isString())
            {
                continue;
            }

            LayerPreviewFrame frame;
            if (value.isString())
            {
                frame.path = QDir(package.package_dir).filePath(value.toString());
                frame.channel = ClassifyPath(frame.path);
                frame.layerindex = ParseLayerIndex(frame.path);
            }
            else
            {
                const QJsonObject object = value.toObject();
                const QString relativePath = object.value("path").toString(object.value("file").toString());
                if (relativePath.isEmpty())
                {
                    continue;
                }
                frame.path = QDir(package.package_dir).filePath(relativePath);
                frame.kind = object.value("kind").toString();
                frame.channel = NormalizeChannel(object.value("channel").toString(), object.value("type").toString());
                frame.layerindex = object.value("layerIndex").toInt(-1);
                frame.printpixels = ReadInt(object, "printPixels");
                frame.displaynonzeropixels = ReadInt(object, "displayNonZeroPixels");
                frame.nonzeropixels = ReadInt(object, "nonZeroPixels");
                frame.maxvalue = ReadInt(object, "maxValue");
            }

            const QString dedupeKey = QString("%1|%2|%3").arg(frame.layerindex).arg(frame.channel, frame.path);
            if (seen.contains(dedupeKey))
            {
                continue;
            }
            seen.insert(dedupeKey);
            AddFrame(preview, frame);
        }
    }
}

void LayerPreviewDataProvider::ReadSliceReport(const PackageSummary& package, LayerPreviewPackage* preview)
{
    const QString reportPath = QDir(package.package_dir).filePath("reports/slice_report.json");
    const QJsonObject root = LoadJsonObject(reportPath);
    if (root.isEmpty())
    {
        return;
    }

    const QJsonObject grid = root.value("grid").toObject();
    if (preview->layercount <= 0)
    {
        preview->layercount = ReadInt(grid, "layerCount");
    }
    if (preview->widthpx <= 0)
    {
        preview->widthpx = ReadInt(grid, "widthPx");
    }
    if (preview->heightpx <= 0)
    {
        preview->heightpx = ReadInt(grid, "heightPx");
    }
    if (preview->layerheightmm <= 0.0)
    {
        preview->layerheightmm = grid.value("layerThicknessMm").toDouble(0.0);
    }

    const QJsonArray layers = root.value("layers").toArray();
    for (const QJsonValue& value : layers)
    {
        const QJsonObject object = value.toObject();
        const int layerIndex = object.value("layerIndex").toInt(object.value("index").toInt(-1));
        if (layerIndex < 0)
        {
            continue;
        }

        LayerPreviewLayerStats stats = preview->layerstats.value(layerIndex);
        stats.layerindex = layerIndex;
        stats.zmm = object.value("zMm").toDouble(stats.zmm);
        stats.rgbprintpixels = ReadInt(object, "rgbPrintPixels");
        stats.whiteprintpixels = ReadInt(object, "whitePrintPixels");
        stats.supportprintpixels = ReadInt(object, "supportPrintPixels");
        stats.varnishprintpixels = ReadInt(object, "varnishPrintPixels");
        stats.modelprintpixels = ReadInt(object, "modelPrintPixels");
        stats.fillwarnings = ReadStringArray(object.value("fillWarnings").toArray());

        const QJsonObject supportConnectivity = object.value("supportConnectivity").toObject();
        stats.supportcomponentcount = ReadInt(supportConnectivity, "componentCount");
        stats.smallcomponentcount = ReadInt(supportConnectivity, "smallComponentCount");
        stats.tinycomponentcount = ReadInt(supportConnectivity, "tinyComponentCount");

        preview->layerstats.insert(layerIndex, stats);
        if (!preview->layerindices.contains(layerIndex))
        {
            preview->layerindices.push_back(layerIndex);
        }
    }
}

void LayerPreviewDataProvider::ReadFallbackPreviewFiles(const PackageSummary& package, LayerPreviewPackage* preview)
{
    for (const QString& path : package.preview_paths)
    {
        LayerPreviewFrame frame;
        frame.path = path;
        frame.channel = ClassifyPath(path);
        frame.layerindex = ParseLayerIndex(path);
        AddFrame(preview, frame);
    }
}

void LayerPreviewDataProvider::EnsureLayerIndices(LayerPreviewPackage* preview) const
{
    if (preview->layercount <= 0)
    {
        int maxLayer = -1;
        for (auto iterator = preview->frames.cbegin(); iterator != preview->frames.cend(); ++iterator)
        {
            maxLayer = qMax(maxLayer, iterator.key());
        }
        preview->layercount = maxLayer + 1;
    }

    if (preview->layerindices.isEmpty() && preview->layercount > 0)
    {
        for (int layerIndex = 0; layerIndex < preview->layercount; ++layerIndex)
        {
            preview->layerindices.push_back(layerIndex);
        }
    }

    std::sort(preview->layerindices.begin(), preview->layerindices.end());
}

void LayerPreviewDataProvider::EnsureChannelOrder(LayerPreviewPackage* preview) const
{
    QSet<QString> channels;
    for (auto layerIterator = preview->frames.cbegin(); layerIterator != preview->frames.cend(); ++layerIterator)
    {
        for (auto frameIterator = layerIterator.value().cbegin(); frameIterator != layerIterator.value().cend(); ++frameIterator)
        {
            channels.insert(frameIterator.key());
        }
    }

    channels.insert("occupancy");
    channels.insert("diagnostic");

    const QStringList ordered{"texture_rgb", "rgb", "white", "support", "varnish", "occupancy", "diagnostic"};
    preview->channels.clear();
    for (const QString& channel : ordered)
    {
        if (channels.contains(channel))
        {
            preview->channels.push_back(channel);
        }
    }
}

void LayerPreviewDataProvider::AddFrame(LayerPreviewPackage* preview, const LayerPreviewFrame& frame) const
{
    if (frame.layerindex < 0 || frame.path.isEmpty() || frame.channel.isEmpty())
    {
        return;
    }
    preview->frames[frame.layerindex].insert(frame.channel, frame);
    if (!preview->layerindices.contains(frame.layerindex))
    {
        preview->layerindices.push_back(frame.layerindex);
    }
}

QColor LayerPreviewDataProvider::ColorFromArray(const QJsonObject& object, const QString& key, const QColor& fallback)
{
    const QJsonArray array = object.value(key).toArray();
    if (array.size() < 3)
    {
        return fallback;
    }
    return QColor(array.at(0).toInt(fallback.red()), array.at(1).toInt(fallback.green()), array.at(2).toInt(fallback.blue()));
}

QString LayerPreviewDataProvider::ClassifyPath(const QString& path)
{
    const QString base = QFileInfo(path).completeBaseName().toLower();
    if (base.contains("texture_rgb"))
    {
        return "texture_rgb";
    }
    if (base.contains("model_rgb") || base.contains("rgb"))
    {
        return "rgb";
    }
    if (base.contains("white") || base.contains("_w") || base.endsWith("w"))
    {
        return "white";
    }
    if (base.contains("support") || base.contains("_s") || base.endsWith("s"))
    {
        return "support";
    }
    if (base.contains("varnish") || base.contains("_v") || base.endsWith("v"))
    {
        return "varnish";
    }
    return "preview";
}

int LayerPreviewDataProvider::ParseLayerIndex(const QString& path)
{
    const QString base = QFileInfo(path).completeBaseName();
    const QRegularExpression expression("(?:layer|z|_)(\\d+)");
    const QRegularExpressionMatch match = expression.match(base);
    if (match.hasMatch())
    {
        return match.captured(1).toInt();
    }

    const QRegularExpression digits("(\\d+)");
    const QRegularExpressionMatch fallback = digits.match(base);
    return fallback.hasMatch() ? fallback.captured(1).toInt() : -1;
}
