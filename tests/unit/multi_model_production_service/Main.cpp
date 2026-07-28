#include "slicer_core/config.h"
#include "slicer_core/model.h"
#include "slicer_core/pipeline/MultiModelProductionService.h"
#include "slicer_core/rip_reader.h"
#include "slicer_core/scene/SceneEffectiveConfig.h"
#include "slicer_core/scene/SceneModel.h"
#include "slicer_core/scene/SceneResourceIdentity.h"
#include "slicer_core/system/Sha256.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
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

bool StableThreeMfResourceIdentityIgnoresExtractionRoot()
{
    const std::filesystem::path root =
        std::filesystem::temp_directory_path()
        / "slicesoft_scene_resource_identity_test";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    const std::filesystem::path firstTexture =
        root / "first/cache/3mf_textures/5_checker.png";
    const std::filesystem::path secondTexture =
        root / "second/cache/3mf_textures/5_checker.png";
    std::filesystem::create_directories(
        firstTexture.parent_path());
    std::filesystem::create_directories(
        secondTexture.parent_path());
    {
        std::ofstream first(firstTexture, std::ios::binary);
        std::ofstream second(secondTexture, std::ios::binary);
        first << "same-texture-content";
        second << "same-texture-content";
    }

    slicer_core::SceneModel first;
    first.model_path = root / "fixture.3mf";
    first.format = "3mf";
    first.three_mf.texture_resource_count = 1;
    first.three_mf.texture_loaded_count = 1;
    slicer_core::MaterialInfo firstMaterial;
    firstMaterial.name = "3mf_texture2dgroup_7";
    firstMaterial.has_diffuse = true;
    firstMaterial.has_texture = true;
    firstMaterial.texture_exists = true;
    firstMaterial.texture_source = "3mf_internal";
    firstMaterial.diffuse_texture_path = firstTexture;
    first.material_infos.push_back(firstMaterial);

    slicer_core::SceneModel second = first;
    second.material_infos.front().diffuse_texture_path =
        secondTexture;
    const std::string firstHash =
        slicer_core::ComputeSceneResourceHash(first);
    const std::string secondHash =
        slicer_core::ComputeSceneResourceHash(second);

    {
        std::ofstream changed(
            secondTexture,
            std::ios::binary | std::ios::trunc);
        changed << "changed-texture-content";
    }
    const std::string changedHash =
        slicer_core::ComputeSceneResourceHash(second);
    std::filesystem::remove_all(root, cleanupError);
    return ExpectTrue(
               firstHash == secondHash,
               "3MF resource identity ignores extraction root")
        && ExpectTrue(
            firstHash != changedHash,
            "3MF resource identity preserves texture content");
}

std::string ReadFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        throw std::runtime_error(
            "failed to read fixture: " + path.generic_string());
    }
    return {
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()};
}

