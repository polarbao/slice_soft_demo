#include "slicer_core/config.h"
#include "slicer_core/layout/GridLayoutPolicy.h"
#include "slicer_core/layout/SceneCollisionService.h"
#include "slicer_core/model.h"
#include "slicer_core/pipeline/LegacySceneLayerAdapter.h"
#include "slicer_core/pipeline/MultiModelScenePackageWriter.h"
#include "slicer_core/pipeline/MultiModelSliceOrchestrator.h"
#include "slicer_core/pipeline/TranslatedSceneRasterReuse.h"
#include "slicer_core/reports/MultiModelSceneMatrixReport.h"
#include "slicer_core/reports/ReportWriter.h"
#include "slicer_core/rip_reader.h"
#include "slicer_core/scene/SceneResourceIdentity.h"
#include "slicer_core/scene/SceneViewGeometry.h"
#include "slicer_core/system/Sha256.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <Windows.h>
#include <Psapi.h>
#endif

namespace
{

constexpr int kFunctionalDpi{127};
constexpr double kFunctionalPitchMm{0.2};
constexpr double kFunctionalLayerThicknessMm{0.2};
constexpr double kRasterQuantizationTolerance{0.500001};

using Clock = std::chrono::steady_clock;

struct CliOptions
{
    std::filesystem::path sourceroot{SLICESOFT_SOURCE_DIR};
    std::filesystem::path outputdir{
        std::filesystem::path(SLICESOFT_SOURCE_DIR)
        / "output/benchmarks/13b_07"};
    std::optional<std::string> caseid;
    std::string samplingstrategy{"legacy_center_sample"};
    bool positiveonly{false};
};

struct AssetSpec
{
    std::string key;
    std::string modelid;
    std::filesystem::path configrelativepath;
};

struct LoadedAsset
{
    AssetSpec spec;
    std::filesystem::path configpath;
    slicer_core::SliceConfig config;
    slicer_core::SceneModel model;
    std::string sourcehash;
    std::string resourcehash;
};

struct PositiveCasePlan
{
    std::string caseid;
    std::vector<std::string> assetkeys;
};

struct RasterPrototype
{
    slicer_core::SceneInstanceRaster raster;
    slicer_core::ModelInstance instance;
};

double ElapsedMs(const Clock::time_point start)
{
    return std::chrono::duration<double, std::milli>(
               Clock::now() - start)
        .count();
}

std::string ReadFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        throw std::runtime_error(
            "failed to read asset: " + path.generic_string());
    }
    return {
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()};
}

std::uint64_t PeakWorkingSetBytes()
{
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS counters{};
    counters.cb = sizeof(counters);
    if (GetProcessMemoryInfo(
            GetCurrentProcess(),
            &counters,
            sizeof(counters))
        == 0)
    {
        return 0U;
    }
    return static_cast<std::uint64_t>(
        counters.PeakWorkingSetSize);
#else
    return 0U;
#endif
}

std::uint64_t DirectoryBytes(
    const std::filesystem::path& directory)
{
    std::uint64_t bytes{0U};
    if (!std::filesystem::exists(directory))
    {
        return bytes;
    }
    for (const std::filesystem::directory_entry& entry :
         std::filesystem::recursive_directory_iterator(directory))
    {
        if (entry.is_regular_file())
        {
            bytes += static_cast<std::uint64_t>(
                entry.file_size());
        }
    }
    return bytes;
}

std::string CompilerName()
{
#ifdef _MSC_VER
    return "MSVC " + std::to_string(_MSC_VER);
#elif defined(__clang__)
    return "Clang";
#elif defined(__GNUC__)
    return "GCC";
#else
    return "unknown";
#endif
}

CliOptions ParseOptions(const int argc, char** argv)
{
    CliOptions options;
    for (int index{1}; index < argc; ++index)
    {
        const std::string argument = argv[index];
        if (argument == "--source-root" && index + 1 < argc)
        {
            options.sourceroot = argv[++index];
        }
        else if (argument == "--output" && index + 1 < argc)
        {
            options.outputdir = argv[++index];
        }
        else if (argument == "--case-id" && index + 1 < argc)
        {
            options.caseid = argv[++index];
        }
        else if (argument == "--sampling-strategy" && index + 1 < argc)
        {
            options.samplingstrategy = argv[++index];
        }
        else if (argument == "--positive-only")
        {
            options.positiveonly = true;
        }
        else if (argument == "--help")
        {
            std::cout
                << "Usage: multi_model_scene_matrix "
                   "[--source-root <path>] [--output <path>] "
                   "[--case-id <13B-M01|13B-M11|13B-M12|13B-M22|13B-M3F>] "
                   "[--sampling-strategy <strategy>] [--positive-only]\n";
            std::exit(0);
        }
        else
        {
            throw std::invalid_argument(
                "unsupported argument: " + argument);
        }
    }
    options.sourceroot =
        std::filesystem::absolute(options.sourceroot)
            .lexically_normal();
    options.outputdir =
        std::filesystem::absolute(options.outputdir)
            .lexically_normal();
    const std::set<std::string> supportedStrategies{
        "legacy_center_sample",
        "layer_slab_pixel_center_candidate",
        "layer_slab_supersample_2x2_at_least_two_candidate",
        "layer_slab_supersample_2x2_any_hit_candidate",
    };
    if (!supportedStrategies.contains(options.samplingstrategy))
    {
        throw std::invalid_argument(
            "unsupported geometry sampling strategy: "
            + options.samplingstrategy);
    }
    return options;
}

