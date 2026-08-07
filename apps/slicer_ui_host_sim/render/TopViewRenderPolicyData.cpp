#include "TopViewRenderPolicy.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>

#include <cmath>
#include <limits>

namespace
{
bool ReadBounds(
    const QJsonObject& value,
    TopViewBounds* bounds)
{
    const QJsonArray minimum = value.value(QStringLiteral("min")).toArray();
    const QJsonArray maximum = value.value(QStringLiteral("max")).toArray();
    if (bounds == nullptr || minimum.size() != 3 || maximum.size() != 3)
    {
        return false;
    }
    bounds->minX = minimum.at(0).toDouble();
    bounds->minY = minimum.at(1).toDouble();
    bounds->maxX = maximum.at(0).toDouble();
    bounds->maxY = maximum.at(1).toDouble();
    return std::isfinite(bounds->minX)
        && std::isfinite(bounds->minY)
        && std::isfinite(bounds->maxX)
        && std::isfinite(bounds->maxY)
        && bounds->maxX > bounds->minX
        && bounds->maxY > bounds->minY;
}

bool ReadMatrix(
    const QJsonArray& value,
    std::array<double, 16>* matrix)
{
    if (matrix == nullptr || value.size() != 16)
    {
        return false;
    }
    for (int index = 0; index < value.size(); ++index)
    {
        const double component = value.at(index).toDouble(
            std::numeric_limits<double>::quiet_NaN());
        if (!std::isfinite(component))
        {
            return false;
        }
        matrix->at(static_cast<std::size_t>(index)) = component;
    }
    return true;
}
}

bool TopViewRenderPolicy::ExecuteJson(
    const QJsonObject& request,
    QJsonObject* result,
    QString* error)
{
    if (result == nullptr)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("JSON 结果目标不能为空。");
        }
        return false;
    }
    QByteArray bytes;
    if (!m_client.Execute(
            QJsonDocument(request).toJson(QJsonDocument::Compact),
            &bytes,
            error))
    {
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("模块返回了无效 JSON：%1")
                         .arg(parseError.errorString());
        }
        return false;
    }
    *result = document.object();
    return true;
}

bool TopViewRenderPolicy::DecodeInstance(
    const QJsonObject& value,
    const QHash<QString, bool>& appearances,
    TopViewInstance* instance,
    QString* error)
{
    if (instance == nullptr)
    {
        return false;
    }
    instance->instanceId = value.value(QStringLiteral("instanceId")).toString();
    instance->appearanceIdentity = value.value(
        QStringLiteral("appearanceIdentity")).toString();
    instance->textureStatus = value.value(
        QStringLiteral("textureStatus")).toString();
    const QJsonObject preview = value.value(
        QStringLiteral("surfacePreview")).toObject();
    instance->previewIdentity = preview.value(
        QStringLiteral("previewIdentity")).toString();
    const bool valid = !instance->instanceId.isEmpty()
        && appearances.contains(instance->appearanceIdentity)
        && preview.value(QStringLiteral("appearanceIdentity")).toString()
            == instance->appearanceIdentity
        && !instance->previewIdentity.isEmpty()
        && preview.value(QStringLiteral("pixelFormat")).toString()
            == QStringLiteral("rgba8_unorm")
        && preview.value(QStringLiteral("colorSpace")).toString()
            == QStringLiteral("srgb")
        && preview.value(QStringLiteral("alphaMode")).toString()
            == QStringLiteral("straight")
        && preview.value(QStringLiteral("rowOrigin")).toString()
            == QStringLiteral("top_left")
        && ReadBounds(
            preview.value(QStringLiteral("localBoundsMm")).toObject(),
            &instance->localBoundsMm)
        && ReadMatrix(
            value.value(QStringLiteral("worldMatrix")).toArray(),
            &instance->worldMatrix);
    if (!valid || !LoadPreview(preview, &instance->surfacePreview, error))
    {
        if (error != nullptr && error->isEmpty())
        {
            *error = QStringLiteral("top ViewData 实例合同不完整：%1")
                         .arg(instance->instanceId);
        }
        return false;
    }
    return true;
}

bool TopViewRenderPolicy::LoadPreview(
    const QJsonObject& descriptor,
    QImage* image,
    QString* error)
{
    const QString identity = descriptor.value(
        QStringLiteral("previewIdentity")).toString();
    const auto cached = m_previewCache.constFind(identity);
    if (cached != m_previewCache.constEnd())
    {
        *image = cached.value();
        return true;
    }

    const int width = descriptor.value(QStringLiteral("widthPx")).toInt();
    const int height = descriptor.value(QStringLiteral("heightPx")).toInt();
    const QString blobId = descriptor.value(
        QStringLiteral("blobId")).toString();
    const int chunkCount = descriptor.value(
        QStringLiteral("chunkCount")).toInt();
    const qint64 totalBytes = static_cast<qint64>(descriptor.value(
        QStringLiteral("totalBytes")).toDouble());
    const qint64 expectedBytes = static_cast<qint64>(width)
        * static_cast<qint64>(height) * 4;
    if (width <= 0 || height <= 0 || blobId.isEmpty()
        || chunkCount <= 0 || totalBytes != expectedBytes)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("surfacePreview blob 描述无效。");
        }
        return false;
    }

    QByteArray bytes;
    if (!ReadBlob(blobId, chunkCount, totalBytes, &bytes, error))
    {
        return false;
    }
    const QImage decoded(
        reinterpret_cast<const uchar*>(bytes.constData()),
        width,
        height,
        width * 4,
        QImage::Format_RGBA8888);
    if (decoded.isNull())
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("surfacePreview RGBA8 解码失败。");
        }
        return false;
    }
    const QImage owned = decoded.copy();
    m_previewCache.insert(identity, owned);
    *image = owned;
    return true;
}

bool TopViewRenderPolicy::ReadBlob(
    const QString& blobId,
    int chunkCount,
    qint64 totalBytes,
    QByteArray* bytes,
    QString* error)
{
    bytes->clear();
    bytes->reserve(static_cast<int>(totalBytes));
    for (int chunkIndex = 0; chunkIndex < chunkCount; ++chunkIndex)
    {
        const QJsonObject request{
            {QStringLiteral("capability"),
             QStringLiteral("scene.get_viewdata")},
            {QStringLiteral("operation"), QStringLiteral("read_blob")},
            {QStringLiteral("blobId"), blobId},
            {QStringLiteral("chunkIndex"), chunkIndex}};
        QByteArray chunk;
        if (!m_client.Execute(
                QJsonDocument(request).toJson(QJsonDocument::Compact),
                &chunk,
                error))
        {
            return false;
        }
        ++m_blobReadCount;
        bytes->append(chunk);
    }
    if (bytes->size() != totalBytes)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("surfacePreview blob 长度不匹配。");
        }
        return false;
    }
    return true;
}