slicer_core::MultiModelScene MakeThreeInstanceScene(
    const std::filesystem::path& profileConfigPath)
{
    slicer_core::SliceConfig profile =
        slicer_core::load_slice_config(profileConfigPath);
    if (profile.input.model_path.is_relative())
    {
        profile.input.model_path =
            std::filesystem::absolute(
                profileConfigPath.parent_path()
                / profile.input.model_path)
                .lexically_normal();
    }
    const slicer_core::SceneModel model =
        slicer_core::load_model_report(
            profile,
            profileConfigPath.parent_path());
    const std::filesystem::path modelPath =
        std::filesystem::absolute(model.model_path)
            .lexically_normal();

    slicer_core::MultiModelScene scene;
    scene.sceneid = "scene-production-service-three";
    scene.scenerevision = 1U;
    scene.resolvedprofileid =
        profile.material_process_profile.name;
    scene.buildvolume.source =
        slicer_core::BuildVolumeSource::Fixture;
    scene.buildvolume.widthmm = 80.0;
    scene.buildvolume.heightmm = 40.0;
    scene.buildvolume.origin =
        slicer_core::BuildVolumeOrigin::LowerLeft;
    scene.buildvolume.xdirection =
        slicer_core::BuildVolumeAxisDirection::Positive;
    scene.buildvolume.ydirection =
        slicer_core::BuildVolumeAxisDirection::Positive;
    scene.buildvolume.isfixture = true;

    slicer_core::ResourceScope scope;
    scope.resourcescopeid = "scope-policy-textured-small";
    scope.kind = slicer_core::ResourceScopeKind::ObjDirectory;
    scope.rootpath = modelPath.parent_path();
    scene.resourcescopes.push_back(scope);

    slicer_core::ModelSource source;
    source.modelid = "model-policy-textured-small";
    source.sourcepath = modelPath;
    source.format = "obj";
    source.resourcescopeid = scope.resourcescopeid;
    source.sourcehash =
        slicer_core::ComputeSha256(ReadFile(modelPath));
    source.resourcehash =
        slicer_core::ComputeSceneResourceHash(model);
    source.displayname = "policy_textured_small";
    scene.models.push_back(source);

    for (int index{0}; index < 3; ++index)
    {
        slicer_core::SceneModelInstance item;
        item.instance.instanceid =
            "instance-" + std::to_string(index + 1);
        item.instance.modelid = source.modelid;
        item.instance.sourcetransformidentity =
            modelPath.generic_string();
        item.instance.sourcebboxmm = model.bbox_mm;
        item.instance.transform.translatexmm =
            static_cast<double>(index) * 5.0;
        item.instance.effectivebboxmm = model.bbox_mm;
        item.instance.effectivebboxmm.min.x +=
            item.instance.transform.translatexmm;
        item.instance.effectivebboxmm.max.x +=
            item.instance.transform.translatexmm;
        item.requestedtransform = item.instance.transform;
        item.effectivetransform = item.instance.transform;
        item.admissionstatus =
            slicer_core::SceneInstanceAdmissionStatus::Admitted;
        item.resolvedprofileid = scene.resolvedprofileid;
        scene.instances.push_back(std::move(item));
    }
    return scene;
}

slicer_core::SceneEffectiveConfigRequest MakeEffectiveRequest(
    const std::filesystem::path& root,
    const std::filesystem::path& profileConfigPath)
{
    slicer_core::SceneEffectiveConfigRequest request;
    request.scene = MakeThreeInstanceScene(profileConfigPath);
    request.sourcescenepath = root / "scene_config.draft.json";
    request.generatedconfigpath =
        root / "scene_config.effective.json";
    request.sourceprofileid =
        request.scene.resolvedprofileid;
    request.sourceprofileconfigpath = profileConfigPath;
    request.outputpackagedir = root / "package";
    request.generatedatutc = "2026-07-28T16:00:00.000Z";
    request.dpix = 600;
    request.dpiy = 600;
    request.layerheightmm = 0.01;
    request.slicepipelinemode = "legacy";
    return request;
}

bool StableErrorNames()
{
    using slicer_core::MultiModelProductionErrorCode;
    const struct
    {
        MultiModelProductionErrorCode code;
        std::string expected;
    } cases[]{
        {MultiModelProductionErrorCode::None, "NONE"},
        {MultiModelProductionErrorCode::EffectiveConfigInvalid,
         "SCENE_EFFECTIVE_CONFIG_INVALID"},
        {MultiModelProductionErrorCode::EffectiveConfigStale,
         "SCENE_EFFECTIVE_CONFIG_STALE"},
        {MultiModelProductionErrorCode::ResourceUnresolved,
         "SCENE_RESOURCE_UNRESOLVED"},
        {MultiModelProductionErrorCode::ProfileMismatch,
         "SCENE_PROFILE_MISMATCH"},
        {MultiModelProductionErrorCode::BuildVolumeUndefined,
         "SCENE_BUILD_VOLUME_UNDEFINED"},
        {MultiModelProductionErrorCode::PipelineModeNotAdmitted,
         "SCENE_PIPELINE_MODE_NOT_ADMITTED"},
        {MultiModelProductionErrorCode::ProductionPackageInvalid,
         "SCENE_PRODUCTION_PACKAGE_INVALID"},
    };

    bool ok{true};
    for (const auto& item : cases)
    {
        ok = ExpectTrue(
                 slicer_core::MultiModelProductionErrorCodeName(
                     item.code)
                     == item.expected,
                 "production error code follows stable contract")
            && ok;
    }
    return ok;
}