std::vector<AssetSpec> AssetCatalog()
{
    return {
        {
            "xiao_ma",
            "model-xiao-ma",
            "samples/configs/scene/13b_07_xiao_ma_legacy.json",
        },
        {
            "yecan",
            "model-yecan",
            "samples/configs/scene/13b_07_yecan_legacy.json",
        },
        {
            "texture2d_3mf",
            "model-texture2d-3mf",
            "samples/configs/scene/13b_07_texture2d_3mf_legacy.json",
        },
    };
}

const AssetSpec& FindAssetSpec(
    const std::vector<AssetSpec>& catalog,
    const std::string& key)
{
    const auto found = std::find_if(
        catalog.begin(),
        catalog.end(),
        [&key](const AssetSpec& asset)
        {
            return asset.key == key;
        });
    if (found == catalog.end())
    {
        throw std::invalid_argument(
            "unknown matrix asset key: " + key);
    }
    return *found;
}

std::filesystem::path PrepareStrategyConfig(
    const CliOptions& options,
    const AssetSpec& spec)
{
    const std::filesystem::path sourceConfigPath{
        (options.sourceroot / spec.configrelativepath)
            .lexically_normal()};
    std::ifstream input(sourceConfigPath, std::ios::binary);
    if (!input)
    {
        throw std::runtime_error(
            "failed to read matrix config: "
            + sourceConfigPath.generic_string());
    }
    slicer_core::Json::Object root{
        slicer_core::Json::parse(input).as_object()};
    slicer_core::Json::Object modelInput{
        root.at("input").as_object()};
    const std::filesystem::path configuredModelPath{
        modelInput.at("modelPath").as_string()};
    const std::filesystem::path resolvedModelPath{
        std::filesystem::absolute(
            sourceConfigPath.parent_path()
            / configuredModelPath)
            .lexically_normal()};
    modelInput["modelPath"] =
        resolvedModelPath.generic_string();
    root["input"] = slicer_core::Json{std::move(modelInput)};
    root["geometrySampling"] = slicer_core::Json::object({
        {"strategy", options.samplingstrategy},
    });

    const std::filesystem::path generatedConfigPath{
        options.outputdir / "effective_configs"
        / (spec.key + ".json")};
    std::filesystem::create_directories(
        generatedConfigPath.parent_path());
    slicer_core::WriteReportJsonFile(
        generatedConfigPath,
        slicer_core::Json{std::move(root)});
    return generatedConfigPath;
}

LoadedAsset LoadAsset(
    const CliOptions& options,
    const AssetSpec& spec)
{
    LoadedAsset asset;
    asset.spec = spec;
    asset.configpath = PrepareStrategyConfig(options, spec);
    asset.config =
        slicer_core::load_slice_config(asset.configpath);
    asset.model =
        slicer_core::load_model_report(
            asset.config,
            asset.configpath.parent_path());
    if (asset.model.triangles.empty()
        || asset.model.model_path.empty())
    {
        throw std::runtime_error(
            "matrix asset importer returned no geometry");
    }
    asset.sourcehash = slicer_core::ComputeSha256(
        ReadFile(asset.model.model_path));
    asset.resourcehash =
        slicer_core::ComputeSceneResourceHash(asset.model);
    return asset;
}

std::map<std::string, LoadedAsset> LoadCaseAssets(
    const CliOptions& options,
    const PositiveCasePlan& plan)
{
    const std::vector<AssetSpec> catalog = AssetCatalog();
    std::map<std::string, LoadedAsset> assets;
    for (const std::string& key : plan.assetkeys)
    {
        if (!assets.contains(key))
        {
            assets.emplace(
                key,
                LoadAsset(options, FindAssetSpec(catalog, key)));
        }
    }
    return assets;
}

slicer_core::ResourceScope MakeResourceScope(
    const LoadedAsset& asset)
{
    slicer_core::ResourceScope scope;
    scope.resourcescopeid = "scope-" + asset.spec.key;
    const std::filesystem::path sourcePath =
        std::filesystem::absolute(asset.model.model_path)
            .lexically_normal();
    if (asset.model.format == "3mf")
    {
        scope.kind =
            slicer_core::ResourceScopeKind::ThreeMfPackage;
        scope.rootpath = sourcePath;
        scope.packagepath = sourcePath;
        scope.partidentity =
            asset.model.three_mf.model_part_path.empty()
            ? "3D/3dmodel.model"
            : asset.model.three_mf.model_part_path;
    }
    else
    {
        scope.kind =
            slicer_core::ResourceScopeKind::ObjDirectory;
        scope.rootpath = sourcePath.parent_path();
    }
    return scope;
}

slicer_core::ModelSource MakeModelSource(
    const LoadedAsset& asset)
{
    slicer_core::ModelSource source;
    source.modelid = asset.spec.modelid;
    source.sourcepath =
        std::filesystem::absolute(asset.model.model_path)
            .lexically_normal();
    source.format = asset.model.format;
    source.resourcescopeid = "scope-" + asset.spec.key;
    source.sourcehash = asset.sourcehash;
    source.resourcehash = asset.resourcehash;
    source.displayname = asset.spec.key;
    return source;
}

