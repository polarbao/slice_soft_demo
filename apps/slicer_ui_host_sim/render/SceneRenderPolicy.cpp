#include "SceneRenderPolicy.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>

#include <algorithm>
#include <utility>

namespace
{
QString FormatViewDataFailure(const QJsonObject& result)
{
    const QString code = result.value(QStringLiteral("code")).toString();
    const QString message = result.value(
        QStringLiteral("message")).toString();
    const QString detail = result.value(QStringLiteral("detail")).toString();
    if (code == QStringLiteral("PM-SLICER-VIEWDATA-BUDGET"))
    {
        return QStringLiteral(
            "PM-SLICER-VIEWDATA-BUDGET：3D 视图数据超过显示预算，"
            "已停止显示且不会使用破碎网格。%1")
            .arg(detail.isEmpty() ? QString{} : QStringLiteral(" 预算：%1 字节。")
                .arg(detail));
    }
    if (!code.isEmpty())
    {
        return QStringLiteral("%1：%2%3")
            .arg(
                code,
                message.isEmpty() ? QStringLiteral("3D ViewData 请求失败")
                                  : message,
                detail.isEmpty() ? QString{} : QStringLiteral("（%1）").arg(detail));
    }
    return QString::fromUtf8(
        QJsonDocument(result).toJson(QJsonDocument::Compact));
}
}

SceneRenderPolicy::SceneRenderPolicy(
    ModuleClient& client,
    slicer::render::IRenderBackend& backend,
    const qint64 maxViewDataBytes)
    : m_client(client),
      m_backend(backend),
      m_cache(backend),
      m_maxViewDataBytes((std::max)(maxViewDataBytes, 1LL))
{
}

bool SceneRenderPolicy::Refresh(
    const quint64 sceneHandle,
    const quint64 sceneRevision,
    ThreeDFrame* frame,
    QString* error)
{
    if (frame == nullptr || sceneHandle == 0U || sceneRevision == 0U)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("3D 刷新缺少有效场景身份。");
        }
        return false;
    }
    *frame = ThreeDFrame{};
    QHash<QString, bool> outOfBounds;
    slicer::render::SceneDecorDesc decor;
    if (!LoadSnapshot(
            sceneHandle, sceneRevision, &outOfBounds, &decor, error))
    {
        return false;
    }
    const QJsonObject request{
        {QStringLiteral("capability"), QStringLiteral("scene.get_viewdata")},
        {QStringLiteral("operation"), QStringLiteral("query")},
        {QStringLiteral("sceneHandle"), static_cast<qint64>(sceneHandle)},
        {QStringLiteral("expectedSceneRevision"),
         static_cast<qint64>(sceneRevision)},
        {QStringLiteral("viewMode"), QStringLiteral("three_d")},
        {QStringLiteral("texturePolicy"),
         QStringLiteral("require_if_present")},
        {QStringLiteral("lod"), QStringLiteral("auto")},
        {QStringLiteral("meshTransform"), QStringLiteral("local")},
        {QStringLiteral("maxBytes"), m_maxViewDataBytes},
        {QStringLiteral("content"), QJsonArray{
             QStringLiteral("bbox"),
             QStringLiteral("outline"),
             QStringLiteral("mesh"),
             QStringLiteral("appearance")}}};
    QJsonObject result;
    if (!ExecuteJson(request, &result, error))
    {
        return false;
    }
    if (!result.value(QStringLiteral("ok")).toBool())
    {
        if (error != nullptr)
        {
            *error = FormatViewDataFailure(result);
        }
        return false;
    }
    const quint64 returnedRevision = static_cast<quint64>(
        result.value(QStringLiteral("sceneRevision")).toDouble());
    const QString identity = result.value(
        QStringLiteral("viewdataIdentity")).toString();
    if (returnedRevision != sceneRevision || identity.isEmpty()
        || result.value(QStringLiteral("viewMode")).toString()
            != QStringLiteral("three_d"))
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("模块返回了不匹配的 three_d ViewData。");
        }
        return false;
    }

    ThreeDFrame decoded;
    decoded.viewDataIdentity = identity;
    decoded.sceneRevision = returnedRevision;
    decoded.descriptor.viewMode = slicer::render::ViewMode::ThreeD;
    decoded.descriptor.decor = decor;
    std::vector<std::string> liveIdentities;
    AppearanceTextureMap textureIdentities;
    MeshValueMap meshes;
    if (!UploadAppearances(
            result, &textureIdentities, &liveIdentities, error)
        || !UploadMeshes(result, &meshes, &liveIdentities, error)
        || !DecodeInstances(
            result, outOfBounds, textureIdentities, meshes, &decoded,
            &liveIdentities, error))
    {
        return false;
    }
    m_cache.ReleaseUnused(liveIdentities);
    *frame = std::move(decoded);
    return true;
}

