#include "SceneRenderPolicy.h"

#include <QJsonArray>
#include <QJsonDocument>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

namespace
{
bool ReadFloatArray(
    const QJsonArray& values,
    float* output,
    int expectedCount)
{
    if (output == nullptr || values.size() != expectedCount)
    {
        return false;
    }
    for (int index = 0; index < expectedCount; ++index)
    {
        const double value = values.at(index).toDouble(
            std::numeric_limits<double>::quiet_NaN());
        if (!std::isfinite(value))
        {
            return false;
        }
        output[index] = static_cast<float>(value);
    }
    return true;
}

bool ReadMatrix(const QJsonArray& values, float output[16])
{
    return ReadFloatArray(values, output, 16);
}

bool ReadBounds(
    const QJsonObject& value,
    float output[4],
    float zBounds[2])
{
    const QJsonArray minimum = value.value(QStringLiteral("min")).toArray();
    const QJsonArray maximum = value.value(QStringLiteral("max")).toArray();
    if (minimum.size() != 3 || maximum.size() != 3)
    {
        return false;
    }
    const std::array<double, 6> components{
        minimum.at(0).toDouble(
            std::numeric_limits<double>::quiet_NaN()),
        minimum.at(1).toDouble(
            std::numeric_limits<double>::quiet_NaN()),
        minimum.at(2).toDouble(
            std::numeric_limits<double>::quiet_NaN()),
        maximum.at(0).toDouble(
            std::numeric_limits<double>::quiet_NaN()),
        maximum.at(1).toDouble(
            std::numeric_limits<double>::quiet_NaN()),
        maximum.at(2).toDouble(
            std::numeric_limits<double>::quiet_NaN())};
    if (!std::all_of(components.begin(), components.end(), [](double item)
        {
            return std::isfinite(item);
        }))
    {
        return false;
    }
    output[0] = static_cast<float>(components[0]);
    output[1] = static_cast<float>(components[1]);
    output[2] = static_cast<float>(components[3]);
    output[3] = static_cast<float>(components[4]);
    zBounds[0] = static_cast<float>(components[2]);
    zBounds[1] = static_cast<float>(components[5]);
    return output[2] > output[0] && output[3] > output[1]
        && zBounds[1] >= zBounds[0];
}

void ExpandBounds(
    const slicer::render::InstanceDraw& instance,
    const float zBounds[2],
    CameraBounds* bounds,
    bool* first)
{
    for (int zIndex = 0; zIndex < 2; ++zIndex)
    {
        for (int yIndex = 0; yIndex < 2; ++yIndex)
        {
            for (int xIndex = 0; xIndex < 2; ++xIndex)
            {
                const float x = instance.localBoundsMm[xIndex == 0 ? 0 : 2];
                const float y = instance.localBoundsMm[yIndex == 0 ? 1 : 3];
                const float z = zBounds[zIndex];
                const float world[3]{
                    instance.worldMatrix[0] * x + instance.worldMatrix[1] * y
                        + instance.worldMatrix[2] * z + instance.worldMatrix[3],
                    instance.worldMatrix[4] * x + instance.worldMatrix[5] * y
                        + instance.worldMatrix[6] * z + instance.worldMatrix[7],
                    instance.worldMatrix[8] * x + instance.worldMatrix[9] * y
                        + instance.worldMatrix[10] * z + instance.worldMatrix[11]};
                if (*first)
                {
                    bounds->minX = bounds->maxX = world[0];
                    bounds->minY = bounds->maxY = world[1];
                    bounds->minZ = bounds->maxZ = world[2];
                    *first = false;
                }
                bounds->minX = (std::min)(bounds->minX, world[0]);
                bounds->minY = (std::min)(bounds->minY, world[1]);
                bounds->minZ = (std::min)(bounds->minZ, world[2]);
                bounds->maxX = (std::max)(bounds->maxX, world[0]);
                bounds->maxY = (std::max)(bounds->maxY, world[1]);
                bounds->maxZ = (std::max)(bounds->maxZ, world[2]);
            }
        }
    }
}

}