std::string InstanceId(
    const std::string& caseId,
    const std::size_t index)
{
    std::ostringstream output;
    output << caseId << "-instance-"
           << std::setfill('0') << std::setw(2)
           << (index + 1U);
    return output.str();
}

slicer_core::MultiModelScene BuildLaidOutScene(
    const PositiveCasePlan& plan,
    const std::map<std::string, LoadedAsset>& assets,
    double& layoutMs)
{
    slicer_core::MultiModelScene scene;
    scene.sceneid = "scene-" + plan.caseid;
    scene.scenerevision = 1U;
    scene.resolvedprofileid =
        "13b_07_functional_127dpi";

    std::set<std::string> addedAssets;
    for (const std::string& key : plan.assetkeys)
    {
        if (addedAssets.insert(key).second)
        {
            const LoadedAsset& asset = assets.at(key);
            scene.resourcescopes.push_back(
                MakeResourceScope(asset));
            scene.models.push_back(MakeModelSource(asset));
        }
    }

    slicer_core::GridLayoutRequest layoutRequest;
    layoutRequest.layout = scene.layout;
    layoutRequest.currentscenerevision =
        scene.scenerevision;
    layoutRequest.expectedscenerevision =
        scene.scenerevision;
    for (std::size_t index{0U};
         index < plan.assetkeys.size();
         ++index)
    {
        const LoadedAsset& asset =
            assets.at(plan.assetkeys.at(index));
        slicer_core::SceneModelInstance sceneInstance;
        sceneInstance.instance.instanceid =
            InstanceId(plan.caseid, index);
        sceneInstance.instance.modelid =
            asset.spec.modelid;
        sceneInstance.instance.sourcetransformidentity =
            asset.model.model_path.generic_string();
        sceneInstance.instance.sourcebboxmm =
            asset.model.bbox_mm;
        sceneInstance.instance.effectivebboxmm =
            asset.model.bbox_mm;
        sceneInstance.resolvedprofileid =
            scene.resolvedprofileid;
        scene.instances.push_back(sceneInstance);

        slicer_core::GridLayoutItem item;
        item.instance = sceneInstance.instance;
        item.requestedtransform =
            sceneInstance.requestedtransform;
        item.currentderivedlayouttransform =
            sceneInstance.derivedlayouttransform;
        layoutRequest.items.push_back(std::move(item));
    }

    const Clock::time_point layoutStart = Clock::now();
    const slicer_core::GridLayoutResult layout =
        slicer_core::ComputeGridLayout(layoutRequest);
    layoutMs = ElapsedMs(layoutStart);
    if (!layout.IsValid()
        || layout.placements.size() != scene.instances.size())
    {
        throw std::runtime_error(
            layout.error.has_value()
            ? layout.error->message
            : "grid layout returned an incomplete result");
    }

    scene.scenerevision = layout.derivedscenerevision;
    for (std::size_t index{0U};
         index < scene.instances.size();
         ++index)
    {
        slicer_core::SceneModelInstance& item =
            scene.instances.at(index);
        const slicer_core::GridLayoutPlacement& placement =
            layout.placements.at(index);
        const bool changed =
            !slicer_core::ModelTransformsEquivalent(
                item.instance.transform,
                placement.effectivetransform);
        item.requestedtransform =
            placement.requestedtransform;
        item.derivedlayouttransform =
            placement.derivedlayouttransform;
        item.effectivetransform =
            placement.effectivetransform;
        item.instance.transform =
            placement.effectivetransform;
        item.instance.effectivebboxmm =
            placement.effectivebboxmm;
        if (changed)
        {
            ++item.instance.transformrevision;
        }
        item.admissionstatus =
            slicer_core::SceneInstanceAdmissionStatus::Admitted;
    }

    scene.buildvolume.source =
        slicer_core::BuildVolumeSource::Fixture;
    scene.buildvolume.widthmm =
        (std::max)(1.0, layout.boundsmm.max.x + 1.0);
    scene.buildvolume.heightmm =
        (std::max)(1.0, layout.boundsmm.max.y + 1.0);
    scene.buildvolume.origin =
        slicer_core::BuildVolumeOrigin::LowerLeft;
    scene.buildvolume.xdirection =
        slicer_core::BuildVolumeAxisDirection::Positive;
    scene.buildvolume.ydirection =
        slicer_core::BuildVolumeAxisDirection::Positive;
    scene.buildvolume.isfixture = true;

    const slicer_core::SceneValidationResult validation =
        slicer_core::ValidateMultiModelScene(
            scene,
            slicer_core::SceneValidationPurpose::
                FunctionalFixture);
    if (!validation.IsValid())
    {
        throw std::runtime_error(
            validation.errors.front().message);
    }
    return scene;
}

const LoadedAsset& FindAssetForModel(
    const std::map<std::string, LoadedAsset>& assets,
    const std::string& modelId)
{
    for (const auto& [key, asset] : assets)
    {
        (void)key;
        if (asset.spec.modelid == modelId)
        {
            return asset;
        }
    }
    throw std::invalid_argument(
        "scene instance references an unloaded model");
}

