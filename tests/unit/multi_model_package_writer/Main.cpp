#include "slicer_core/json_value.h"
#include "slicer_core/output/rgbwsv/RgbwsvPackageWriter.h"
#include "slicer_core/pipeline/MultiModelScenePackageWriter.h"
#include "slicer_core/reports/MultiModelSceneReport.h"
#include "slicer_core/rip_reader.h"
#include "slicer_core/scene/MultiModelScene.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace
{

constexpr int kWidth{8};
constexpr int kHeight{4};
constexpr int kLayerCount{2};
constexpr std::size_t kChannelCount{6U};

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
    }
    return condition;
}

std::filesystem::path MakeTestDirectory(const std::string& name)
{
    const auto suffix =
        std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path directory =
        std::filesystem::temp_directory_path()
        / ("slicesoft_scene_package_" + name + "_"
           + std::to_string(suffix));
    std::filesystem::create_directories(directory);
    return directory;
}

slicer_core::Json ReadJson(const std::filesystem::path& path)
{
    std::ifstream input{path, std::ios::binary};
    if (!input)
    {
        throw std::runtime_error(
            "failed to read fixture JSON: " + path.string());
    }
    return slicer_core::Json::parse(input);
}

slicer_core::ModelSource MakeSource(
    const std::string& modelId,
    const std::string& fileName)
{
    slicer_core::ModelSource source;
    source.modelid = modelId;
    source.sourcepath = "fixtures" / std::filesystem::path{fileName};
    source.format = "obj";
    source.resourcescopeid = "scope_" + modelId;
    source.sourcehash = "source_hash_" + modelId;
    source.resourcehash = "resource_hash_" + modelId;
    source.displayname = modelId;
    return source;
}

slicer_core::SceneModelInstance MakeInstance(
    const std::string& modelId,
    const std::string& instanceId,
    const double translateX)
{
    slicer_core::SceneModelInstance result;
    result.instance.modelid = modelId;
    result.instance.instanceid = instanceId;
    result.instance.sourcetransformidentity =
        "fixtures/" + modelId + ".obj";
    result.instance.transformrevision = 3U;
    result.instance.visible = true;
    result.instance.transform.translatexmm = translateX;
    result.requestedtransform = result.instance.transform;
    result.effectivetransform = result.instance.transform;
    result.admissionstatus =
        slicer_core::SceneInstanceAdmissionStatus::Admitted;
    result.resolvedprofileid = "fixture_profile";
    return result;
}

slicer_core::MultiModelScene MakeScene()
{
    slicer_core::MultiModelScene scene;
    scene.sceneid = "scene_package_fixture";
    scene.scenerevision = 7U;
    scene.resolvedprofileid = "fixture_profile";
    scene.buildvolume.source =
        slicer_core::BuildVolumeSource::Fixture;
    scene.buildvolume.widthmm = 50.0;
    scene.buildvolume.heightmm = 30.0;
    scene.buildvolume.origin =
        slicer_core::BuildVolumeOrigin::LowerLeft;
    scene.buildvolume.xdirection =
        slicer_core::BuildVolumeAxisDirection::Positive;
    scene.buildvolume.ydirection =
        slicer_core::BuildVolumeAxisDirection::Positive;
    scene.buildvolume.isfixture = true;
    scene.resourcescopes.push_back({
        "scope_model_a",
        slicer_core::ResourceScopeKind::ObjDirectory,
        "fixtures",
        {},
        {},
    });
    scene.resourcescopes.push_back({
        "scope_model_b",
        slicer_core::ResourceScopeKind::ObjDirectory,
        "fixtures",
        {},
        {},
    });
    scene.models.push_back(MakeSource("model_a", "a.obj"));
    scene.models.push_back(MakeSource("model_b", "b.obj"));
    scene.instances.push_back(
        MakeInstance("model_a", "instance_a", 0.0));
    scene.instances.push_back(
        MakeInstance("model_b", "instance_b", 5.0));
    return scene;
}

slicer_core::MultiModelScene MakeSingleInstanceScene()
{
    slicer_core::MultiModelScene scene = MakeScene();
    scene.resourcescopes.resize(1U);
    scene.models.resize(1U);
    scene.instances.resize(1U);
    return scene;
}

