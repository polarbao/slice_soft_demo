#pragma once

#include "../ModuleClient.h"
#include "../camera/CameraController.h"
#include "AppearanceCache.h"
#include "IRenderBackend.h"

#include <QHash>
#include <QJsonObject>
#include <QString>

#include <cstdint>
#include <string>
#include <vector>

/** @brief Host-owned three-dimensional frame for one scene revision. */
struct ThreeDFrame final
{
    QString viewDataIdentity;
    QString meshLod;
    quint64 sceneRevision{0U};
    CameraBounds worldBounds;
    slicer::render::FrameDesc descriptor;
};

/**
 * @brief Fetches frozen three_d ViewData and submits only local frames.
 */
class SceneRenderPolicy final
{
public:
    /**
     * @brief Creates a policy over the public module and one renderer backend.
     * @param client Runtime-loaded module client.
     * @param backend Backend-neutral renderer implementation.
     * @param maxViewDataBytes Maximum accepted three-dimensional ViewData bytes.
     */
    SceneRenderPolicy(
        ModuleClient& client,
        slicer::render::IRenderBackend& backend,
        qint64 maxViewDataBytes = 128LL * 1024LL * 1024LL);

    /**
     * @brief Refreshes authoritative three_d ViewData and resource caches.
     * @param sceneHandle Module-owned scene handle.
     * @param sceneRevision Expected authoritative scene revision.
     * @param frame Receives local frame resources and authoritative flags.
     * @param error Receives a fail-closed DTO or resource diagnostic.
     * @return True when mesh, UV, submesh, material and texture close.
     */
    bool Refresh(
        quint64 sceneHandle,
        quint64 sceneRevision,
        ThreeDFrame* frame,
        QString* error);

    /**
     * @brief Renders one frame without crossing the public module boundary.
     * @param frame Previously refreshed local frame.
     * @param camera Host-local camera state.
     * @param widthPx Output width in pixels.
     * @param heightPx Output height in pixels.
     * @param output Receives RGBA8 pixels.
     * @return Backend frame result and timing data.
     */
    [[nodiscard]] slicer::render::FrameResult Render(
        const ThreeDFrame& frame,
        const CameraController& camera,
        std::uint32_t widthPx,
        std::uint32_t heightPx,
        slicer::render::ImageOut* output);

    /** @brief Returns real mesh uploads performed by the local cache. */
    [[nodiscard]] std::uint64_t MeshUploadCount() const;

    /** @brief Returns real texture uploads performed by the local cache. */
    [[nodiscard]] std::uint64_t TextureUploadCount() const;

    /** @brief Returns ViewData blob chunks fetched through the public ABI. */
    [[nodiscard]] quint64 BlobReadCount() const;

private:
    using TextureIdentityMap = QHash<QString, QString>;
    using AppearanceTextureMap = QHash<QString, TextureIdentityMap>;
    using MeshValueMap = QHash<QString, QJsonObject>;

    bool ExecuteJson(
        const QJsonObject& request,
        QJsonObject* result,
        QString* error);
    bool ReadBlob(
        const QJsonObject& descriptor,
        QByteArray* bytes,
        QString* error);
    bool LoadSnapshot(
        quint64 sceneHandle,
        quint64 sceneRevision,
        QHash<QString, bool>* outOfBounds,
        slicer::render::SceneDecorDesc* decor,
        QString* error);
    bool UploadAppearances(
        const QJsonObject& result,
        AppearanceTextureMap* identities,
        std::vector<std::string>* liveIdentities,
        QString* error);
    bool UploadMeshes(
        const QJsonObject& result,
        MeshValueMap* meshes,
        std::vector<std::string>* liveIdentities,
        QString* error);
    bool DecodeInstances(
        const QJsonObject& result,
        const QHash<QString, bool>& outOfBounds,
        const AppearanceTextureMap& identities,
        const MeshValueMap& meshes,
        ThreeDFrame* frame,
        std::vector<std::string>* liveIdentities,
        QString* error);
    bool UploadTexture(
        const QJsonObject& value,
        slicer::render::TextureDesc* texture,
        QString* error);
    bool UploadMesh(
        const QJsonObject& value,
        QString* error);

    ModuleClient& m_client;
    slicer::render::IRenderBackend& m_backend;
    AppearanceCache m_cache;
    qint64 m_maxViewDataBytes{128LL * 1024LL * 1024LL};
    quint64 m_blobReadCount{0U};
};
