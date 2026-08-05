#pragma once

#include "slicer_core/api/CommonDtos.h"

#include <array>
#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace slicer_core::api {

/** @brief Narrow import options that do not expose the engine SliceConfig. */
struct ModelImportRequest
{
    std::filesystem::path model_path;
    bool compute_bbox{true};
    bool extract_materials{true};
};

/** @brief Imported material metadata used by model and view facades. */
struct ModelMaterial
{
    std::string name;
    std::array<double, 3> diffuse_rgb{1.0, 1.0, 1.0};
    std::filesystem::path texture_path;
};

/** @brief Stable metadata returned for an imported model handle. */
struct ModelMetadata
{
    ModelId model_id{0};
    std::filesystem::path source_path;
    std::string format;
    std::size_t vertex_count{0};
    std::size_t triangle_count{0};
    bool has_uv{false};
    bool has_normals{false};
    std::vector<ModelMaterial> materials;
    Bounds3d local_bounds_mm;
    bool has_texture{false};
    std::string source_digest;
    std::string mesh_identity;
    std::string appearance_identity;
};

}  // namespace slicer_core::api