slicer_core::SceneCollisionResult MakeAdmission(
    const slicer_core::MultiModelScene& scene)
{
    slicer_core::SceneCollisionResult result;
    result.sceneid = scene.sceneid;
    result.sourcescenerevision = scene.scenerevision;
    result.purpose =
        slicer_core::SceneValidationPurpose::FunctionalFixture;
    result.buildvolume = scene.buildvolume;
    result.scenestatus = slicer_core::SceneCollisionStatus::Passed;
    result.functionalallowed = true;
    for (const slicer_core::SceneModelInstance& instance :
         scene.instances)
    {
        slicer_core::SceneCollisionInstanceResult item;
        item.modelid = instance.instance.modelid;
        item.instanceid = instance.instance.instanceid;
        item.transformrevision =
            instance.instance.transformrevision;
        const slicer_core::ModelTransformHashResult transformHash =
            slicer_core::ComputeModelTransformHash(
                instance.instance.transform,
                instance.instance.sourcetransformidentity,
                instance.instance.instanceid,
                instance.instance.modelid);
        item.transformhash = transformHash.hash;
        item.visible = true;
        item.admissionstatus =
            slicer_core::SceneInstanceAdmissionStatus::Admitted;
        item.boundsvalid = true;
        item.inbounds = true;
        result.instances.push_back(std::move(item));
    }
    result.statistics.totalinstancecount = scene.instances.size();
    result.statistics.visibleinstancecount = scene.instances.size();
    return result;
}

slicer_core::RgbwsvProductionLayer MakeLayer(const int layerIndex)
{
    slicer_core::RgbwsvProductionLayer layer;
    layer.layerIndex = layerIndex;
    layer.zMm = (static_cast<double>(layerIndex) + 0.5) * 0.01;
    layer.widthPx = kWidth;
    layer.heightPx = kHeight;
    layer.channels.assign(
        static_cast<std::size_t>(kWidth * kHeight)
            * kChannelCount,
        255U);
    const std::size_t first =
        (static_cast<std::size_t>(layerIndex + 1) * kChannelCount);
    const std::size_t second =
        (static_cast<std::size_t>(kWidth + layerIndex + 4)
         * kChannelCount);
    layer.channels.at(first) = 12U;
    layer.channels.at(first + 1U) = 34U;
    layer.channels.at(first + 2U) = 56U;
    layer.channels.at(second + 3U) = 0U;
    return layer;
}

slicer_core::SceneLayerComposeResult MakeComposition(
    const slicer_core::MultiModelScene& scene)
{
    slicer_core::SceneLayerComposeResult result;
    result.available = true;
    result.status = "ready_for_writer";
    result.sceneid = scene.sceneid;
    result.scenerevision = scene.scenerevision;
    result.grid.widthpx = kWidth;
    result.grid.heightpx = kHeight;
    result.grid.layercount = kLayerCount;
    result.grid.originxmm = 0.0;
    result.grid.originymm = 0.0;
    result.grid.originzmm = 0.0;
    result.grid.pitchxmm = 25.4 / 635.0;
    result.grid.pitchymm = 25.4 / 600.0;
    result.grid.layerthicknessmm = 0.01;
    result.protocol = slicer_core::FixedSceneRasterProtocol();
    result.effectivepipelinemode =
        slicer_core::SlicePipelineMode::Legacy;
    for (int layerIndex{0}; layerIndex < kLayerCount; ++layerIndex)
    {
        result.layers.push_back(MakeLayer(layerIndex));
    }
    result.statistics.totalinstancecount = 2U;
    result.statistics.visibleinstancecount = 2U;
    result.statistics.outputlayercount = kLayerCount;
    result.statistics.modelpixels = 4U;
    result.statistics.emptypixels =
        static_cast<std::size_t>(kWidth * kHeight * kLayerCount) - 4U;
    result.statistics.instances = {
        {"instance_a", 2U, 0U, 0U},
        {"instance_b", 2U, 0U, 0U},
    };
    result.composems = 2.5;
    result.peakworkingbytes = 4096U;
    return result;
}

