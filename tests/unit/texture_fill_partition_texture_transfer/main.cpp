#include "slicer_core/config.h"
#include "slicer_core/geometry/NearestTriangleQuery.h"
#include "slicer_core/geometry/SceneModelTriangleMeshAdapter.h"
#include "slicer_core/materials/texture_application/TextureFillPartitionTextureTransfer.h"
#include "slicer_core/model.h"

#include <array>
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL " << message << '\n';
        return false;
    }
    return true;
}

bool HasIssueCode(
    const std::vector<slicer_core::ValidationIssue>& issues,
    const std::string& code)
{
    for (const slicer_core::ValidationIssue& issue : issues)
    {
        if (issue.code == code)
        {
            return true;
        }
    }
    return false;
}

slicer_core::AdaptedTriangleMesh LoadAdapted(const std::string& relativeConfig)
{
    const std::filesystem::path configPath =
        std::filesystem::path{SLICESOFT_SOURCE_DIR} / relativeConfig;
    slicer_core::SliceConfig config =
        slicer_core::load_slice_config(configPath);
    if (!config.input.model_path.is_absolute())
    {
        const std::filesystem::path fromConfig =
            configPath.parent_path() / config.input.model_path;
        config.input.model_path = std::filesystem::exists(fromConfig)
            ? fromConfig.lexically_normal()
            : (std::filesystem::path{SLICESOFT_SOURCE_DIR}
                / config.input.model_path).lexically_normal();
    }
    const slicer_core::SceneModel scene = slicer_core::load_model_report(
        config,
        configPath.parent_path());
    return slicer_core::AdaptSceneModelToTriangleMesh(scene);
}

std::size_t FindTriangleWithTexture(
    const slicer_core::AdaptedTriangleMesh& adapted)
{
    for (std::size_t index{0U}; index < adapted.triangle_attributes.size(); ++index)
    {
        const slicer_core::SurfaceTriangleAttributes& attributes =
            adapted.triangle_attributes.at(index);
        if (!attributes.has_uv)
        {
            continue;
        }
        for (const slicer_core::MaterialInfo& material : adapted.material_infos)
        {
            if (material.name == attributes.material_name
                && material.has_texture
                && material.texture_exists)
            {
                return index;
            }
        }
    }
    throw std::runtime_error("fixture has no textured triangle");
}

std::size_t FindTriangleWithDiffuseOnly(
    const slicer_core::AdaptedTriangleMesh& adapted)
{
    for (std::size_t index{0U}; index < adapted.triangle_attributes.size(); ++index)
    {
        const std::string& materialName =
            adapted.triangle_attributes.at(index).material_name;
        for (const slicer_core::MaterialInfo& material : adapted.material_infos)
        {
            if (material.name == materialName
                && material.has_diffuse
                && !material.has_texture)
            {
                return index;
            }
        }
    }
    throw std::runtime_error("fixture has no diffuse-only triangle");
}

slicer_core::GlobalTextureFillPartitionResult MakePartition(
    const std::size_t triangleIndex,
    const bool allTexture = false)
{
    slicer_core::GlobalTextureFillPartitionResult partition;
    partition.available = true;
    partition.partitionPass = true;
    partition.status = "diagnostic";
    partition.grid.width = 3;
    partition.grid.height = 1;
    partition.grid.depth = 1;
    partition.grid.spacingXMm = 0.05;
    partition.grid.spacingYMm = 0.05;
    partition.grid.spacingZMm = 0.01;
    partition.modelMask.grid = partition.grid;
    partition.textureSurfaceMask.grid = partition.grid;
    partition.modelFillMask.grid = partition.grid;
    partition.modelMask.values = {1U, 1U, 0U};
    partition.textureSurfaceMask.values = allTexture
        ? std::vector<std::uint8_t>{1U, 1U, 0U}
        : std::vector<std::uint8_t>{1U, 0U, 0U};
    partition.modelFillMask.values = allTexture
        ? std::vector<std::uint8_t>{0U, 0U, 0U}
        : std::vector<std::uint8_t>{0U, 1U, 0U};
    partition.closestSurfaceReferences.assign(
        3U,
        slicer_core::TextureFillClosestSurfaceReference{});
    for (std::size_t index{0U}; index < 2U; ++index)
    {
        slicer_core::TextureFillClosestSurfaceReference& reference =
            partition.closestSurfaceReferences.at(index);
        reference.valid = true;
        reference.triangleIndex = triangleIndex;
        reference.barycentric = {0.25, 0.25, 0.50};
        reference.distanceMm = 0.05;
    }
    return partition;
}

