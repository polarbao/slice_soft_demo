#include "slicer_core/geometry/repair/StrictRepairAssetRecheck.h"

#include "slicer_core/api/ProfileIdentity.h"
#include "slicer_core/config.h"
#include "slicer_core/geometry/repair/MeshRepairHash.h"
#include "slicer_core/geometry/repair/MeshRepairPreflight.h"
#include "slicer_core/model.h"

#include <fstream>
#include <stdexcept>

namespace slicer_core
{
namespace
{

void ThrowIfCancelled(const StrictRepairAssetRecheckRequest& request)
{
    if (request.cancellationRequested && request.cancellationRequested())
    {
        throw std::runtime_error("strict repair asset recheck was cancelled");
    }
}

Json ReadJson(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        throw std::runtime_error(
            "failed to open effective Profile: " + path.generic_string());
    }
    return Json::parse(input);
}

void ValidateRequest(const StrictRepairAssetRecheckRequest& request)
{
    if (request.expectedMesh == nullptr)
    {
        throw std::runtime_error("strict repair recheck requires expected mesh evidence");
    }
    if (!std::filesystem::is_regular_file(request.stagedObjPath)
        || request.stagedObjPath.extension() != ".obj")
    {
        throw std::runtime_error("strict repair recheck requires a staged OBJ asset");
    }
    if (!std::filesystem::is_regular_file(request.profileConfigPath)
        || request.profileHash.empty())
    {
        throw std::runtime_error("strict repair recheck requires effective Profile identity");
    }
}

SliceConfig MakeCanonicalAssetReimportConfig(
    const StrictRepairAssetRecheckRequest& request)
{
    const Json profileDocument = ReadJson(request.profileConfigPath);
    if (api::ComputeProfileDocumentHash(profileDocument) != request.profileHash)
    {
        throw std::runtime_error("effective Profile hash is stale");
    }
    SliceConfig config = load_slice_config(request.profileConfigPath);
    config.input.model_path = request.stagedObjPath;
    config.input.format = "obj";

    // 修复候选项已经按最终毫米坐标序列化。重新导入时沿用 Profile 的资源与
    // 材质策略，但绝不能再次应用源实例变换。
    config.transform.unit = "mm";
    config.transform.scale = {1.0, 1.0, 1.0};
    config.transform.rotation_deg = {0.0, 0.0, 0.0};
    config.transform.translation_mm = {0.0, 0.0, 0.0};
    config.auto_orient.enabled = false;
    validate_slice_config(config);
    return config;
}

}  // namespace

StrictRepairAssetRecheckResult RecheckStrictRepairAsset(
    const StrictRepairAssetRecheckRequest& request)
{
    ValidateRequest(request);
    ThrowIfCancelled(request);
    const SliceConfig config = MakeCanonicalAssetReimportConfig(request);
    const ModelReport scene = load_model_report(
        config, request.profileConfigPath.parent_path());
    ThrowIfCancelled(request);

    const AdaptedTriangleMesh imported = AdaptSceneModelToTriangleMesh(scene);
    StrictRepairAssetRecheckResult result;
    result.assetReimported = true;
    result.geometryHash = ComputeMeshRepairGeometryHash(imported.mesh);
    result.attributeHash = ComputeMeshRepairAttributeHash(imported);
    const std::string expectedGeometryHash =
        ComputeMeshRepairGeometryHash(request.expectedMesh->mesh);
    const std::string expectedAttributeHash =
        ComputeMeshRepairAttributeHash(*request.expectedMesh);
    result.attributesPreserved = result.geometryHash == expectedGeometryHash
        && result.attributeHash == expectedAttributeHash;
    if (!result.attributesPreserved)
    {
        throw std::runtime_error(
            "reimported repair asset does not preserve geometry/attribute identity");
    }

    MeshRepairPreflightRequest preflightRequest;
    preflightRequest.mesh = &imported;
    preflightRequest.input.sourcePath = request.stagedObjPath.generic_string();
    preflightRequest.input.inputFormat = "obj";
    preflightRequest.options.enabled = false;
    preflightRequest.options.mode = "strict_closed";
    preflightRequest.options.analyzeCompleteSelfIntersections = true;
    preflightRequest.sourceHash = result.geometryHash;
    const MeshRepairResult strict = EvaluateMeshRepairPreflight(preflightRequest);
    ThrowIfCancelled(request);

    result.diagnostics = strict.preRepair;
    result.strictComplete = strict.completeSelfIntersectionAnalysis.complete;
    result.strictPass = result.strictComplete
        && strict.status == MeshRepairStatus::StrictPassNoRepair
        && strict.preRepair.strictPass;
    if (!result.strictComplete)
    {
        throw std::runtime_error("strict repair recheck did not complete");
    }
    return result;
}

}  // namespace slicer_core