slicer_core::SceneLayerComposeResult MakeSingleInstanceComposition(
    const slicer_core::MultiModelScene& scene)
{
    slicer_core::SceneLayerComposeResult result =
        MakeComposition(scene);
    result.statistics.totalinstancecount = 1U;
    result.statistics.visibleinstancecount = 1U;
    result.statistics.instances = {
        {"instance_a", 4U, 0U, 0U},
    };
    return result;
}

slicer_core::RgbwsvProductionPackageWriteRequest MakeRequest(
    const std::filesystem::path& packageDir)
{
    slicer_core::RgbwsvProductionPackageWriteRequest request;
    request.packageDir = packageDir;
    request.sourceConfigPath = "fixtures/scene.json";
    request.sourceFormat = "scene";
    request.requestedPipelineMode = "legacy";
    request.effectivePipelineMode = "legacy";
    request.productionAcceptance = "functional_fixture_admitted";
    request.grid.dpiX = 635;
    request.grid.dpiY = 600;
    request.preview.enabled = false;
    request.preview.format = "png";
    request.preview.interval = 1;
    return request;
}

bool ScenePackageContainsOneGlobalTiffPerLayer()
{
    const std::filesystem::path directory =
        MakeTestDirectory("positive");
    const std::filesystem::path packageDir = directory / "package";
    const slicer_core::MultiModelScene scene = MakeScene();
    const slicer_core::SceneCollisionResult admission =
        MakeAdmission(scene);
    const auto result =
        slicer_core::WriteMultiModelSceneProductionPackage(
            MakeRequest(packageDir),
            MakeComposition(scene),
            scene,
            admission);
    const slicer_core::RipValidationResult rip =
        slicer_core::validate_slice_package(packageDir);
    const slicer_core::Json manifest =
        ReadJson(packageDir / "manifest.json");
    const slicer_core::Json report = ReadJson(
        packageDir / slicer_core::MultiModelSceneReportRelativePath());

    std::size_t tiffCount{0U};
    for (const auto& entry :
         std::filesystem::directory_iterator(packageDir / "layers"))
    {
        if (entry.path().extension() == ".tiff")
        {
            ++tiffCount;
        }
    }

    return ExpectTrue(
               result.productionOutputWritten,
               "scene package reports production-format output")
        && ExpectTrue(
            result.strictProtocolValidated,
            "scene package reports staged strict RGBWSV validation")
        && ExpectTrue(
            tiffCount == kLayerCount,
            "scene package contains one global TIFF per layer")
        && ExpectTrue(
            rip.schema == "p0.rgbwsv.2"
                && rip.bit_depth == 8
                && rip.layer_count == kLayerCount,
            "RIP strict accepts the scene package")
        && ExpectTrue(
            manifest.at("schema").as_string() == "p0.rgbwsv.2",
            "scene package keeps the fixed manifest schema")
        && ExpectTrue(
            manifest.at("scene").at("sceneId").as_string()
                == scene.sceneid,
            "manifest records scene identity")
        && ExpectTrue(
            !manifest.at("scene").at("productionReady").as_bool(),
            "functional fixture is not reported as production ready")
        && ExpectTrue(
            manifest.at("reports").at("scene").as_string()
                == slicer_core::MultiModelSceneReportRelativePath()
                       .generic_string(),
            "manifest links the scene report")
        && ExpectTrue(
            report.at("schema").as_string()
                == slicer_core::MultiModelSceneReportSchemaName(),
            "scene report uses the fixed schema")
        && ExpectTrue(
            report.at("instances").size() == 2U,
            "scene report records both instances")
        && ExpectTrue(
            report.at("composition")
                    .at("totals")
                    .at("modelPixels")
                    .as_int()
                == 4,
            "scene report preserves composition totals");
}

