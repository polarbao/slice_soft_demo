#include "slicer_core/api/ProfileIdentity.h"
#include "slicer_core/config.h"
#include "slicer_core/geometry/SceneModelTriangleMeshAdapter.h"
#include "slicer_core/geometry/repair/DeterministicObjAssetWriter.h"
#include "slicer_core/geometry/repair/StrictRepairAssetRecheck.h"
#include "slicer_core/model.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{

std::filesystem::path SourceRoot()
{
    return std::filesystem::path(SLICESOFT_SOURCE_DIR);
}

void WriteText(const std::filesystem::path& path, const std::string& text)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << text;
}

slicer_core::Json ReadJson(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return slicer_core::Json::parse(input);
}

bool Expect(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
    }
    return condition;
}

slicer_core::AdaptedTriangleMesh LoadFixture(
    const std::filesystem::path& sourceObj,
    const std::filesystem::path& profilePath)
{
    slicer_core::SliceConfig config = slicer_core::load_slice_config(profilePath);
    config.input.model_path = sourceObj;
    config.input.format = "obj";
    config.transform.unit = "mm";
    config.transform.scale = {1.0, 1.0, 1.0};
    config.transform.rotation_deg = {0.0, 0.0, 0.0};
    config.transform.translation_mm = {0.0, 0.0, 0.0};
    config.auto_orient.enabled = false;
    slicer_core::validate_slice_config(config);
    const slicer_core::ModelReport report = slicer_core::load_model_report(
        config, profilePath.parent_path());
    return slicer_core::AdaptSceneModelToTriangleMesh(report);
}

bool ReimportsAndStrictlyAcceptsClosedObj()
{
    const std::filesystem::path root =
        std::filesystem::temp_directory_path()
        / "slicesoft_stage14d08_r3_strict_recheck";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root);
    const std::filesystem::path sourceObj = root / "source.obj";
    WriteText(
        sourceObj,
        "v 0 0 0\n"
        "v 1 0 0\n"
        "v 0 1 0\n"
        "v 0 0 1\n"
        "f 1 3 2\n"
        "f 1 2 4\n"
        "f 2 3 4\n"
        "f 3 1 4\n");
    const std::filesystem::path profilePath = SourceRoot()
        / "samples" / "configs" / "material_process"
        / "obj_mtl_texture_rgb_white_ondemand.json";
    const slicer_core::AdaptedTriangleMesh source =
        LoadFixture(sourceObj, profilePath);

    slicer_core::DeterministicObjAssetWriteRequest writeRequest;
    writeRequest.mesh = &source;
    writeRequest.outputObjPath = root / "staging" / "repaired.obj";
    const slicer_core::DeterministicObjAssetWriteResult written =
        slicer_core::WriteDeterministicObjAsset(writeRequest);

    slicer_core::StrictRepairAssetRecheckRequest recheckRequest;
    recheckRequest.stagedObjPath = written.objPath;
    recheckRequest.profileConfigPath = profilePath;
    recheckRequest.profileHash = slicer_core::api::ComputeProfileDocumentHash(
        ReadJson(profilePath));
    recheckRequest.expectedMesh = &source;
    const slicer_core::StrictRepairAssetRecheckResult result =
        slicer_core::RecheckStrictRepairAsset(recheckRequest);

    const bool pass = Expect(result.assetReimported, "asset must be reimported")
        && Expect(result.attributesPreserved, "geometry and attributes must match")
        && Expect(result.strictComplete, "strict audit must be complete")
        && Expect(result.strictPass, "closed tetrahedron must pass strict audit");
    std::filesystem::remove_all(root, ignored);
    return pass;
}

bool RejectsStaleProfileIdentity()
{
    const std::filesystem::path root =
        std::filesystem::temp_directory_path()
        / "slicesoft_stage14d08_r3_strict_recheck_stale";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root);
    const std::filesystem::path sourceObj = root / "source.obj";
    WriteText(
        sourceObj,
        "v 0 0 0\nv 1 0 0\nv 0 1 0\nv 0 0 1\n"
        "f 1 3 2\nf 1 2 4\nf 2 3 4\nf 3 1 4\n");
    const std::filesystem::path profilePath = SourceRoot()
        / "samples" / "configs" / "material_process"
        / "obj_mtl_texture_rgb_white_ondemand.json";
    const slicer_core::AdaptedTriangleMesh source =
        LoadFixture(sourceObj, profilePath);
    slicer_core::DeterministicObjAssetWriteRequest writeRequest;
    writeRequest.mesh = &source;
    writeRequest.outputObjPath = root / "staging" / "repaired.obj";
    const auto written = slicer_core::WriteDeterministicObjAsset(writeRequest);

    slicer_core::StrictRepairAssetRecheckRequest request;
    request.stagedObjPath = written.objPath;
    request.profileConfigPath = profilePath;
    request.profileHash = "stale";
    request.expectedMesh = &source;
    bool rejected{false};
    try
    {
        static_cast<void>(slicer_core::RecheckStrictRepairAsset(request));
    }
    catch (const std::runtime_error&)
    {
        rejected = true;
    }
    std::filesystem::remove_all(root, ignored);
    return Expect(rejected, "stale Profile hash must fail closed");
}

}  // namespace

int main()
{
    const bool pass = ReimportsAndStrictlyAcceptsClosedObj()
        && RejectsStaleProfileIdentity();
    if (!pass)
    {
        return 1;
    }
    std::cout << "stage14d08_r3_strict_recheck_tests: PASS\n";
    return 0;
}