bool SceneRenderPolicy::UploadAppearances(
    const QJsonObject& result,
    AppearanceTextureMap* identities,
    std::vector<std::string>* liveIdentities,
    QString* error)
{
    const QJsonArray appearances = result.value(
        QStringLiteral("appearances")).toArray();
    if (appearances.isEmpty())
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("three_d ViewData 缺少 appearances。");
        }
        return false;
    }
    for (const QJsonValue& item : appearances)
    {
        const QJsonObject appearance = item.toObject();
        const QString appearanceIdentity = appearance.value(
            QStringLiteral("appearanceIdentity")).toString();
        if (appearanceIdentity.isEmpty()
            || identities->contains(appearanceIdentity))
        {
            if (error != nullptr)
            {
                *error = QStringLiteral("ViewData 外观身份缺失或重复。");
            }
            return false;
        }
        TextureIdentityMap textureIdentities;
        for (const QJsonValue& textureValue : appearance.value(
                 QStringLiteral("textures")).toArray())
        {
            slicer::render::TextureDesc texture;
            if (!UploadTexture(textureValue.toObject(), &texture, error))
            {
                return false;
            }
            const QString textureId = textureValue.toObject().value(
                QStringLiteral("textureId")).toString();
            if (textureId.isEmpty() || textureIdentities.contains(textureId))
            {
                if (error != nullptr)
                {
                    *error = QStringLiteral("ViewData textureId 缺失或重复。");
                }
                return false;
            }
            textureIdentities.insert(
                textureId,
                QString::fromStdString(texture.textureIdentity));
            liveIdentities->push_back(texture.textureIdentity);
        }
        const QJsonArray materials = appearance.value(
            QStringLiteral("materials")).toArray();
        if (materials.isEmpty())
        {
            if (error != nullptr)
            {
                *error = QStringLiteral("ViewData appearance 缺少材质。");
            }
            return false;
        }
        for (const QJsonValue& materialValue : materials)
        {
            const QJsonObject value = materialValue.toObject();
            slicer::render::MaterialDesc material;
            material.appearanceIdentity = appearanceIdentity.toStdString();
            material.materialId = value.value(
                QStringLiteral("materialId")).toString().toStdString();
            material.alphaMode = value.value(
                QStringLiteral("alphaMode")).toString().toStdString();
            material.alphaCutoff = static_cast<float>(value.value(
                QStringLiteral("alphaCutoff")).toDouble(0.5));
            material.doubleSided = value.value(
                QStringLiteral("doubleSided")).toBool();
            if (!ReadFloatArray(value.value(
                    QStringLiteral("baseColorFactor")).toArray(),
                    material.baseColorFactor, 4)
                || !ReadFloatArray(value.value(
                    QStringLiteral("uvTransform")).toArray(),
                    material.uvTransform, 9))
            {
                if (error != nullptr)
                {
                    *error = QStringLiteral("ViewData 材质数值字段无效。");
                }
                return false;
            }
            const QString textureId = value.value(
                QStringLiteral("baseColorTextureId")).toString();
            if (!textureId.isEmpty())
            {
                const auto identity = textureIdentities.constFind(textureId);
                if (identity == textureIdentities.constEnd())
                {
                    if (error != nullptr)
                    {
                        *error = QStringLiteral("材质引用了未闭合的纹理：%1")
                                     .arg(textureId);
                    }
                    return false;
                }
                material.baseColorTextureIdentity = identity.value().toStdString();
            }
            if (!m_cache.UploadMaterial(material))
            {
                if (error != nullptr)
                {
                    *error = QStringLiteral("渲染后端拒绝材质：%1")
                                 .arg(QString::fromStdString(material.materialId));
                }
                return false;
            }
        }
        identities->insert(appearanceIdentity, textureIdentities);
        liveIdentities->push_back(appearanceIdentity.toStdString());
    }
    return true;
}