bool SingleInstanceSceneUsesTheSamePackageContract()
{
    const std::filesystem::path directory =
        MakeTestDirectory("single_instance");
    const std::filesystem::path packageDir = directory / "package";
    const slicer_core::MultiModelScene scene =
        MakeSingleInstanceScene();

    (void)slicer_core::WriteMultiModelSceneProductionPackage(
        MakeRequest(packageDir),
        MakeSingleInstanceComposition(scene),
        scene,
        MakeAdmission(scene));
    const slicer_core::RipValidationResult rip =
        slicer_core::validate_slice_package(packageDir);
    const slicer_core::Json manifest =
        ReadJson(packageDir / "manifest.json");
    const slicer_core::Json report = ReadJson(
        packageDir / slicer_core::MultiModelSceneReportRelativePath());
    return ExpectTrue(
               rip.layer_count == kLayerCount,
               "single-instance scene keeps one global TIFF per layer")
        && ExpectTrue(
            manifest.at("scene").at("instanceCount").as_int() == 1,
            "single-instance manifest records one scene instance")
        && ExpectTrue(
            report.at("instances").size() == 1U,
            "single-instance scene report records one instance");
}

bool LegacyWriterWithoutSceneExtensionStaysCompatible()
{
    const std::filesystem::path directory =
        MakeTestDirectory("legacy_compat");
    const std::filesystem::path packageDir = directory / "package";
    slicer_core::RgbwsvProductionPackageWriteRequest request =
        MakeRequest(packageDir);
    request.sourceModelPath = "fixtures/a.obj";
    request.sourceFormat = "obj";
    request.productionAcceptance = "admitted";
    request.grid.widthPx = kWidth;
    request.grid.heightPx = kHeight;
    request.grid.layerCount = kLayerCount;
    request.grid.pixelSizeXmm = 25.4 / 635.0;
    request.grid.pixelSizeYmm = 25.4 / 600.0;
    request.grid.layerThicknessMm = 0.01;
    request.layers = {MakeLayer(0), MakeLayer(1)};

    (void)slicer_core::WriteRgbwsvProductionPackage(request);
    const slicer_core::Json manifest =
        ReadJson(packageDir / "manifest.json");
    return ExpectTrue(
               !manifest.contains("scene"),
               "legacy writer does not add an empty scene summary")
        && ExpectTrue(
            !manifest.at("reports").contains("scene"),
            "legacy writer does not add an empty scene report link")
        && ExpectTrue(
            !std::filesystem::exists(
                packageDir
                / slicer_core::MultiModelSceneReportRelativePath()),
            "legacy writer creates no scene report");
}

bool BlockedCompositionPublishesNothing()
{
    const std::filesystem::path directory =
        MakeTestDirectory("blocked");
    const std::filesystem::path packageDir = directory / "package";
    const slicer_core::MultiModelScene scene = MakeScene();
    slicer_core::SceneLayerComposeResult composition =
        MakeComposition(scene);
    composition.available = false;
    composition.status = "blocked";
    composition.layers.clear();
    try
    {
        (void)slicer_core::WriteMultiModelSceneProductionPackage(
            MakeRequest(packageDir),
            std::move(composition),
            scene,
            MakeAdmission(scene));
    }
    catch (const std::exception&)
    {
        return ExpectTrue(
            !std::filesystem::exists(packageDir),
            "blocked composition publishes no package");
    }
    return ExpectTrue(false, "blocked composition must fail closed");
}

bool SceneIdentityMismatchPreservesExistingPackage()
{
    const std::filesystem::path directory =
        MakeTestDirectory("identity");
    const std::filesystem::path packageDir = directory / "package";
    std::filesystem::create_directories(packageDir);
    {
        std::ofstream sentinel{packageDir / "sentinel.txt"};
        sentinel << "old-success";
    }

    const slicer_core::MultiModelScene scene = MakeScene();
    slicer_core::SceneLayerComposeResult composition =
        MakeComposition(scene);
    composition.sceneid = "other_scene";
    try
    {
        (void)slicer_core::WriteMultiModelSceneProductionPackage(
            MakeRequest(packageDir),
            std::move(composition),
            scene,
            MakeAdmission(scene));
    }
    catch (const std::exception&)
    {
        return ExpectTrue(
                   std::filesystem::exists(
                       packageDir / "sentinel.txt"),
                   "identity failure preserves the previous package")
            && ExpectTrue(
                !std::filesystem::exists(
                    packageDir / "manifest.json"),
                "identity failure publishes no replacement manifest");
    }
    return ExpectTrue(false, "scene identity mismatch must fail closed");
}