slicer_core::SceneCollisionResult AdmitScene(
    const slicer_core::MultiModelScene& scene,
    const std::map<std::string, LoadedAsset>& assets)
{
    slicer_core::SceneCollisionRequest request;
    request.sceneid = scene.sceneid;
    request.currentscenerevision = scene.scenerevision;
    request.expectedscenerevision = scene.scenerevision;
    request.purpose =
        slicer_core::SceneValidationPurpose::FunctionalFixture;
    request.buildvolume = scene.buildvolume;
    request.contactepsilonmm = 0.0;
    request.items.reserve(scene.instances.size());

    for (const slicer_core::SceneModelInstance& item :
         scene.instances)
    {
        const LoadedAsset& asset =
            FindAssetForModel(assets, item.instance.modelid);
        slicer_core::SceneViewGeometryRequest geometryRequest;
        geometryRequest.sceneid = scene.sceneid;
        geometryRequest.scenerevision = scene.scenerevision;
        geometryRequest.expectedscenerevision =
            scene.scenerevision;
        geometryRequest.expectedtransformrevision =
            item.instance.transformrevision;
        geometryRequest.instance = item.instance;
        geometryRequest.textureoptions = {
            asset.config.texture.sampler,
            asset.config.texture.uv_address_mode,
            asset.config.texture.flip_v,
        };
        geometryRequest.admissionstatus =
            slicer_core::SceneViewAdmissionStatus::Admitted;
        slicer_core::SceneViewGeometryResult geometry =
            slicer_core::BuildSceneViewGeometry(
                asset.model,
                geometryRequest);
        if (!geometry.IsValid())
        {
            throw std::runtime_error(
                geometry.error->message);
        }

        slicer_core::SceneCollisionItem collisionItem;
        collisionItem.instance = item.instance;
        collisionItem.admissionstatus =
            slicer_core::SceneInstanceAdmissionStatus::Admitted;
        collisionItem.geometry =
            std::move(geometry.geometry);
        request.items.push_back(std::move(collisionItem));
    }

    slicer_core::SceneCollisionResult admission =
        slicer_core::EvaluateSceneCollisionAdmission(request);
    if (!admission.IsValid())
    {
        throw std::runtime_error(
            admission.errors.empty()
            ? "scene collision admission failed"
            : admission.errors.front().message);
    }
    return admission;
}

const slicer_core::SceneCollisionInstanceResult&
FindAdmissionInstance(
    const slicer_core::SceneCollisionResult& admission,
    const std::string& instanceId)
{
    const auto found = std::find_if(
        admission.instances.begin(),
        admission.instances.end(),
        [&instanceId](
            const slicer_core::SceneCollisionInstanceResult& item)
        {
            return item.instanceid == instanceId;
        });
    if (found == admission.instances.end())
    {
        throw std::runtime_error(
            "instance admission evidence is missing");
    }
    return *found;
}

slicer_core::SceneRasterIdentity MakeRasterIdentity(
    const slicer_core::MultiModelScene& scene,
    const slicer_core::SceneModelInstance& instance,
    const slicer_core::SceneCollisionResult& admission)
{
    const slicer_core::SceneCollisionInstanceResult& evidence =
        FindAdmissionInstance(
            admission,
            instance.instance.instanceid);
    slicer_core::SceneRasterIdentity identity;
    identity.sceneid = scene.sceneid;
    identity.modelid = instance.instance.modelid;
    identity.instanceid = instance.instance.instanceid;
    identity.scenerevision = scene.scenerevision;
    identity.transformrevision =
        instance.instance.transformrevision;
    identity.admittedtransformrevision =
        evidence.transformrevision;
    identity.transformhash = evidence.transformhash;
    identity.admittedtransformhash =
        evidence.transformhash;
    identity.visible = instance.instance.visible;
    identity.admitted =
        evidence.admissionstatus
        == slicer_core::SceneInstanceAdmissionStatus::Admitted;
    identity.effectivepipelinemode =
        slicer_core::SlicePipelineMode::Legacy;
    return identity;
}

std::vector<slicer_core::SceneInstanceRaster>
ProduceSceneRasters(
    const slicer_core::MultiModelScene& scene,
    const slicer_core::SceneCollisionResult& admission,
    const std::map<std::string, LoadedAsset>& assets,
    int& producerCount,
    int& reusedCount)
{
    std::map<std::string, RasterPrototype> prototypes;
    std::vector<slicer_core::SceneInstanceRaster> rasters;
    rasters.reserve(scene.instances.size());

    for (const slicer_core::SceneModelInstance& item :
         scene.instances)
    {
        const LoadedAsset& asset =
            FindAssetForModel(assets, item.instance.modelid);
        const slicer_core::SceneRasterIdentity identity =
            MakeRasterIdentity(scene, item, admission);
        const auto prototype = prototypes.find(asset.spec.key);
        if (prototype == prototypes.end())
        {
            slicer_core::LegacySceneLayerAdapterRequest request;
            request.configpath = asset.configpath;
            request.identity = identity;
            request.instance = item.instance;
            slicer_core::SceneRasterAdapterResult adapted =
                slicer_core::AdaptLegacySceneLayers(request);
            if (!adapted.IsValid())
            {
                throw std::runtime_error(
                    adapted.error.has_value()
                    ? adapted.error->message
                    : "Legacy scene adapter failed");
            }
            ++producerCount;
            RasterPrototype stored;
            stored.raster = std::move(adapted.raster);
            stored.instance = item.instance;
            rasters.push_back(stored.raster);
            prototypes.emplace(
                asset.spec.key,
                std::move(stored));
        }
        else
        {
            slicer_core::TranslatedSceneRasterReuseRequest request;
            request.prototype = &prototype->second.raster;
            request.prototypeinstance =
                prototype->second.instance;
            request.targetidentity = identity;
            request.targetinstance = item.instance;
            request.quantizationtolerance =
                kRasterQuantizationTolerance;
            slicer_core::SceneRasterAdapterResult reused =
                slicer_core::ReuseTranslatedSceneRaster(request);
            if (!reused.IsValid())
            {
                throw std::runtime_error(
                    reused.error.has_value()
                    ? reused.error->message
                    : "translated scene raster reuse failed");
            }
            ++reusedCount;
            rasters.push_back(std::move(reused.raster));
        }
    }
    return rasters;
}