bool SceneRenderPolicy::UploadTexture(
    const QJsonObject& value,
    slicer::render::TextureDesc* texture,
    QString* error)
{
    texture->textureIdentity = value.value(
        QStringLiteral("textureIdentity")).toString().toStdString();
    texture->widthPx = static_cast<std::uint32_t>(value.value(
        QStringLiteral("widthPx")).toInt());
    texture->heightPx = static_cast<std::uint32_t>(value.value(
        QStringLiteral("heightPx")).toInt());
    texture->rowOriginTopLeft = value.value(
        QStringLiteral("rowOrigin")).toString() == QStringLiteral("top_left");
    if (texture->textureIdentity.empty() || texture->widthPx == 0U
        || texture->heightPx == 0U
        || value.value(QStringLiteral("pixelFormat")).toString()
            != QStringLiteral("rgba8_unorm")
        || value.value(QStringLiteral("colorSpace")).toString()
            != QStringLiteral("srgb")
        || value.value(QStringLiteral("alphaMode")).toString()
            != QStringLiteral("straight"))
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("three_d 纹理合同不完整。");
        }
        return false;
    }
    if (m_cache.HasTexture(texture->textureIdentity))
    {
        return true;
    }
    QByteArray bytes;
    if (!ReadBlob(value, &bytes, error)
        || bytes.size() != static_cast<qint64>(texture->widthPx)
            * texture->heightPx * 4LL)
    {
        return false;
    }
    texture->rgba8 = bytes.constData();
    return m_cache.UploadTexture(*texture);
}

bool SceneRenderPolicy::DecodeInstances(
    const QJsonObject& result,
    const QHash<QString, bool>& outOfBounds,
    const AppearanceTextureMap& identities,
    ThreeDFrame* frame,
    std::vector<std::string>* liveIdentities,
    QString* error)
{
    const QJsonArray instances = result.value(
        QStringLiteral("instances")).toArray();
    if (instances.isEmpty())
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("three_d ViewData 未返回实例。");
        }
        return false;
    }
    bool firstBounds{true};
    for (const QJsonValue& item : instances)
    {
        const QJsonObject value = item.toObject();
        slicer::render::InstanceDraw instance;
        instance.instanceId = value.value(
            QStringLiteral("instanceId")).toString().toStdString();
        instance.appearanceIdentity = value.value(
            QStringLiteral("appearanceIdentity")).toString().toStdString();
        instance.meshIdentity = value.value(QStringLiteral("mesh"))
            .toObject().value(QStringLiteral("meshIdentity"))
            .toString().toStdString();
        const QString instanceId = QString::fromStdString(instance.instanceId);
        const QString appearance = QString::fromStdString(
            instance.appearanceIdentity);
        float zBounds[2]{};
        const QString textureStatus = value.value(
            QStringLiteral("textureStatus")).toString();
        const bool textureContractClosed =
            textureStatus == QStringLiteral("available")
            || (textureStatus == QStringLiteral("not_provided")
                && identities.value(appearance).isEmpty());
        if (instance.instanceId.empty() || instance.meshIdentity.empty()
            || !identities.contains(appearance))
        {
            if (error != nullptr)
            {
                *error = QStringLiteral("three_d 实例身份未闭合：%1")
                             .arg(instanceId);
            }
            return false;
        }
        if (!ReadMatrix(value.value(
                QStringLiteral("worldMatrix")).toArray(),
                instance.worldMatrix)
            || !ReadBounds(value.value(
                QStringLiteral("bboxLocalMm")).toObject(),
                instance.localBoundsMm, zBounds))
        {
            if (error != nullptr)
            {
                *error = QStringLiteral("three_d 实例空间字段无效：%1")
                             .arg(instanceId);
            }
            return false;
        }
        if (!textureContractClosed)
        {
            if (error != nullptr)
            {
                *error = QStringLiteral("three_d 实例纹理状态未闭合：%1/%2")
                             .arg(instanceId, textureStatus);
            }
            return false;
        }
        if (!UploadMesh(value.value(
                QStringLiteral("mesh")).toObject(), error))
        {
            if (error != nullptr && error->isEmpty())
            {
                *error = QStringLiteral("three_d 实例网格未闭合：%1")
                             .arg(instanceId);
            }
            return false;
        }
        instance.outOfBuildVolume = outOfBounds.value(instanceId, false);
        frame->descriptor.instances.push_back(instance);
        ExpandBounds(instance, zBounds, &frame->worldBounds, &firstBounds);
        liveIdentities->push_back(instance.meshIdentity);
    }
    return !firstBounds;
}
