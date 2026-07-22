#pragma once

#include "slicer_core/json_value.h"
#include "slicer_core/preflight/ModelPreflightService.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace slicer_core
{

/**
 * @brief Supported origin of one required-family candidate.
 */
enum class RepairedAssetCandidateKind
{
    Unknown,
    StrictPassOriginal,
    ExternalRepaired,
    IndependentlyRebuilt,
};

/**
 * @brief Provenance required for externally changed geometry.
 */
struct RepairedAssetProvenance
{
    std::string provider;
    std::string tool;
    std::string tool_version;
    std::string operation_summary;
    std::string timestamp;
    std::string operator_id;
};

/**
 * @brief Explicit approval limits for geometry and attribute differences.
 */
struct RepairedAssetApproval
{
    double max_dimension_delta_mm{0.10};
    bool allow_attribute_changes{false};
    std::string attribute_change_reason;
};

/**
 * @brief One R4-06 required-family intake request.
 */
struct RepairedAssetIntakeRequest
{
    std::string family_id;
    std::string candidate_id;
    RepairedAssetCandidateKind candidate_kind{
        RepairedAssetCandidateKind::Unknown};
    std::filesystem::path original_config_path;
    std::filesystem::path candidate_config_path;
    std::string expected_original_source_hash;
    std::string expected_candidate_source_hash;
    RepairedAssetProvenance provenance;
    RepairedAssetApproval approval;
    ModelPreflightOptions preflight_options;
};

/**
 * @brief Auditable geometry, resource and preflight snapshot for one asset.
 */
struct RepairedAssetSnapshot
{
    std::string source_path;
    std::string input_format;
    std::string source_hash;
    std::string resource_hash;
    std::string transform_hash;
    std::string geometry_hash;
    std::string attribute_hash;
    std::string audit_hash;
    std::string preflight_status;
    std::string configured_unit;
    std::string selected_orientation;
    std::uint64_t vertex_count{0U};
    std::uint64_t triangle_count{0U};
    std::uint64_t component_count{0U};
    std::uint64_t boundary_edge_count{0U};
    std::uint64_t non_manifold_edge_count{0U};
    std::uint64_t duplicate_face_count{0U};
    std::uint64_t opposite_duplicate_face_count{0U};
    std::uint64_t local_winding_issue_count{0U};
    std::uint64_t degenerate_triangle_count{0U};
    std::uint64_t self_intersection_candidate_pair_count{0U};
    std::uint64_t self_intersection_tested_pair_count{0U};
    std::uint64_t confirmed_self_intersection_pair_count{0U};
    std::uint64_t coplanar_overlap_pair_count{0U};
    std::string self_intersection_candidate_pair_hash;
    std::uint64_t material_count{0U};
    std::uint64_t texture_count{0U};
    std::uint64_t missing_texture_count{0U};
    std::uint64_t uv_triangle_count{0U};
    double uv_coverage{0.0};
    std::array<double, 3> bbox_min_mm{};
    std::array<double, 3> bbox_max_mm{};
    std::array<double, 3> dimensions_mm{};
    double volume_mm3{0.0};
    bool full_audit_complete{false};
    bool strict_pass{false};
};

/**
 * @brief Differences between the original and candidate snapshots.
 */
struct RepairedAssetDelta
{
    std::int64_t vertex_count{0};
    std::int64_t triangle_count{0};
    std::int64_t component_count{0};
    std::int64_t material_count{0};
    std::int64_t texture_count{0};
    std::int64_t uv_triangle_count{0};
    std::array<double, 3> dimensions_mm{};
    double max_abs_dimension_delta_mm{0.0};
    bool geometry_changed{false};
    bool attributes_changed{false};
    bool resources_changed{false};
    bool transform_changed{false};
    bool orientation_changed{false};
};

/**
 * @brief Stable intake blocker or informational issue.
 */
struct RepairedAssetIntakeIssue
{
    std::string code;
    std::string message;
    Json context{Json::object({})};
};

/**
 * @brief Complete R4-06 intake result for one required-family or development candidate.
 */
struct RepairedAssetIntakeResult
{
    std::string family_id;
    std::string candidate_id;
    RepairedAssetCandidateKind candidate_kind{
        RepairedAssetCandidateKind::Unknown};
    bool manifest_accepted{false};
    bool admitted{false};
    bool repeatability_pass{false};
    bool production_output_written{false};
    std::uint64_t required_family_pass_count{0U};
    RepairedAssetProvenance provenance;
    RepairedAssetApproval approval;
    RepairedAssetSnapshot original;
    RepairedAssetSnapshot candidate;
    RepairedAssetDelta delta;
    std::vector<RepairedAssetIntakeIssue> issues;
};

/**
 * @brief Convert a candidate kind to stable report text.
 * @param kind Candidate kind.
 * @return Stable snake_case name.
 */
std::string RepairedAssetCandidateKindName(RepairedAssetCandidateKind kind);

/**
 * @brief Parse stable candidate kind text.
 * @param value Candidate kind text.
 * @return Parsed kind or Unknown.
 */
RepairedAssetCandidateKind ParseRepairedAssetCandidateKind(
    const std::string& value);

}  // namespace slicer_core