slicer::render::FrameResult SceneRenderPolicy::Render(
    const ThreeDFrame& frame,
    const CameraController& camera,
    const std::uint32_t widthPx,
    const std::uint32_t heightPx,
    slicer::render::ImageOut* output)
{
    slicer::render::FrameDesc descriptor = frame.descriptor;
    descriptor.viewportWidthPx = widthPx;
    descriptor.viewportHeightPx = heightPx;
    descriptor.camera = camera.BuildCamera();
    if (output != nullptr)
    {
        const bool success = m_backend.RenderToImage(descriptor, *output);
        return {success, success ? std::string{} : "HOST-RENDER-FAILED",
                0.0, 0.0, 0U};
    }
    return m_backend.RenderFrame(descriptor);
}

std::uint64_t SceneRenderPolicy::MeshUploadCount() const
{
    return m_cache.MeshUploadCount();
}

std::uint64_t SceneRenderPolicy::TextureUploadCount() const
{
    return m_cache.TextureUploadCount();
}

quint64 SceneRenderPolicy::BlobReadCount() const
{
    return m_blobReadCount;
}

bool SceneRenderPolicy::ExecuteJson(
    const QJsonObject& request,
    QJsonObject* result,
    QString* error)
{
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

bool SceneRenderPolicy::ReadBlob(
    const QJsonObject& descriptor,
    QByteArray* bytes,
    QString* error)
{
    const QString blobId = descriptor.value(QStringLiteral("blobId")).toString();
    const int chunkCount = descriptor.value(QStringLiteral("chunkCount")).toInt();
    const qint64 totalBytes = static_cast<qint64>(
        descriptor.value(QStringLiteral("totalBytes")).toDouble());
    if (bytes == nullptr || blobId.isEmpty() || chunkCount <= 0
        || totalBytes <= 0 || totalBytes > 256LL * 1024LL * 1024LL)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("ViewData blob 描述无效。");
        }
        return false;
    }
    bytes->clear();
    bytes->reserve(static_cast<int>(totalBytes));
    for (int chunkIndex = 0; chunkIndex < chunkCount; ++chunkIndex)
    {
        QByteArray chunk;
        if (!m_client.Execute(
                QJsonDocument(QJsonObject{
                    {QStringLiteral("capability"),
                     QStringLiteral("scene.get_viewdata")},
                    {QStringLiteral("operation"), QStringLiteral("read_blob")},
                    {QStringLiteral("blobId"), blobId},
                    {QStringLiteral("chunkIndex"), chunkIndex}})
                    .toJson(QJsonDocument::Compact),
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
            *error = QStringLiteral("ViewData blob 长度不匹配。");
        }
        return false;
    }
    return true;
}

bool SceneRenderPolicy::LoadSnapshot(
    const quint64 sceneHandle,
    const quint64 sceneRevision,
    QHash<QString, bool>* outOfBounds,
    slicer::render::SceneDecorDesc* decor,
    QString* error)
{
    QJsonObject result;
    if (!ExecuteJson(QJsonObject{
            {QStringLiteral("capability"),
             QStringLiteral("scene.get_snapshot")},
            {QStringLiteral("sceneHandle"), static_cast<qint64>(sceneHandle)}},
            &result,
            error)
        || !result.value(QStringLiteral("ok")).toBool()
        || static_cast<quint64>(result.value(
               QStringLiteral("sceneRevision")).toDouble()) != sceneRevision)
    {
        return false;
    }
    const QJsonObject volume = result.value(
        QStringLiteral("buildVolume")).toObject();
    decor->buildVolumeMm[0] = static_cast<float>(
        volume.value(QStringLiteral("widthMm")).toDouble(230.0));
    decor->buildVolumeMm[1] = static_cast<float>(
        volume.value(QStringLiteral("heightMm")).toDouble(100.0));
    decor->buildVolumeMm[2] = static_cast<float>(
        volume.value(QStringLiteral("zLimitMm")).toDouble(60.0));
    for (const QJsonValue& item : result.value(
             QStringLiteral("instances")).toArray())
    {
        const QJsonObject instance = item.toObject();
        outOfBounds->insert(
            instance.value(QStringLiteral("instanceId")).toString(),
            instance.value(QStringLiteral("outOfBounds")).toBool());
    }
    return !outOfBounds->isEmpty();
}
