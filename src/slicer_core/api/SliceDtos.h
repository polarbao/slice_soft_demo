#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace slicer_core::api {

/** @brief Fast or full model preflight request. */
struct PreflightRequest
{
    std::filesystem::path scene_config_path;
    bool authoritative{false};
};

/** @brief Preflight admission result with stable error codes. */
struct PreflightResult
{
    bool admitted{false};
    bool authoritative{false};
    std::vector<std::string> issue_codes;
};

/** @brief Production slice request. All output paths are caller-owned. */
struct SliceRequest
{
    std::string job_id;
    std::string correlation_id;
    std::string scene_hash;
    std::filesystem::path scene_config_path;
    std::filesystem::path package_dir;
};

/** @brief Production slice output summary. */
struct SliceResult
{
    std::filesystem::path package_dir;
    std::filesystem::path manifest_path;
    int layer_count{0};
    std::array<int, 2> grid_px{};
    std::string engine_version;
    std::uint64_t elapsed_ms{0};
};

/** @brief Geometry repair request owned by the Worker lane. */
struct RepairRequest
{
    std::filesystem::path source_model_path;
    std::filesystem::path repaired_model_path;
};

/** @brief Geometry repair evidence. */
struct RepairResult
{
    std::filesystem::path repaired_model_path;
    std::string source_hash;
    std::string repaired_hash;
};

}  // namespace slicer_core::api