bool InvalidTypedSceneExtensionPublishesNothing()
{
    const std::filesystem::path directory =
        MakeTestDirectory("extension");
    const std::filesystem::path packageDir = directory / "package";
    const slicer_core::MultiModelScene scene = MakeScene();
    const slicer_core::SceneCollisionResult admission =
        MakeAdmission(scene);
    const slicer_core::SceneLayerComposeResult composition =
        MakeComposition(scene);
    slicer_core::RgbwsvProductionPackageWriteRequest request =
        MakeRequest(packageDir);
    request.grid.widthPx = kWidth;
    request.grid.heightPx = kHeight;
    request.grid.layerCount = kLayerCount;
    request.grid.pixelSizeXmm = 25.4 / 635.0;
    request.grid.pixelSizeYmm = 25.4 / 600.0;
    request.grid.layerThicknessMm = 0.01;
    request.layers = composition.layers;
    request.scene = slicer_core::BuildMultiModelSceneReport(
        scene,
        admission,
        composition,
        "legacy",
        packageDir);
    slicer_core::Json::Object invalidReport =
        request.scene->report.as_object();
    invalidReport["schema"] = "invalid.scene.report";
    request.scene->report =
        slicer_core::Json{std::move(invalidReport)};
    try
    {
        (void)slicer_core::WriteRgbwsvProductionPackage(request);
    }
    catch (const std::exception&)
    {
        return ExpectTrue(
            !std::filesystem::exists(packageDir),
            "invalid typed scene extension publishes nothing");
    }
    return ExpectTrue(false, "invalid scene extension must fail closed");
}

bool MutatedLayerProtocolPublishesNothing()
{
    const std::filesystem::path directory =
        MakeTestDirectory("layer_protocol");
    const std::filesystem::path packageDir = directory / "package";
    const slicer_core::MultiModelScene scene = MakeScene();
    slicer_core::SceneLayerComposeResult composition =
        MakeComposition(scene);
    composition.layers.front().channelOrder.at(0U) = "B";
    try
    {
        (void)slicer_core::WriteMultiModelSceneProductionPackage(
            MakeRequest(packageDir),
            std::move(composition),
            scene,
            MakeAdmission(scene));
    }
    catch (const std::exception&)
    {
        return ExpectTrue(
            !std::filesystem::exists(packageDir),
            "mutated layer protocol publishes nothing");
    }
    return ExpectTrue(false, "mutated layer protocol must fail closed");
}

bool StaleTransformEvidencePublishesNothing()
{
    const std::filesystem::path directory =
        MakeTestDirectory("stale_transform");
    const std::filesystem::path packageDir = directory / "package";
    const slicer_core::MultiModelScene scene = MakeScene();
    slicer_core::SceneCollisionResult admission =
        MakeAdmission(scene);
    admission.instances.front().transformhash = "stale";
    try
    {
        (void)slicer_core::WriteMultiModelSceneProductionPackage(
            MakeRequest(packageDir),
            MakeComposition(scene),
            scene,
            admission);
    }
    catch (const std::exception&)
    {
        return ExpectTrue(
            !std::filesystem::exists(packageDir),
            "stale transform evidence publishes nothing");
    }
    return ExpectTrue(false, "stale transform evidence must fail closed");
}

bool ContradictoryAdmissionPublishesNothing()
{
    const std::filesystem::path directory =
        MakeTestDirectory("admission_contradiction");
    const std::filesystem::path packageDir = directory / "package";
    const slicer_core::MultiModelScene scene = MakeScene();
    slicer_core::SceneCollisionResult admission =
        MakeAdmission(scene);
    admission.instances.front().inbounds = false;
    try
    {
        (void)slicer_core::WriteMultiModelSceneProductionPackage(
            MakeRequest(packageDir),
            MakeComposition(scene),
            scene,
            admission);
    }
    catch (const std::exception&)
    {
        return ExpectTrue(
            !std::filesystem::exists(packageDir),
            "contradictory admission publishes nothing");
    }
    return ExpectTrue(
        false,
        "contradictory admission evidence must fail closed");
}