std::vector<std::string> UniqueFormats(
    const std::map<std::string, LoadedAsset>& assets)
{
    std::set<std::string> values;
    for (const auto& [key, asset] : assets)
    {
        (void)key;
        values.insert(asset.model.format);
    }
    return {values.begin(), values.end()};
}

std::vector<std::string> ModelIds(
    const slicer_core::MultiModelScene& scene)
{
    std::vector<std::string> values;
    values.reserve(scene.models.size());
    for (const slicer_core::ModelSource& model : scene.models)
    {
        values.push_back(model.modelid);
    }
    return values;
}

slicer_core::MultiModelSceneMatrixCase RunPositiveCase(
    const CliOptions& options,
    const PositiveCasePlan& plan)
{
    slicer_core::MultiModelSceneMatrixCase result;
    result.caseid = plan.caseid;
    result.category = "positive";
    result.status = "blocked";
    result.expectedpass = true;
    result.instancecount =
        static_cast<int>(plan.assetkeys.size());
    const Clock::time_point totalStart = Clock::now();

    const Clock::time_point importStart = Clock::now();
    const std::map<std::string, LoadedAsset> assets =
        LoadCaseAssets(options, plan);
    result.timing.importms = ElapsedMs(importStart);
    result.uniquemodelcount =
        static_cast<int>(assets.size());
    result.formats = UniqueFormats(assets);

    slicer_core::MultiModelScene scene =
        BuildLaidOutScene(
            plan,
            assets,
            result.timing.layoutms);
    result.modelids = ModelIds(scene);

    const Clock::time_point admissionStart = Clock::now();
    const slicer_core::SceneCollisionResult admission =
        AdmitScene(scene, assets);
    result.timing.preflightadmissionms =
        ElapsedMs(admissionStart);

    const Clock::time_point sliceStart = Clock::now();
    std::vector<slicer_core::SceneInstanceRaster> rasters =
        ProduceSceneRasters(
            scene,
            admission,
            assets,
            result.sliceproducerinvocationcount,
            result.reusedinstancecount);
    result.timing.slicems = ElapsedMs(sliceStart);

    const Clock::time_point composeStart = Clock::now();
    slicer_core::MultiModelLayerComposeRequest composeRequest;
    composeRequest.admission = admission;
    composeRequest.currentscenerevision =
        scene.scenerevision;
    composeRequest.effectivepipelinemode =
        slicer_core::SlicePipelineMode::Legacy;
    composeRequest.instances = std::move(rasters);
    composeRequest.quantizationtolerance =
        kRasterQuantizationTolerance;
    slicer_core::SceneLayerComposeResult composition =
        slicer_core::ComposeAdmittedSceneRasters(
            composeRequest);
    result.timing.composems = ElapsedMs(composeStart);
    if (!composition.IsValid())
    {
        throw std::runtime_error(
            composition.error.has_value()
            ? composition.error->message
            : "scene layer composition failed");
    }
    result.widthpx = composition.grid.widthpx;
    result.heightpx = composition.grid.heightpx;
    result.layercount = composition.grid.layercount;

    result.packagedir =
        options.outputdir / plan.caseid / "package";
    std::filesystem::create_directories(
        result.packagedir.parent_path());
    slicer_core::RgbwsvProductionPackageWriteRequest writeRequest;
    writeRequest.packageDir = result.packagedir;
    writeRequest.sourceConfigPath =
        options.sourceroot
        / "docs/slice/DOC/DOC_PREP_13B_07_真实模型矩阵与阶段收口准备.md";
    writeRequest.sourceModelPath =
        options.sourceroot / "samples/configs/scene";
    writeRequest.sourceFormat = "scene";
    writeRequest.requestedPipelineMode = "legacy";
    writeRequest.effectivePipelineMode = "legacy";
    writeRequest.productionAcceptance =
        "functional_fixture_admitted";
    writeRequest.grid.dpiX = kFunctionalDpi;
    writeRequest.grid.dpiY = kFunctionalDpi;
    writeRequest.grid.pixelSizeXmm =
        kFunctionalPitchMm;
    writeRequest.grid.pixelSizeYmm =
        kFunctionalPitchMm;
    writeRequest.grid.layerThicknessMm =
        kFunctionalLayerThicknessMm;
    writeRequest.preview.enabled = false;
    writeRequest.storage.storageMode = "stripped";
    writeRequest.storage.rowsPerStrip = 64;

    const Clock::time_point writerStart = Clock::now();
    const slicer_core::RgbwsvProductionPackageWriteResult written =
        slicer_core::WriteMultiModelSceneProductionPackage(
            std::move(writeRequest),
            std::move(composition),
            scene,
            admission);
    result.timing.tiffandreportwritems =
        ElapsedMs(writerStart);
    result.packagewritten =
        written.productionOutputWritten;

    const Clock::time_point ripStart = Clock::now();
    const slicer_core::RipValidationResult rip =
        slicer_core::validate_slice_package(
            result.packagedir);
    result.timing.ripvalidationms =
        ElapsedMs(ripStart);
    result.ripstrictpass =
        rip.schema == "p0.rgbwsv.2"
        && rip.bit_depth == 8
        && rip.channel_order
            == std::array<std::string, 6>{
                "R", "G", "B", "W", "S", "V"}
        && rip.width_px == result.widthpx
        && rip.height_px == result.heightpx
        && rip.layer_count == result.layercount
        && rip.dpi_x == kFunctionalDpi
        && rip.dpi_y == kFunctionalDpi
        && rip.warnings_count == 0;
    result.packagebytes =
        DirectoryBytes(result.packagedir);
    result.peakworkingsetbytes =
        PeakWorkingSetBytes();
    result.timing.totalms = ElapsedMs(totalStart);
    result.passed =
        result.packagewritten && result.ripstrictpass;
    result.status = result.passed ? "passed" : "blocked";
    result.message =
        result.passed
        ? "functional fixture package and RIP strict passed"
        : "package or RIP strict validation failed";
    return result;
}

