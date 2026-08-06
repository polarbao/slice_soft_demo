#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "slicer_core/json_value.h"

namespace slicer_core::api {

/** @brief Fast or full model preflight request. */
struct PreflightRequest
{
    std::filesystem::path scene_config_path;
    std::filesystem::path profile_config_path;
    std::string scene_hash;
    std::string profile_hash;
    std::uint64_t expected_scene_revision{0U};
    std::string target_mode;
    bool authoritative{true};
};

/** @brief One stable structured issue returned by authoritative preflight. */
struct PreflightIssue
{
    std::string code;
    std::string severity;
    std::uint64_t count{0U};
    std::string detail;
    std::string model_id;
    std::string instance_id;
    Json context{Json::object({})};
};

/** @brief Stable topology counters for one model or aggregate scene. */
struct PreflightTopology
{
    bool available{false};
    bool strict_pass{false};
    std::uint64_t boundary_edges{0U};
    std::uint64_t non_manifold_edges{0U};
    std::uint64_t duplicate_faces{0U};
    std::uint64_t opposite_duplicate_faces{0U};
    std::uint64_t local_winding_issues{0U};
    std::uint64_t degenerate_triangles{0U};
    std::uint64_t connected_components{0U};
    std::uint64_t self_intersection_pairs{0U};
};

/** @brief Stable axis-aligned bounds in millimetres. */
struct PreflightBounds
{
    bool valid{false};
    std::array<double, 3> min_mm{};
    std::array<double, 3> max_mm{};
};

/** @brief Full preflight evidence for one committed scene instance. */
struct PreflightInstanceResult
{
    std::string model_id;
    std::string instance_id;
    std::uint64_t transform_revision{0U};
    std::string transform_hash;
    bool visible{true};
    bool skipped_hidden{false};
    bool complete{false};
    bool blocked{true};
    bool out_of_bounds{false};
    std::string source_status;
    std::string transformed_status;
    std::string legacy_admission;
    std::string global_admission;
    PreflightTopology topology;
    PreflightBounds bbox_mm;
    std::vector<PreflightIssue> issues;
};

/** @brief One deterministic positive-area collision pair. */
struct PreflightCollision
{
    std::string first_instance_id;
    std::string second_instance_id;
};

/** @brief Preflight admission result with stable error codes. */
struct PreflightResult
{
    bool admitted{false};
    bool authoritative{false};
    bool complete{false};
    bool cancelled{false};
    bool out_of_bounds{false};
    std::string scene_id;
    std::uint64_t scene_revision{0U};
    std::string scene_hash;
    std::string target_mode;
    std::size_t checked_model_count{0U};
    std::size_t checked_instance_count{0U};
    std::size_t blocked_instance_count{0U};
    std::size_t skipped_instance_count{0U};
    PreflightTopology topology;
    PreflightBounds bbox_mm;
    std::vector<std::string> issue_codes;
    std::vector<PreflightIssue> issues;
    std::vector<PreflightInstanceResult> instances;
    std::vector<PreflightCollision> collisions;
    std::vector<std::string> out_of_bounds_instances;
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
    std::string job_id;
    std::string correlation_id;
    std::string model_id;
    std::filesystem::path source_model_path;
    std::filesystem::path repaired_model_path;
    std::filesystem::path profile_config_path;
    std::filesystem::path source_resource_root;
    std::filesystem::path job_root_path;
    std::string profile_hash;
    std::string model_format{"obj"};
    std::string repair_output_format{"obj"};
    std::string policy{"conservative"};
    bool require_strict_pass{true};
};

/** @brief Geometry repair evidence. */
struct RepairResult
{
    std::filesystem::path repaired_model_path;
    std::string source_hash;
    std::string repaired_hash;
    Json preflight_before{Json::object({})};
    Json preflight_after{Json::object({})};
    Json evidence{Json::object({})};
    std::uint64_t elapsed_ms{0U};
};

}  // namespace slicer_core::api