bool CompositionStatisticsMismatchPublishesNothing()
{
    const std::filesystem::path directory =
        MakeTestDirectory("statistics_mismatch");
    const std::filesystem::path packageDir = directory / "package";
    const slicer_core::MultiModelScene scene = MakeScene();
    slicer_core::SceneLayerComposeResult composition =
        MakeComposition(scene);
    ++composition.statistics.modelpixels;
    try
    {
        (void)slicer_core::WriteMultiModelSceneProductionPackage(
            MakeRequest(packageDir),
            std::move(composition),
            scene,
            MakeAdmission(scene));
    }
    catch (const std::exception&)
    {
        return ExpectTrue(
            !std::filesystem::exists(packageDir),
            "statistics mismatch publishes nothing");
    }
    return ExpectTrue(
        false,
        "composition statistics mismatch must fail closed");
}

bool SceneBuildVolumeMismatchPublishesNothing()
{
    const std::filesystem::path directory =
        MakeTestDirectory("volume_mismatch");
    const std::filesystem::path packageDir = directory / "package";
    const slicer_core::MultiModelScene scene = MakeScene();
    slicer_core::SceneCollisionResult admission =
        MakeAdmission(scene);
    admission.buildvolume.widthmm = 55.0;
    try
    {
        (void)slicer_core::WriteMultiModelSceneProductionPackage(
            MakeRequest(packageDir),
            MakeComposition(scene),
            scene,
            admission);
    }
    catch (const std::exception&)
    {
        return ExpectTrue(
            !std::filesystem::exists(packageDir),
            "build-volume mismatch publishes nothing");
    }
    return ExpectTrue(
        false,
        "build-volume mismatch must fail closed");
}

bool FixtureCannotClaimProductionAdmission()
{
    const std::filesystem::path directory =
        MakeTestDirectory("fixture_claim");
    const std::filesystem::path packageDir = directory / "package";
    const slicer_core::MultiModelScene scene = MakeScene();
    slicer_core::RgbwsvProductionPackageWriteRequest request =
        MakeRequest(packageDir);
    request.productionAcceptance = "admitted";
    try
    {
        (void)slicer_core::WriteMultiModelSceneProductionPackage(
            std::move(request),
            MakeComposition(scene),
            scene,
            MakeAdmission(scene));
    }
    catch (const std::exception&)
    {
        return ExpectTrue(
            !std::filesystem::exists(packageDir),
            "fixture production claim publishes nothing");
    }
    return ExpectTrue(
        false,
        "fixture output must use an explicit fixture acceptance");
}

bool SceneReportPathMutationPublishesNothing()
{
    const std::filesystem::path directory =
        MakeTestDirectory("report_path");
    const std::filesystem::path packageDir = directory / "package";
    const slicer_core::MultiModelScene scene = MakeScene();
    const slicer_core::SceneCollisionResult admission =
        MakeAdmission(scene);
    const slicer_core::SceneLayerComposeResult composition =
        MakeComposition(scene);
    slicer_core::RgbwsvProductionPackageWriteRequest request =
        MakeRequest(packageDir);
    request.grid.widthPx = kWidth;
    request.grid.heightPx = kHeight;
    request.grid.layerCount = kLayerCount;
    request.grid.pixelSizeXmm = 25.4 / 635.0;
    request.grid.pixelSizeYmm = 25.4 / 600.0;
    request.grid.layerThicknessMm = 0.01;
    request.layers = composition.layers;
    request.scene = slicer_core::BuildMultiModelSceneReport(
        scene,
        admission,
        composition,
        "legacy",
        packageDir);
    slicer_core::Json::Object invalidSummary =
        request.scene->manifestsummary.as_object();
    invalidSummary["sceneReport"] = "reports/other.json";
    request.scene->manifestsummary =
        slicer_core::Json{std::move(invalidSummary)};
    try
    {
        (void)slicer_core::WriteRgbwsvProductionPackage(request);
    }
    catch (const std::exception&)
    {
        return ExpectTrue(
            !std::filesystem::exists(packageDir),
            "scene report path mutation publishes nothing");
    }
    return ExpectTrue(
        false,
        "scene report path mutation must fail closed");
}

