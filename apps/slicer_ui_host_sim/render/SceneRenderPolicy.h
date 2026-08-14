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

/** @brief 由宿主持有的单场景修订三维帧。 */
struct ThreeDFrame final
{
    QString viewDataIdentity;
    QString meshLod;
    quint64 sceneRevision{0U};
    CameraBounds worldBounds;
    slicer::render::FrameDesc descriptor;
};

/**
 * @brief 获取冻结的 three_d ViewData，并且仅提交本地帧。
 */
class SceneRenderPolicy final
{
public:
    /**
     * @brief 基于公共模块与指定渲染后端创建策略。
     * @param client 运行时加载的模块客户端。
     * @param backend 后端中立的渲染器实现。
     * @param maxViewDataBytes 可接受的三维 ViewData 最大字节数。
     */
    SceneRenderPolicy(
        ModuleClient& client,
        slicer::render::IRenderBackend& backend,
        qint64 maxViewDataBytes = 128LL * 1024LL * 1024LL);

    /**
     * @brief 刷新权威 three_d ViewData 与资源缓存。
     * @param sceneHandle 模块持有的场景句柄。
     * @param sceneRevision 预期的权威场景修订号。
     * @param frame 接收本地帧资源与权威标志。
     * @param error 接收失败即拒绝的 DTO 或资源诊断。
     * @return 网格、UV、子网格、材料与纹理均闭合时返回 true。
     */
    bool Refresh(
        quint64 sceneHandle,
        quint64 sceneRevision,
        ThreeDFrame* frame,
        QString* error);

    /**
     * @brief 在不跨越公共模块边界的前提下渲染一帧。
     * @param frame 先前刷新得到的本地帧。
     * @param camera 宿主本地相机状态。
     * @param widthPx 输出像素宽度。
     * @param heightPx 输出像素高度。
     * @param output 接收 RGBA8 像素。
     * @return 后端帧结果与耗时数据。
     */
    [[nodiscard]] slicer::render::FrameResult Render(
        const ThreeDFrame& frame,
        const CameraController& camera,
        std::uint32_t widthPx,
        std::uint32_t heightPx,
        slicer::render::ImageOut* output);

    /** @brief 返回本地缓存执行的实际网格上传次数。 */
    [[nodiscard]] std::uint64_t MeshUploadCount() const;

    /** @brief 返回本地缓存执行的实际纹理上传次数。 */
    [[nodiscard]] std::uint64_t TextureUploadCount() const;

    /** @brief 返回通过公共 ABI 获取的 ViewData blob 分块数。 */
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