bool ThreeInstanceSceneWritesOneStrictPackage()
{
    const std::filesystem::path sourceRoot{SLICESOFT_SOURCE_DIR};
    const std::filesystem::path profileConfigPath =
        sourceRoot
        / "samples/configs/golden/material_process_top2_fixture.json";
    const std::filesystem::path root =
        std::filesystem::temp_directory_path()
        / "slicesoft_13b_08_multi_model_production";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    const slicer_core::SceneEffectiveConfigRequest effectiveRequest =
        MakeEffectiveRequest(root, profileConfigPath);
    const slicer_core::SceneEffectiveConfigResult effective =
        slicer_core::WriteSceneEffectiveConfig(effectiveRequest);
    if (!ExpectTrue(
            effective.IsValid(),
            "three-instance effective config writes"))
    {
        return false;
    }

    slicer_core::MultiModelProductionRequest request;
    request.effectiveconfigpath =
        effectiveRequest.generatedconfigpath;
    const slicer_core::MultiModelProductionResult result =
        slicer_core::RunMultiModelProductionService(request);
    if (!ExpectTrue(
            result.IsValid(),
            "three-instance scene production succeeds"))
    {
        if (result.error.has_value())
        {
            std::cerr << "  error: " << result.error->message << '\n';
        }
        return false;
    }

    const slicer_core::RipValidationResult rip =
        slicer_core::validate_slice_package(result.packagedir);
    std::ifstream manifestInput(
        result.packagedir / "manifest.json",
        std::ios::binary);
    const slicer_core::Json manifest =
        slicer_core::Json::parse(manifestInput);
    const bool ok =
        ExpectTrue(
            result.visibleinstancecount == 3U,
            "all three visible instances are produced")
        && ExpectTrue(
            rip.schema == "p0.rgbwsv.2"
                && rip.bit_depth == 8
                && rip.layer_count == result.layercount
                && rip.warnings_count == 0,
            "scene package passes strict RGBWSV validation")
        && ExpectTrue(
            manifest.at("scene").at("sceneId").as_string()
                    == effectiveRequest.scene.sceneid
                && static_cast<std::uint64_t>(
                       manifest.at("scene")
                           .at("sceneRevision")
                           .as_double())
                    == effectiveRequest.scene.scenerevision,
            "manifest scene identity matches the effective config");
    std::filesystem::remove_all(root, cleanupError);
    return ok;
}

bool HiddenInstanceIsReportedAndNotProduced()
{
    const std::filesystem::path sourceRoot{SLICESOFT_SOURCE_DIR};
    const std::filesystem::path profileConfigPath =
        sourceRoot
        / "samples/configs/golden/material_process_top2_fixture.json";
    const std::filesystem::path root =
        std::filesystem::temp_directory_path()
        / "slicesoft_13b_08_hidden_scene_instance";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    slicer_core::SceneEffectiveConfigRequest effectiveRequest =
        MakeEffectiveRequest(root, profileConfigPath);
    effectiveRequest.scene.instances.back().instance.visible = false;
    const slicer_core::SceneEffectiveConfigResult effective =
        slicer_core::WriteSceneEffectiveConfig(effectiveRequest);
    if (!ExpectTrue(
            effective.IsValid(),
            "effective config with one hidden instance writes"))
    {
        return false;
    }

    slicer_core::MultiModelProductionRequest request;
    request.effectiveconfigpath =
        effectiveRequest.generatedconfigpath;
    const slicer_core::MultiModelProductionResult result =
        slicer_core::RunMultiModelProductionService(request);
    if (!ExpectTrue(
            result.IsValid(),
            "scene with one hidden instance produces"))
    {
        if (result.error.has_value())
        {
            std::cerr << "  error: " << result.error->message << '\n';
        }
        return false;
    }

    std::ifstream reportInput(
        result.packagedir
            / "reports/multimodel_scene_report.json",
        std::ios::binary);
    const slicer_core::Json report =
        slicer_core::Json::parse(reportInput);
    const bool ok =
        ExpectTrue(
            result.visibleinstancecount == 2U,
            "only visible instances are produced")
        && ExpectTrue(
            report.at("visibleInstanceCount").as_double() == 2.0
                && report.at("hiddenInstanceCount").as_double() == 1.0,
            "scene report preserves visible and hidden counts")
        && ExpectTrue(
            report.at("composition")
                        .at("visibleInstanceCount")
                        .as_double()
                    == 2.0
                && report.at("composition")
                        .at("hiddenInstanceCount")
                        .as_double()
                    == 1.0,
            "composition report preserves the hidden instance");
    std::filesystem::remove_all(root, cleanupError);
    return ok;
}

