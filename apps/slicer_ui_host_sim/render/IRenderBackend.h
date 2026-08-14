#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace slicer::render
{

/** @brief 在不暴露实现 API 的前提下描述渲染器能力。 */
struct BackendCaps
{
    std::string backendId;
    bool deterministic{false};
    bool supportsMsaa{false};
    bool supportsIdPicking{false};
    std::uint32_t maxTextureSizePx{0U};
    std::uint64_t vramBudgetBytes{0U};
};

/** @brief 将连续索引范围绑定到 ViewData 材料标识。 */
struct SubmeshDesc
{
    std::uint32_t firstIndex{0U};
    std::uint32_t indexCount{0U};
    std::string materialId;
};

/** @brief 描述一次不可变 ViewData 网格上传。 */
struct MeshDesc
{
    std::string meshIdentity;
    std::uint32_t vertexCount{0U};
    std::uint32_t triangleCount{0U};
    const void* position{nullptr};
    const void* normal{nullptr};
    const void* texcoord0{nullptr};
    const void* index{nullptr};
    bool indexIs32Bit{true};
    std::vector<SubmeshDesc> submeshes;
};

/** @brief 描述一次不可变 RGBA8 ViewData 纹理上传。 */
struct TextureDesc
{
    std::string textureIdentity;
    std::uint32_t widthPx{0U};
    std::uint32_t heightPx{0U};
    const void* rgba8{nullptr};
    bool rowOriginTopLeft{true};
};

/** @brief 描述可复用外观中的一个材料。 */
struct MaterialDesc
{
    std::string appearanceIdentity;
    std::string materialId;
    float baseColorFactor[4]{1.0F, 1.0F, 1.0F, 1.0F};
    std::string baseColorTextureIdentity;
    std::string alphaMode{"opaque"};
    float alphaCutoff{0.5F};
    bool doubleSided{false};
    float uvTransform[9]{
        1.0F, 0.0F, 0.0F,
        0.0F, 1.0F, 0.0F,
        0.0F, 0.0F, 1.0F};
};

/** @brief 选择宿主本地展示模式。 */
enum class ViewMode
{
    Top,
    ThreeD
};

/** @brief 选择宿主本地正交或透视投影。 */
enum class Projection
{
    Orthographic,
    Perspective
};

/** @brief 承载宿主本地视图与投影矩阵。 */
struct CameraDesc
{
    Projection projection{Projection::Orthographic};
    float viewMatrix[16]{};
    float projMatrix[16]{};
};

/** @brief 描述使用缓存资源的一次权威实例绘制。 */
struct InstanceDraw
{
    std::string instanceId;
    std::string meshIdentity;
    std::string previewTextureIdentity;
    float localBoundsMm[4]{};
    std::string appearanceIdentity;
    float worldMatrix[16]{};
    bool selected{false};
    bool outOfBuildVolume{false};
};

/** @brief 仅描述构建体积与网格展示。 */
struct SceneDecorDesc
{
    bool showBuildVolume{true};
    float buildVolumeMm[3]{230.0F, 100.0F, 60.0F};
    bool showGrid{true};
    float gridMinorMm{1.0F};
    float gridMajorMm{10.0F};
    bool showAxes{true};
    bool showWhiteContrastAid{true};
};

/** @brief 提交给渲染器的完全本地帧。 */
struct FrameDesc
{
    ViewMode viewMode{ViewMode::ThreeD};
    std::uint32_t viewportWidthPx{0U};
    std::uint32_t viewportHeightPx{0U};
    CameraDesc camera;
    std::vector<InstanceDraw> instances;
    SceneDecorDesc decor;
};

/** @brief 在不使用后端专属类型的前提下报告一次渲染操作。 */
struct FrameResult
{
    bool ok{false};
    std::string errorCode;
    double cpuMs{0.0};
    double gpuMs{0.0};
    std::uint32_t drawCallCount{0U};
};

/** @brief 报告宿主本地拾取输出。 */
struct PickResult
{
    bool hit{false};
    std::string instanceId;
    float worldPosMm[3]{};
};

/** @brief 持有 RGBA8 离屏渲染结果。 */
struct ImageOut
{
    std::uint32_t widthPx{0U};
    std::uint32_t heightPx{0U};
    std::vector<std::uint8_t> rgba8;
};

/** @brief Stage 14E 冻结的后端中立宿主渲染接口。 */
class IRenderBackend
{
public:
    virtual ~IRenderBackend() = default;

    /** @brief 返回不可变后端能力元数据。 */
    [[nodiscard]] virtual BackendCaps Caps() const = 0;

    /** @brief 幂等上传一个 ViewData 网格。 */
    virtual bool UploadMesh(const MeshDesc& mesh) = 0;

    /** @brief 幂等上传一个 ViewData 纹理。 */
    virtual bool UploadTexture(const TextureDesc& texture) = 0;

    /** @brief 幂等上传一个 ViewData 材料。 */
    virtual bool UploadMaterial(const MaterialDesc& material) = 0;

    /** @brief 释放给定活动标识集合中不存在的资源。 */
    virtual void ReleaseUnused(
        const std::vector<std::string>& liveIdentities) = 0;

    /** @brief 渲染一个本地帧并返回耗时诊断。 */
    [[nodiscard]] virtual FrameResult RenderFrame(
        const FrameDesc& frame) = 0;

    /** @brief 渲染一张确定性的离屏 RGBA8 图像。 */
    virtual bool RenderToImage(
        const FrameDesc& frame,
        ImageOut& output) = 0;

    /** @brief 在不调用模块的前提下执行后端本地拾取。 */
    [[nodiscard]] virtual PickResult Pick(
        const FrameDesc& frame,
        int xPx,
        int yPx) = 0;
};

}  // namespace slicer::render