slicer_core::GridLayoutItem MakeNegativeLayoutItem(
    const int index)
{
    slicer_core::GridLayoutItem item;
    item.instance.instanceid =
        "negative-instance-" + std::to_string(index);
    item.instance.modelid = "negative-model";
    item.instance.sourcetransformidentity =
        "negative-source";
    item.instance.sourcebboxmm =
        {{0.0, 0.0, 0.0}, {1.0, 1.0, 1.0}};
    item.instance.effectivebboxmm =
        item.instance.sourcebboxmm;
    return item;
}

slicer_core::SceneCollisionItem MakeNegativeCollisionItem(
    const int index,
    const double minX,
    const double minY,
    const double maxX,
    const double maxY,
    const std::uint64_t sceneRevision)
{
    slicer_core::SceneCollisionItem item;
    item.instance.instanceid =
        "negative-instance-" + std::to_string(index);
    item.instance.modelid =
        "negative-model-" + std::to_string(index);
    item.instance.sourcetransformidentity =
        "negative-source-" + std::to_string(index);
    item.instance.transformrevision = 1U;
    item.instance.sourcebboxmm =
        {{minX, minY, 0.0}, {maxX, maxY, 1.0}};
    item.instance.effectivebboxmm =
        item.instance.sourcebboxmm;
    item.admissionstatus =
        slicer_core::SceneInstanceAdmissionStatus::Admitted;

    const slicer_core::ModelTransformHashResult hash =
        slicer_core::ComputeModelTransformHash(
            item.instance.transform,
            item.instance.sourcetransformidentity,
            item.instance.instanceid,
            item.instance.modelid);
    if (!hash.IsValid())
    {
        throw std::runtime_error(
            "negative fixture transform hash failed");
    }

    slicer_core::SceneViewGeometry geometry;
    geometry.sceneid = "negative-scene";
    geometry.modelid = item.instance.modelid;
    geometry.instanceid = item.instance.instanceid;
    geometry.scenerevision = sceneRevision;
    geometry.transformrevision =
        item.instance.transformrevision;
    geometry.transformhash = hash.hash;
    geometry.worldboundsmm =
        {{minX, minY}, {maxX, maxY}};
    geometry.effectivebboxmm =
        item.instance.effectivebboxmm;
    geometry.visible = true;
    geometry.admissionstatus =
        slicer_core::SceneViewAdmissionStatus::Admitted;
    geometry.triangles = {
        {
            {minX, minY},
            {maxX, minY},
            {maxX, maxY},
            {0.0, 0.0, 0.0},
        },
        {
            {minX, minY},
            {maxX, maxY},
            {minX, maxY},
            {0.0, 0.0, 0.0},
        },
    };
    slicer_core::RefreshSceneViewGeometryHash(geometry);
    item.geometry = std::move(geometry);
    return item;
}

slicer_core::SceneBuildVolume NegativeFixtureVolume()
{
    slicer_core::SceneBuildVolume volume;
    volume.source = slicer_core::BuildVolumeSource::Fixture;
    volume.widthmm = 100.0;
    volume.heightmm = 100.0;
    volume.origin =
        slicer_core::BuildVolumeOrigin::LowerLeft;
    volume.xdirection =
        slicer_core::BuildVolumeAxisDirection::Positive;
    volume.ydirection =
        slicer_core::BuildVolumeAxisDirection::Positive;
    volume.isfixture = true;
    return volume;
}

slicer_core::MultiModelSceneMatrixCase MakeNegativeResult(
    const std::string& caseId,
    const int instanceCount,
    const std::string& errorCode,
    const std::string& message,
    const double totalMs)
{
    slicer_core::MultiModelSceneMatrixCase result;
    result.caseid = caseId;
    result.category = "negative";
    result.status = "blocked";
    result.expectedpass = false;
    result.passed = false;
    result.instancecount = instanceCount;
    result.errorcode = errorCode;
    result.message = message;
    result.timing.totalms = totalMs;
    result.peakworkingsetbytes = PeakWorkingSetBytes();
    return result;
}

