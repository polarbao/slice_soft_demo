#include "SceneRenderPolicy.h"

#include "MeshAttributeDecoder.h"

#include <QJsonArray>

#include <cstdint>
#include <utility>

namespace
{
bool ReadBuffer(
    const QJsonObject& buffers,
    const QString& name,
    const QString& format,
    const qint64 blobSize,
    qint64* offset,
    qint64* length)
{
    const QJsonObject value = buffers.value(name).toObject();
    *offset = static_cast<qint64>(
        value.value(QStringLiteral("byteOffset")).toDouble(-1.0));
    *length = static_cast<qint64>(
        value.value(QStringLiteral("byteLength")).toDouble(-1.0));
    return value.value(QStringLiteral("format")).toString() == format
        && *offset >= 0 && *length > 0 && *offset <= blobSize
        && *length <= blobSize - *offset;
}
}

bool SceneRenderPolicy::UploadMesh(
    const QJsonObject& value,
    QString* error)
{
    const std::string identity = value.value(
        QStringLiteral("meshIdentity")).toString().toStdString();
    if (identity.empty()
        || value.value(QStringLiteral("meshTransform")).toString()
            != QStringLiteral("local"))
    {
        return false;
    }
    if (m_cache.HasMesh(identity))
    {
        return true;
    }
    QByteArray bytes;
    if (!ReadBlob(value, &bytes, error))
    {
        return false;
    }
    const QJsonObject buffers = value.value(
        QStringLiteral("buffers")).toObject();
    qint64 indexOffset{0};
    qint64 indexLength{0};
    const int vertexCount = value.value(
        QStringLiteral("vertexCount")).toInt();
    const int triangleCount = value.value(
        QStringLiteral("triangleCount")).toInt();
    std::vector<float> positions;
    std::vector<float> normals;
    std::vector<float> texcoord0;
    if (vertexCount <= 0 || triangleCount <= 0
        || !slicer::render::DecodeMeshAttribute(
            bytes, buffers, QStringLiteral("position"), 3,
            vertexCount, &positions, error)
        || !slicer::render::DecodeMeshAttribute(
            bytes, buffers, QStringLiteral("normal"), 3,
            vertexCount, &normals, error)
        || !slicer::render::DecodeMeshAttribute(
            bytes, buffers, QStringLiteral("texcoord0"), 2,
            vertexCount, &texcoord0, error)
        || !ReadBuffer(buffers, QStringLiteral("index"),
            QStringLiteral("uint32"), bytes.size(),
            &indexOffset, &indexLength)
        || indexLength != triangleCount * 3
            * static_cast<int>(sizeof(std::uint32_t)))
    {
        if (error != nullptr && error->isEmpty())
        {
            *error = QStringLiteral("three_d 网格 buffer 合同无效。");
        }
        return false;
    }
    slicer::render::MeshDesc mesh;
    mesh.meshIdentity = identity;
    mesh.vertexCount = static_cast<std::uint32_t>(vertexCount);
    mesh.triangleCount = static_cast<std::uint32_t>(triangleCount);
    mesh.position = positions.data();
    mesh.normal = normals.data();
    mesh.texcoord0 = texcoord0.data();
    mesh.index = bytes.constData() + indexOffset;
    mesh.indexIs32Bit = true;
    for (const QJsonValue& item : value.value(
             QStringLiteral("submeshes")).toArray())
    {
        const QJsonObject submesh = item.toObject();
        slicer::render::SubmeshDesc descriptor;
        descriptor.firstIndex = static_cast<std::uint32_t>(submesh.value(
            QStringLiteral("firstIndex")).toInt(-1));
        descriptor.indexCount = static_cast<std::uint32_t>(submesh.value(
            QStringLiteral("indexCount")).toInt(-1));
        descriptor.materialId = submesh.value(
            QStringLiteral("materialId")).toString().toStdString();
        if (descriptor.materialId.empty()
            || descriptor.indexCount == 0U
            || descriptor.indexCount % 3U != 0U
            || static_cast<std::uint64_t>(descriptor.firstIndex)
                + descriptor.indexCount
                > static_cast<std::uint64_t>(triangleCount) * 3U)
        {
            if (error != nullptr)
            {
                *error = QStringLiteral("three_d submesh 合同无效。");
            }
            return false;
        }
        mesh.submeshes.push_back(std::move(descriptor));
    }
    if (mesh.submeshes.empty() || !m_cache.UploadMesh(mesh))
    {
        if (error != nullptr && error->isEmpty())
        {
            *error = QStringLiteral("渲染后端拒绝 three_d 网格。");
        }
        return false;
    }
    return true;
}