slicer_core::AdaptedTriangleMesh MakeManualAdapted(
    const bool hasUv,
    const bool hasTexture,
    const bool textureExists)
{
    slicer_core::AdaptedTriangleMesh adapted;
    adapted.mesh.vertices = {
        {0.0, 0.0, 0.0},
        {1.0, 0.0, 0.0},
        {0.0, 1.0, 0.0},
    };
    adapted.mesh.triangles = {{0, 1, 2}};
    slicer_core::SurfaceTriangleAttributes attributes;
    attributes.has_uv = hasUv;
    attributes.uv = {{{0.0, 0.0}, {1.0, 0.0}, {0.0, 1.0}}};
    attributes.material_name = "fixture";
    adapted.triangle_attributes.push_back(attributes);
    slicer_core::MaterialInfo material;
    material.name = "fixture";
    material.has_diffuse = true;
    material.diffuse_rgb = {40, 80, 120};
    material.has_texture = hasTexture;
    material.texture_exists = textureExists;
    material.diffuse_texture_path = textureExists
        ? std::filesystem::path{SLICESOFT_SOURCE_DIR}
            / "samples/models/textured/textures/checker.png"
        : std::filesystem::path{"missing-texture.png"};
    adapted.material_infos.push_back(material);
    return adapted;
}

slicer_core::TextureFillPartitionTextureTransferRequest MakeRequest(
    const slicer_core::AdaptedTriangleMesh& adapted,
    const slicer_core::GlobalTextureFillPartitionResult& partition)
{
    slicer_core::TextureFillPartitionTextureTransferRequest request;
    request.adaptedMesh = &adapted;
    request.partition = &partition;
    request.fallbackRgb = {12, 34, 56};
    request.missingTexturePolicy = "warn_and_fallback";
    return request;
}

bool ObjUsesStoredClosestReference()
{
    const slicer_core::AdaptedTriangleMesh adapted = LoadAdapted(
        "samples/configs/openvdb/surface_shell_obj_real.json");
    const std::size_t triangleIndex = FindTriangleWithTexture(adapted);
    const slicer_core::GlobalTextureFillPartitionResult partition =
        MakePartition(triangleIndex);
    const auto result = slicer_core::TransferTextureFillPartition(
        MakeRequest(adapted, partition));
    return ExpectTrue(result.available, "OBJ transfer is available")
        && ExpectTrue(result.status == "diagnostic", "OBJ transfer is diagnostic")
        && ExpectTrue(result.stats.sampledTextureCount == 1U, "OBJ texture sampled")
        && ExpectTrue(result.stats.reusedReferenceCount == 1U, "OBJ reference reused")
        && ExpectTrue(result.stats.nearestQueryCount == 0U, "OBJ transfer runs no nearest query")
        && ExpectTrue(
            result.voxelRgb.at(1) == std::array<std::uint8_t, 3>{255, 255, 255},
            "model-fill voxel stays uncolored")
        && ExpectTrue(
            result.voxelRgb.at(2) == std::array<std::uint8_t, 3>{255, 255, 255},
            "outside voxel stays uncolored")
        && ExpectTrue(result.stats.outsideColoredCount == 0U, "OBJ colors stay inside texture mask");
}

bool ThreeMfUsesSameTransferService()
{
    const slicer_core::AdaptedTriangleMesh adapted = LoadAdapted(
        "samples/configs/openvdb/surface_shell_3mf_real.json");
    const std::size_t triangleIndex = FindTriangleWithTexture(adapted);
    const slicer_core::GlobalTextureFillPartitionResult partition =
        MakePartition(triangleIndex);
    const auto result = slicer_core::TransferTextureFillPartition(
        MakeRequest(adapted, partition));
    return ExpectTrue(result.available, "3MF transfer is available")
        && ExpectTrue(result.stats.sampledTextureCount == 1U, "3MF texture sampled")
        && ExpectTrue(result.stats.missingTextureCount == 0U, "3MF texture resource resolved")
        && ExpectTrue(result.stats.outsideColoredCount == 0U, "3MF colors stay inside texture mask");
}

bool ThreeMfColorGroupUsesDiffuseWithoutMissingTexture()
{
    const slicer_core::AdaptedTriangleMesh adapted = LoadAdapted(
        "samples/configs/3mf/three_mf_color_group_rgb.json");
    const std::size_t triangleIndex = FindTriangleWithDiffuseOnly(adapted);
    const auto partition = MakePartition(triangleIndex);
    const auto result = slicer_core::TransferTextureFillPartition(
        MakeRequest(adapted, partition));
    return ExpectTrue(result.available, "3MF ColorGroup transfer is available")
        && ExpectTrue(result.stats.materialDiffuseCount == 1U, "3MF ColorGroup uses diffuse")
        && ExpectTrue(result.stats.missingTextureCount == 0U, "3MF ColorGroup is not a missing texture");
}