std::vector<slicer_core::MultiModelSceneMatrixCase>
RunNegativeCases()
{
    std::vector<slicer_core::MultiModelSceneMatrixCase> results;

    {
        const Clock::time_point start = Clock::now();
        slicer_core::GridLayoutRequest request;
        request.currentscenerevision = 1U;
        request.expectedscenerevision = 1U;
        for (int index{0}; index < 23; ++index)
        {
            request.items.push_back(
                MakeNegativeLayoutItem(index));
        }
        const slicer_core::GridLayoutResult layout =
            slicer_core::ComputeGridLayout(request);
        if (layout.IsValid() || !layout.error.has_value())
        {
            throw std::runtime_error(
                "23-instance negative case did not block");
        }
        results.push_back(MakeNegativeResult(
            "13B-N23",
            23,
            std::string{
                slicer_core::GridLayoutErrorCodeName(
                    layout.error->code)},
            layout.error->message,
            ElapsedMs(start)));
    }

    {
        const Clock::time_point start = Clock::now();
        slicer_core::SceneCollisionRequest request;
        request.sceneid = "negative-scene";
        request.currentscenerevision = 1U;
        request.expectedscenerevision = 1U;
        request.purpose =
            slicer_core::SceneValidationPurpose::
                FunctionalFixture;
        request.buildvolume = NegativeFixtureVolume();
        request.items.push_back(
            MakeNegativeCollisionItem(
                0, 10.0, 10.0, 20.0, 20.0, 1U));
        request.items.push_back(
            MakeNegativeCollisionItem(
                1, 15.0, 15.0, 25.0, 25.0, 1U));
        const slicer_core::SceneCollisionResult admission =
            slicer_core::EvaluateSceneCollisionAdmission(request);
        if (admission.IsValid() || admission.errors.empty())
        {
            throw std::runtime_error(
                "overlap negative case did not block");
        }
        results.push_back(MakeNegativeResult(
            "13B-NOVERLAP",
            2,
            std::string{
                slicer_core::SceneCollisionErrorCodeName(
                    admission.errors.front().code)},
            admission.errors.front().message,
            ElapsedMs(start)));
    }

    {
        const Clock::time_point start = Clock::now();
        slicer_core::SceneCollisionRequest request;
        request.sceneid = "negative-scene";
        request.currentscenerevision = 1U;
        request.expectedscenerevision = 1U;
        request.purpose =
            slicer_core::SceneValidationPurpose::
                FunctionalFixture;
        request.buildvolume = NegativeFixtureVolume();
        request.items.push_back(
            MakeNegativeCollisionItem(
                0, 95.0, 95.0, 105.0, 105.0, 1U));
        const slicer_core::SceneCollisionResult admission =
            slicer_core::EvaluateSceneCollisionAdmission(request);
        if (admission.IsValid() || admission.errors.empty())
        {
            throw std::runtime_error(
                "out-of-bounds negative case did not block");
        }
        results.push_back(MakeNegativeResult(
            "13B-NBOUNDS",
            1,
            std::string{
                slicer_core::SceneCollisionErrorCodeName(
                    admission.errors.front().code)},
            admission.errors.front().message,
            ElapsedMs(start)));
    }

    {
        const Clock::time_point start = Clock::now();
        slicer_core::SceneCollisionRequest request;
        request.sceneid = "negative-scene";
        request.currentscenerevision = 1U;
        request.expectedscenerevision = 1U;
        request.purpose =
            slicer_core::SceneValidationPurpose::
                FunctionalFixture;
        const slicer_core::SceneCollisionResult admission =
            slicer_core::EvaluateSceneCollisionAdmission(request);
        if (admission.IsValid() || admission.errors.empty())
        {
            throw std::runtime_error(
                "undefined-volume negative case did not block");
        }
        results.push_back(MakeNegativeResult(
            "13B-NVOLUME",
            0,
            std::string{
                slicer_core::SceneCollisionErrorCodeName(
                    admission.errors.front().code)},
            admission.errors.front().message,
            ElapsedMs(start)));
    }

    {
        const Clock::time_point start = Clock::now();
        slicer_core::SceneCollisionRequest request;
        request.sceneid = "negative-scene";
        request.currentscenerevision = 2U;
        request.expectedscenerevision = 1U;
        request.purpose =
            slicer_core::SceneValidationPurpose::
                FunctionalFixture;
        request.buildvolume = NegativeFixtureVolume();
        const slicer_core::SceneCollisionResult admission =
            slicer_core::EvaluateSceneCollisionAdmission(request);
        if (admission.IsValid() || admission.errors.empty())
        {
            throw std::runtime_error(
                "stale-revision negative case did not block");
        }
        results.push_back(MakeNegativeResult(
            "13B-NSTALE",
            0,
            std::string{
                slicer_core::SceneCollisionErrorCodeName(
                    admission.errors.front().code)},
            admission.errors.front().message,
            ElapsedMs(start)));
    }
    return results;
}