bool MissingProfileAndGlobalModeFailClosed()
{
    const std::filesystem::path sourceRoot{SLICESOFT_SOURCE_DIR};
    const std::filesystem::path profileConfigPath =
        sourceRoot
        / "samples/configs/golden/material_process_top2_fixture.json";
    const std::filesystem::path root =
        std::filesystem::temp_directory_path()
        / "slicesoft_13b_08_multi_model_negative";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    slicer_core::SceneEffectiveConfigRequest missingProfile =
        MakeEffectiveRequest(root / "missing", profileConfigPath);
    missingProfile.sourceprofileconfigpath =
        root / "missing-profile.json";
    const auto missingEffective =
        slicer_core::WriteSceneEffectiveConfig(missingProfile);
    slicer_core::MultiModelProductionRequest missingRequest;
    missingRequest.effectiveconfigpath =
        missingProfile.generatedconfigpath;
    const auto missingResult =
        slicer_core::RunMultiModelProductionService(
            missingRequest);

    slicer_core::SceneEffectiveConfigRequest global =
        MakeEffectiveRequest(root / "global", profileConfigPath);
    global.slicepipelinemode = "global_surface_shell";
    const auto globalEffective =
        slicer_core::WriteSceneEffectiveConfig(global);
    slicer_core::MultiModelProductionRequest globalRequest;
    globalRequest.effectiveconfigpath =
        global.generatedconfigpath;
    const auto globalResult =
        slicer_core::RunMultiModelProductionService(globalRequest);

    const bool ok =
        ExpectTrue(
            missingEffective.IsValid()
                && !missingResult.IsValid()
                && missingResult.error->code
                    == slicer_core::MultiModelProductionErrorCode::
                        ResourceUnresolved,
            "missing explicit Profile config fails closed")
        && ExpectTrue(
            globalEffective.IsValid()
                && !globalResult.IsValid()
                && globalResult.error->code
                    == slicer_core::MultiModelProductionErrorCode::
                        PipelineModeNotAdmitted
                && !std::filesystem::exists(
                    global.outputpackagedir),
            "unadmitted Global scene mode never falls back to Legacy");
    std::filesystem::remove_all(root, cleanupError);
    return ok;
}