bool MissingUvWarnsAndUsesDiffuse()
{
    const slicer_core::AdaptedTriangleMesh adapted =
        MakeManualAdapted(false, false, false);
    const auto partition = MakePartition(0U);
    const auto result = slicer_core::TransferTextureFillPartition(
        MakeRequest(adapted, partition));
    return ExpectTrue(result.available, "missing UV warn policy remains available")
        && ExpectTrue(result.stats.missingUvCount == 1U, "missing UV counted")
        && ExpectTrue(result.stats.materialDiffuseCount == 1U, "diffuse fallback used")
        && ExpectTrue(
            HasIssueCode(result.issues, "E_12E_TEXTURE_MISSING_UV"),
            "missing UV warning is stable");
}

bool MissingUvFailFastBlocks()
{
    const slicer_core::AdaptedTriangleMesh adapted =
        MakeManualAdapted(false, false, false);
    const auto partition = MakePartition(0U);
    auto request = MakeRequest(adapted, partition);
    request.missingTexturePolicy = "fail_fast";
    const auto result = slicer_core::TransferTextureFillPartition(request);
    return ExpectTrue(!result.available, "missing UV fail-fast is unavailable")
        && ExpectTrue(result.status == "blocked", "missing UV fail-fast blocks")
        && ExpectTrue(result.voxelRgb.empty(), "blocked transfer exposes no partial RGB")
        && ExpectTrue(
            HasIssueCode(result.issues, "E_12E_TEXTURE_MISSING_UV"),
            "missing UV fail-fast issue is stable");
}

bool MissingResourceWarnsAndUsesDiffuse()
{
    const slicer_core::AdaptedTriangleMesh adapted =
        MakeManualAdapted(true, true, false);
    const auto partition = MakePartition(0U);
    const auto result = slicer_core::TransferTextureFillPartition(
        MakeRequest(adapted, partition));
    return ExpectTrue(result.available, "missing resource warn policy remains available")
        && ExpectTrue(result.stats.missingTextureCount == 1U, "missing resource counted")
        && ExpectTrue(result.stats.materialDiffuseCount == 1U, "missing resource uses diffuse")
        && ExpectTrue(
            HasIssueCode(result.issues, "E_12E_TEXTURE_MISSING_RESOURCE"),
            "missing resource warning is stable");
}

bool InvalidReferenceBlocksWithoutAccess()
{
    const slicer_core::AdaptedTriangleMesh adapted =
        MakeManualAdapted(true, false, false);
    auto partition = MakePartition(99U);
    const auto result = slicer_core::TransferTextureFillPartition(
        MakeRequest(adapted, partition));
    return ExpectTrue(!result.available, "invalid reference is unavailable")
        && ExpectTrue(
            HasIssueCode(result.issues, "E_12E_TEXTURE_TRIANGLE_OUT_OF_RANGE"),
            "invalid reference uses stable issue");
}

bool MissingReferenceUsesStableIssue()
{
    const slicer_core::AdaptedTriangleMesh adapted =
        MakeManualAdapted(true, false, false);
    auto partition = MakePartition(0U);
    partition.closestSurfaceReferences.at(0).valid = false;
    const auto result = slicer_core::TransferTextureFillPartition(
        MakeRequest(adapted, partition));
    return ExpectTrue(!result.available, "missing reference is unavailable")
        && ExpectTrue(
            HasIssueCode(result.issues, "E_12E_TEXTURE_REFERENCE_MISSING"),
            "missing reference uses stable issue");
}

bool RepeatUvSamplingIsReported()
{
    slicer_core::AdaptedTriangleMesh adapted =
        MakeManualAdapted(true, true, true);
    adapted.triangle_attributes.at(0).uv = {
        slicer_core::TexCoord{1.25, 1.25},
        slicer_core::TexCoord{2.25, 1.25},
        slicer_core::TexCoord{1.25, 2.25},
    };
    const auto partition = MakePartition(0U);
    auto request = MakeRequest(adapted, partition);
    request.textureSample.uv_address_mode = "repeat";
    const auto result = slicer_core::TransferTextureFillPartition(request);
    return ExpectTrue(result.available, "repeat UV transfer is available")
        && ExpectTrue(result.stats.sampledTextureCount == 1U, "repeat UV samples texture")
        && ExpectTrue(result.stats.uvOutOfRangeCount == 1U, "repeat UV out-of-range is reported");
}