bool PipelineModeMismatchPublishesNothing()
{
    const std::filesystem::path directory =
        MakeTestDirectory("mode_mismatch");
    const std::filesystem::path packageDir = directory / "package";
    const slicer_core::MultiModelScene scene = MakeScene();
    slicer_core::RgbwsvProductionPackageWriteRequest request =
        MakeRequest(packageDir);
    request.requestedPipelineMode = "global_surface_shell";
    request.effectivePipelineMode = "global_surface_shell";
    try
    {
        (void)slicer_core::WriteMultiModelSceneProductionPackage(
            std::move(request),
            MakeComposition(scene),
            scene,
            MakeAdmission(scene));
    }
    catch (const std::exception&)
    {
        return ExpectTrue(
            !std::filesystem::exists(packageDir),
            "pipeline mode mismatch publishes nothing");
    }
    return ExpectTrue(
        false,
        "pipeline mode mismatch must fail closed");
}

bool LayerGridMismatchPublishesNothing()
{
    const std::filesystem::path directory =
        MakeTestDirectory("grid_mismatch");
    const std::filesystem::path packageDir = directory / "package";
    const slicer_core::MultiModelScene scene = MakeScene();
    slicer_core::SceneLayerComposeResult composition =
        MakeComposition(scene);
    ++composition.grid.widthpx;
    try
    {
        (void)slicer_core::WriteMultiModelSceneProductionPackage(
            MakeRequest(packageDir),
            std::move(composition),
            scene,
            MakeAdmission(scene));
    }
    catch (const std::exception&)
    {
        return ExpectTrue(
            !std::filesystem::exists(packageDir),
            "layer/grid mismatch publishes nothing");
    }
    return ExpectTrue(
        false,
        "layer/grid mismatch must fail closed");
}

bool IdenticalSceneReplacementIsDeterministic()
{
    const std::filesystem::path directory =
        MakeTestDirectory("replacement");
    const std::filesystem::path packageDir = directory / "package";
    const slicer_core::MultiModelScene scene = MakeScene();
    const slicer_core::SceneCollisionResult admission =
        MakeAdmission(scene);

    (void)slicer_core::WriteMultiModelSceneProductionPackage(
        MakeRequest(packageDir),
        MakeComposition(scene),
        scene,
        admission);
    const std::string firstManifest =
        ReadJson(packageDir / "manifest.json").dump(0);
    const std::string firstReport =
        ReadJson(
            packageDir
            / slicer_core::MultiModelSceneReportRelativePath())
            .dump(0);

    const auto second =
        slicer_core::WriteMultiModelSceneProductionPackage(
            MakeRequest(packageDir),
            MakeComposition(scene),
            scene,
            admission);
    return ExpectTrue(
               second.replacedPackageBackupDir.empty(),
               "successful replacement cleans its backup")
        && ExpectTrue(
            firstManifest
                == ReadJson(packageDir / "manifest.json").dump(0),
            "identical scene manifest is deterministic")
        && ExpectTrue(
            firstReport
                == ReadJson(
                       packageDir
                       / slicer_core::MultiModelSceneReportRelativePath())
                       .dump(0),
            "identical scene report is deterministic");
}

}  // namespace

int main()
{
    bool passed{true};
    passed = ScenePackageContainsOneGlobalTiffPerLayer() && passed;
    passed = SingleInstanceSceneUsesTheSamePackageContract() && passed;
    passed = LegacyWriterWithoutSceneExtensionStaysCompatible() && passed;
    passed = BlockedCompositionPublishesNothing() && passed;
    passed = SceneIdentityMismatchPreservesExistingPackage() && passed;
    passed = InvalidTypedSceneExtensionPublishesNothing() && passed;
    passed = MutatedLayerProtocolPublishesNothing() && passed;
    passed = StaleTransformEvidencePublishesNothing() && passed;
    passed = ContradictoryAdmissionPublishesNothing() && passed;
    passed = CompositionStatisticsMismatchPublishesNothing() && passed;
    passed = SceneBuildVolumeMismatchPublishesNothing() && passed;
    passed = FixtureCannotClaimProductionAdmission() && passed;
    passed = SceneReportPathMutationPublishesNothing() && passed;
    passed = PipelineModeMismatchPublishesNothing() && passed;
    passed = LayerGridMismatchPublishesNothing() && passed;
    passed = IdenticalSceneReplacementIsDeterministic() && passed;
    if (passed)
    {
        std::cout << "multi model package writer unit tests passed\n";
        return 0;
    }
    return 1;
}