bool SceneContractFailuresAreStable()
{
    const std::filesystem::path sourceRoot{SLICESOFT_SOURCE_DIR};
    const std::filesystem::path profileConfigPath =
        sourceRoot
        / "samples/configs/golden/material_process_top2_fixture.json";
    const std::filesystem::path root =
        std::filesystem::temp_directory_path()
        / "slicesoft_13b_08_scene_contract_negative";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    slicer_core::SceneEffectiveConfigRequest noVisible =
        MakeEffectiveRequest(root / "no-visible", profileConfigPath);
    for (slicer_core::SceneModelInstance& item :
         noVisible.scene.instances)
    {
        item.instance.visible = false;
    }
    const auto noVisibleEffective =
        slicer_core::WriteSceneEffectiveConfig(noVisible);
    slicer_core::MultiModelProductionRequest noVisibleRequest;
    noVisibleRequest.effectiveconfigpath =
        noVisible.generatedconfigpath;
    const auto noVisibleResult =
        slicer_core::RunMultiModelProductionService(
            noVisibleRequest);

    slicer_core::SceneEffectiveConfigRequest noVolume =
        MakeEffectiveRequest(root / "no-volume", profileConfigPath);
    noVolume.scene.buildvolume =
        slicer_core::SceneBuildVolume{};
    const auto noVolumeEffective =
        slicer_core::WriteSceneEffectiveConfig(noVolume);
    slicer_core::MultiModelProductionRequest noVolumeRequest;
    noVolumeRequest.effectiveconfigpath =
        noVolume.generatedconfigpath;
    const auto noVolumeResult =
        slicer_core::RunMultiModelProductionService(
            noVolumeRequest);

    slicer_core::SceneEffectiveConfigRequest profileMismatch =
        MakeEffectiveRequest(root / "profile-mismatch", profileConfigPath);
    profileMismatch.scene.resolvedprofileid =
        "different-profile";
    profileMismatch.sourceprofileid =
        profileMismatch.scene.resolvedprofileid;
    for (slicer_core::SceneModelInstance& item :
         profileMismatch.scene.instances)
    {
        item.resolvedprofileid =
            profileMismatch.scene.resolvedprofileid;
    }
    const auto mismatchEffective =
        slicer_core::WriteSceneEffectiveConfig(profileMismatch);
    slicer_core::MultiModelProductionRequest mismatchRequest;
    mismatchRequest.effectiveconfigpath =
        profileMismatch.generatedconfigpath;
    const auto mismatchResult =
        slicer_core::RunMultiModelProductionService(
            mismatchRequest);

    const bool ok =
        ExpectTrue(
            noVisibleEffective.IsValid()
                && !noVisibleResult.IsValid()
                && noVisibleResult.error->code
                    == slicer_core::MultiModelProductionErrorCode::
                        EffectiveConfigInvalid,
            "scene without visible instances fails closed")
        && ExpectTrue(
            noVolumeEffective.IsValid()
                && !noVolumeResult.IsValid()
                && noVolumeResult.error->code
                    == slicer_core::MultiModelProductionErrorCode::
                        BuildVolumeUndefined,
            "scene without explicit build volume fails closed")
        && ExpectTrue(
            mismatchEffective.IsValid()
                && !mismatchResult.IsValid()
                && mismatchResult.error->code
                    == slicer_core::MultiModelProductionErrorCode::
                        ProfileMismatch,
            "scene-wide Profile mismatch fails closed");
    std::filesystem::remove_all(root, cleanupError);
    return ok;
}

}  // namespace

int main(const int argc, char** argv)
{
    if (argc == 3
        && std::string(argv[1]) == "--emit-fixture")
    {
        const std::filesystem::path root = argv[2];
        const std::filesystem::path profileConfigPath =
            std::filesystem::path(SLICESOFT_SOURCE_DIR)
            / "samples/configs/golden/material_process_top2_fixture.json";
        std::error_code cleanupError;
        std::filesystem::remove_all(root, cleanupError);
        std::filesystem::create_directories(root);
        const slicer_core::SceneEffectiveConfigRequest request =
            MakeEffectiveRequest(root, profileConfigPath);
        const slicer_core::SceneEffectiveConfigResult result =
            slicer_core::WriteSceneEffectiveConfig(request);
        if (!result.IsValid())
        {
            std::cerr
                << "failed to emit scene route fixture\n";
            return 1;
        }
        std::cout
            << request.generatedconfigpath.generic_string()
            << '\n';
        return 0;
    }

    const bool ok =
        StableThreeMfResourceIdentityIgnoresExtractionRoot()
        && StableErrorNames()
        && ThreeInstanceSceneWritesOneStrictPackage()
        && HiddenInstanceIsReportedAndNotProduced()
        && MissingProfileAndGlobalModeFailClosed()
        && SceneContractFailuresAreStable();
    if (!ok)
    {
        return 1;
    }
    std::cout
        << "multi_model_production_service_unit_tests: PASS\n";
    return 0;
}