bool BackendNamesDoNotChangeStoredReferenceTransfer()
{
    const slicer_core::AdaptedTriangleMesh adapted =
        MakeManualAdapted(true, false, false);
    auto cpuPartition = MakePartition(0U, true);
    auto openVdbPartition = cpuPartition;
    cpuPartition.backend = "legacy_cpu_global_distance";
    openVdbPartition.backend = "openvdb_conformance";
    const auto cpu = slicer_core::TransferTextureFillPartition(
        MakeRequest(adapted, cpuPartition));
    const auto openVdb = slicer_core::TransferTextureFillPartition(
        MakeRequest(adapted, openVdbPartition));
    return ExpectTrue(cpu.available && openVdb.available, "both backend references transfer")
        && ExpectTrue(cpu.voxelRgb == openVdb.voxelRgb, "backend-neutral RGB matches")
        && ExpectTrue(cpu.colorSources == openVdb.colorSources, "backend-neutral sources match")
        && ExpectTrue(
            cpu.stats.reusedReferenceCount == openVdb.stats.reusedReferenceCount,
            "backend-neutral reference counts match");
}

bool AllTextureAndTieEvidenceAreDeterministic()
{
    const slicer_core::AdaptedTriangleMesh adapted =
        MakeManualAdapted(true, false, false);
    auto partition = MakePartition(0U, true);
    partition.closestSurfaceReferences.at(0).tieCandidateCount = 2U;
    partition.closestSurfaceReferences.at(1).tieCandidateCount = 1U;
    const auto first = slicer_core::TransferTextureFillPartition(
        MakeRequest(adapted, partition));
    const auto second = slicer_core::TransferTextureFillPartition(
        MakeRequest(adapted, partition));
    return ExpectTrue(first.available && second.available, "all-texture transfers are available")
        && ExpectTrue(first.stats.materialDiffuseCount == 2U, "all model voxels receive diffuse")
        && ExpectTrue(first.stats.medialAxisTieCount == 2U, "tie voxels counted")
        && ExpectTrue(first.voxelRgb == second.voxelRgb, "tie transfer RGB is deterministic")
        && ExpectTrue(first.colorSources == second.colorSources, "tie sources are deterministic");
}

bool NearestQueryRecordsDeterministicTieEvidence()
{
    slicer_core::TriangleMeshData mesh;
    mesh.vertices = {
        {0.0, 0.0, 0.0},
        {1.0, 0.0, 0.0},
        {0.0, 1.0, 0.0},
    };
    mesh.triangles = {{0, 1, 2}, {0, 1, 2}};
    const slicer_core::NearestTriangleQuery query(mesh);
    const slicer_core::NearestTriangleHit first = query.FindNearest(
        {0.25, 0.25, 0.5});
    const slicer_core::NearestTriangleHit second = query.FindNearest(
        {0.25, 0.25, 0.5});
    return ExpectTrue(first.found && second.found, "tie fixture finds a triangle")
        && ExpectTrue(first.triangle_index == 0U, "stable tie chooses lower triangle index")
        && ExpectTrue(first.tie_candidate_count == 1U, "nearest query records one tie candidate")
        && ExpectTrue(
            first.triangle_index == second.triangle_index
                && first.tie_candidate_count == second.tie_candidate_count,
            "nearest tie evidence is deterministic");
}

}  // namespace

int main()
{
    const std::vector<std::pair<std::string, bool (*)()>> tests{
        {"obj_uses_stored_closest_reference", ObjUsesStoredClosestReference},
        {"three_mf_uses_same_transfer_service", ThreeMfUsesSameTransferService},
        {"three_mf_color_group_uses_diffuse_without_missing_texture", ThreeMfColorGroupUsesDiffuseWithoutMissingTexture},
        {"missing_uv_warns_and_uses_diffuse", MissingUvWarnsAndUsesDiffuse},
        {"missing_uv_fail_fast_blocks", MissingUvFailFastBlocks},
        {"missing_resource_warns_and_uses_diffuse", MissingResourceWarnsAndUsesDiffuse},
        {"invalid_reference_blocks_without_access", InvalidReferenceBlocksWithoutAccess},
        {"missing_reference_uses_stable_issue", MissingReferenceUsesStableIssue},
        {"repeat_uv_sampling_is_reported", RepeatUvSamplingIsReported},
        {"backend_names_do_not_change_stored_reference_transfer", BackendNamesDoNotChangeStoredReferenceTransfer},
        {"all_texture_and_tie_evidence_are_deterministic", AllTextureAndTieEvidenceAreDeterministic},
        {"nearest_query_records_deterministic_tie_evidence", NearestQueryRecordsDeterministicTieEvidence},
    };
    for (const auto& test : tests)
    {
        std::cout << "RUN " << test.first << std::endl;
        try
        {
            if (!test.second())
            {
                return 1;
            }
        }
        catch (const std::exception& error)
        {
            std::cerr << "FAIL " << test.first << " exception=" << error.what() << '\n';
            return 1;
        }
        std::cout << "PASS " << test.first << '\n';
    }
    std::cout << "Texture/fill partition texture transfer unit tests complete.\n";
    return 0;
}
