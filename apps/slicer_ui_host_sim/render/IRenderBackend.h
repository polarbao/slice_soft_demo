#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace slicer::render
{

/** @brief Describes renderer capabilities without exposing an implementation API. */
struct BackendCaps
{
    std::string backendId;
    bool deterministic{false};
    bool supportsMsaa{false};
    bool supportsIdPicking{false};
    std::uint32_t maxTextureSizePx{0U};
    std::uint64_t vramBudgetBytes{0U};
};

/** @brief Binds one contiguous index range to a ViewData material identity. */
struct SubmeshDesc
{
    std::uint32_t firstIndex{0U};
    std::uint32_t indexCount{0U};
    std::string materialId;
};

/** @brief Describes one immutable ViewData mesh upload. */
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

/** @brief Describes one immutable RGBA8 ViewData texture upload. */
struct TextureDesc
{
    std::string textureIdentity;
    std::uint32_t widthPx{0U};
    std::uint32_t heightPx{0U};
    const void* rgba8{nullptr};
    bool rowOriginTopLeft{true};
};

/** @brief Describes one material within a reusable appearance. */
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

/** @brief Selects the host-local presentation mode. */
enum class ViewMode
{
    Top,
    ThreeD
};

/** @brief Selects orthographic or perspective host-local projection. */
enum class Projection
{
    Orthographic,
    Perspective
};

/** @brief Carries host-local view and projection matrices. */
struct CameraDesc
{
    Projection projection{Projection::Orthographic};
    float viewMatrix[16]{};
    float projMatrix[16]{};
};

/** @brief Describes one authoritative instance draw using cached resources. */
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

/** @brief Describes build-volume and grid presentation only. */
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

/** @brief Fully local frame submitted to a renderer. */
struct FrameDesc
{
    ViewMode viewMode{ViewMode::ThreeD};
    std::uint32_t viewportWidthPx{0U};
    std::uint32_t viewportHeightPx{0U};
    CameraDesc camera;
    std::vector<InstanceDraw> instances;
    SceneDecorDesc decor;
};

/** @brief Reports one rendering operation without backend-specific types. */
struct FrameResult
{
    bool ok{false};
    std::string errorCode;
    double cpuMs{0.0};
    double gpuMs{0.0};
    std::uint32_t drawCallCount{0U};
};

/** @brief Reports host-local picking output. */
struct PickResult
{
    bool hit{false};
    std::string instanceId;
    float worldPosMm[3]{};
};

/** @brief Owns an RGBA8 off-screen rendering result. */
struct ImageOut
{
    std::uint32_t widthPx{0U};
    std::uint32_t heightPx{0U};
    std::vector<std::uint8_t> rgba8;
};

/** @brief Backend-neutral host rendering interface frozen for Stage 14E. */
class IRenderBackend
{
public:
    virtual ~IRenderBackend() = default;

    /** @brief Returns immutable backend capability metadata. */
    [[nodiscard]] virtual BackendCaps Caps() const = 0;

    /** @brief Idempotently uploads one ViewData mesh. */
    virtual bool UploadMesh(const MeshDesc& mesh) = 0;

    /** @brief Idempotently uploads one ViewData texture. */
    virtual bool UploadTexture(const TextureDesc& texture) = 0;

    /** @brief Idempotently uploads one ViewData material. */
    virtual bool UploadMaterial(const MaterialDesc& material) = 0;

    /** @brief Releases resources absent from the supplied live identity set. */
    virtual void ReleaseUnused(
        const std::vector<std::string>& liveIdentities) = 0;

    /** @brief Renders one local frame and returns timing diagnostics. */
    [[nodiscard]] virtual FrameResult RenderFrame(
        const FrameDesc& frame) = 0;

    /** @brief Renders one deterministic off-screen RGBA8 image. */
    virtual bool RenderToImage(
        const FrameDesc& frame,
        ImageOut& output) = 0;

    /** @brief Performs backend-local picking without module calls. */
    [[nodiscard]] virtual PickResult Pick(
        const FrameDesc& frame,
        int xPx,
        int yPx) = 0;
};

}  // namespace slicer::render
