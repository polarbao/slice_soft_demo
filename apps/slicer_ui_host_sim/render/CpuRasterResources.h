#pragma once

#include "IRenderBackend.h"

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace cpu_raster_detail
{

struct MeshResource final
{
    std::vector<float> positions;
    std::vector<float> normals;
    std::vector<float> texcoord0;
    std::vector<std::uint32_t> indices;
    std::vector<slicer::render::SubmeshDesc> submeshes;
};

struct TextureResource final
{
    std::uint32_t widthPx{0U};
    std::uint32_t heightPx{0U};
    std::vector<std::uint8_t> rgba8;
    bool rowOriginTopLeft{true};
};

struct MaterialResource final
{
    std::array<float, 4> baseColor{1.0F, 1.0F, 1.0F, 1.0F};
    std::string textureIdentity;
    std::string alphaMode{"opaque"};
    float alphaCutoff{0.5F};
    bool doubleSided{false};
    std::array<float, 9> uvTransform{
        1.0F, 0.0F, 0.0F,
        0.0F, 1.0F, 0.0F,
        0.0F, 0.0F, 1.0F};
};

struct Resources final
{
    std::unordered_map<std::string, MeshResource> meshes;
    std::unordered_map<std::string, TextureResource> textures;
    std::unordered_map<std::string, MaterialResource> materials;
};

[[nodiscard]] std::string MaterialKey(
    const std::string& appearanceIdentity,
    const std::string& materialId);

bool Rasterize(
    const Resources& resources,
    const slicer::render::FrameDesc& frame,
    slicer::render::ImageOut* output,
    std::uint32_t* drawCallCount,
    std::string* errorCode);

[[nodiscard]] slicer::render::PickResult Pick(
    const Resources& resources,
    const slicer::render::FrameDesc& frame,
    int xPx,
    int yPx);

}  // namespace cpu_raster_detail
