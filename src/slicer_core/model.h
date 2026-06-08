#pragma once

#include "slicer_core/config.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace slicer_core {

struct Vec3 {
    double x{0.0};
    double y{0.0};
    double z{0.0};
};

struct TexCoord {
    double u{0.0};
    double v{0.0};
};

struct BoundingBox {
    Vec3 min;
    Vec3 max;
};

struct Triangle {
    Vec3 a;
    Vec3 b;
    Vec3 c;
};

struct MaterialStat {
    std::string name;
    std::size_t face_count{0};
    std::size_t triangle_count{0};
};

struct MaterialInfo {
    std::string name;
    std::array<std::uint8_t, 3> diffuse_rgb{0, 0, 0};
    bool has_diffuse{false};
    std::filesystem::path diffuse_texture_path;
    bool has_texture{false};
    bool texture_exists{false};
};

struct TriangleTextureInfo {
    bool has_uv{false};
    std::array<TexCoord, 3> uv{};
    std::string material_name;
};

struct AutoOrientReport {
    bool enabled{true};
    bool applied{false};
    double max_height_mm{6.0};
    std::string selected_orientation{"identity"};
    BoundingBox original_bbox_mm;
};

struct ModelReport {
    std::filesystem::path model_path;
    std::string format;
    std::string stl_encoding;
    std::size_t vertex_count{0};
    std::size_t face_count{0};
    std::size_t triangle_count{0};
    std::size_t degenerate_triangle_count{0};
    std::size_t texcoord_count{0};
    std::size_t faces_with_uv{0};
    std::size_t faces_without_uv{0};
    std::vector<std::string> material_libraries;
    std::vector<MaterialStat> materials;
    std::vector<MaterialInfo> material_infos;
    AutoOrientReport auto_orient;
    BoundingBox bbox_mm;
    std::vector<Triangle> triangles;
    std::vector<TriangleTextureInfo> triangle_textures;
};

ModelReport load_model_report(const SliceConfig& config, const std::filesystem::path& config_dir);

}  // namespace slicer_core