std::vector<PositiveCasePlan> PositivePlans()
{
    std::vector<PositiveCasePlan> plans;
    plans.push_back({"13B-M01", {"xiao_ma"}});
    plans.push_back(
        {"13B-M11", std::vector<std::string>(11U, "xiao_ma")});

    PositiveCasePlan twelve;
    twelve.caseid = "13B-M12";
    twelve.assetkeys.assign(11U, "xiao_ma");
    twelve.assetkeys.push_back("yecan");
    plans.push_back(std::move(twelve));

    PositiveCasePlan twentyTwo;
    twentyTwo.caseid = "13B-M22";
    twentyTwo.assetkeys.assign(11U, "xiao_ma");
    twentyTwo.assetkeys.insert(
        twentyTwo.assetkeys.end(),
        11U,
        "yecan");
    plans.push_back(std::move(twentyTwo));
    plans.push_back(
        {"13B-M3F", {"xiao_ma", "texture2d_3mf"}});
    return plans;
}

std::vector<PositiveCasePlan> SelectedPositivePlans(
    const CliOptions& options)
{
    std::vector<PositiveCasePlan> plans{PositivePlans()};
    if (!options.caseid.has_value())
    {
        return plans;
    }
    const auto found = std::find_if(
        plans.begin(),
        plans.end(),
        [&options](const PositiveCasePlan& plan)
        {
            return plan.caseid == *options.caseid;
        });
    if (found == plans.end())
    {
        throw std::invalid_argument(
            "unknown positive matrix case: "
            + *options.caseid);
    }
    return {*found};
}

bool ExpectedOutcome(
    const slicer_core::MultiModelSceneMatrixCase& item)
{
    return item.expectedpass ? item.passed : !item.passed;
}

void WriteMarkdown(
    const std::filesystem::path& path,
    const std::string& content)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    if (!output)
    {
        throw std::runtime_error(
            "failed to open matrix Markdown output");
    }
    output << content;
    output.flush();
    if (!output)
    {
        throw std::runtime_error(
            "failed to write matrix Markdown output");
    }
}

int Run(const CliOptions& options)
{
    std::filesystem::create_directories(
        options.outputdir.parent_path());
    std::filesystem::create_directories(options.outputdir);
    slicer_core::MultiModelSceneMatrixReport report;
    report.buildconfig = SLICESOFT_BUILD_CONFIG;
    report.compiler = CompilerName();
    report.productionblockers = {
        "device_build_volume_open",
        "device_origin_and_xy_axes_open",
        "22_instance_performance_budget_open",
    };
    report.knowncoveragegaps = {
        "aishen_meigui_titian_complex_relief_strict_pass_0_of_3",
        "global_openvdb_multimodel_matrix_not_run_in_default_build",
        "functional_127dpi_metrics_are_not_device_production_sla",
    };

    for (const PositiveCasePlan& plan :
         SelectedPositivePlans(options))
    {
        try
        {
            report.cases.push_back(
                RunPositiveCase(options, plan));
        }
        catch (const std::exception& error)
        {
            slicer_core::MultiModelSceneMatrixCase failed;
            failed.caseid = plan.caseid;
            failed.category = "positive";
            failed.status = "blocked";
            failed.expectedpass = true;
            failed.passed = false;
            failed.instancecount =
                static_cast<int>(plan.assetkeys.size());
            failed.errorcode = "MATRIX_CASE_FAILED";
            failed.message = error.what();
            failed.peakworkingsetbytes =
                PeakWorkingSetBytes();
            report.cases.push_back(std::move(failed));
        }
    }

    if (!options.positiveonly)
    {
        const auto negativeCases = RunNegativeCases();
        report.cases.insert(
            report.cases.end(),
            negativeCases.begin(),
            negativeCases.end());
    }
    report.functionalmatrixpass = std::all_of(
        report.cases.begin(),
        report.cases.end(),
        ExpectedOutcome);
    report.status =
        report.functionalmatrixpass ? "passed" : "blocked";

    const slicer_core::Json document =
        slicer_core::SerializeMultiModelSceneMatrixReport(report);
    slicer_core::WriteReportJsonFile(
        options.outputdir / "real_model_matrix.json",
        document);
    WriteMarkdown(
        options.outputdir / "real_model_matrix.md",
        slicer_core::RenderMultiModelSceneMatrixMarkdown(report));

    std::cout << "13B-07 functionalMatrixPass="
              << (report.functionalmatrixpass ? "true" : "false")
              << " productionStatus=INPUT_OPEN\n";
    for (const slicer_core::MultiModelSceneMatrixCase& item :
         report.cases)
    {
        std::cout << "  " << item.caseid
                  << " status=" << item.status
                  << " instances=" << item.instancecount
                  << " totalMs=" << item.timing.totalms;
        if (!item.errorcode.empty())
        {
            std::cout << " error=" << item.errorcode;
        }
        std::cout << '\n';
    }
    std::cout << "report="
              << (options.outputdir / "real_model_matrix.json")
                     .generic_string()
              << '\n';
    return report.functionalmatrixpass ? 0 : 1;
}

}  // namespace

int main(const int argc, char** argv)
{
    try
    {
        return Run(ParseOptions(argc, argv));
    }
    catch (const std::exception& error)
    {
        std::cerr << "multi_model_scene_matrix error: "
                  << error.what() << '\n';
        return 1;
    }
}
