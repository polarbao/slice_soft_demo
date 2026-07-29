#include "slicer_core/pipeline/MultiModelProductionService.h"

#include "slicer_core/config.h"
#include "slicer_core/layout/SceneCollisionService.h"
#include "slicer_core/materials/varnish_geometry/OuterVarnishDiscretization.h"
#include "slicer_core/model.h"
#include "slicer_core/pipeline/LegacySceneLayerAdapter.h"
#include "slicer_core/pipeline/MultiModelScenePackageWriter.h"
#include "slicer_core/pipeline/MultiModelSliceOrchestrator.h"
#include "slicer_core/scene/SceneEffectiveConfig.h"
#include "slicer_core/scene/SceneResourceIdentity.h"
#include "slicer_core/scene/SceneViewGeometry.h"
#include "slicer_core/system/Sha256.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace slicer_core
{
namespace
{

constexpr double kNumericTolerance{1.0e-9};
constexpr double kRasterQuantizationTolerance{0.500001};
using ProductionClock = std::chrono::steady_clock;

double ElapsedMilliseconds(
    const ProductionClock::time_point& start)
{
    return std::chrono::duration<double, std::milli>(
               ProductionClock::now() - start)
        .count();
}

void ReportProgress(
    const MultiModelProductionRequest& request,
    const ProductionClock::time_point& runStart,
    const std::string& phase,
    const int current,
    const int total,
    const int percent)
{
    if (!request.progresscallback)
    {
        return;
    }
    request.progresscallback(
        SliceRunProgress{
            phase,
            current,
            total,
            std::clamp(percent, 0, 100),
            ElapsedMilliseconds(runStart)});
}

struct SceneProductionContract
{
    std::filesystem::path profileconfigpath;
    std::filesystem::path outputpackagedir;
    std::filesystem::path scenebasedirectory;
    int dpix{0};
    int dpiy{0};
    double layerheightmm{0.0};
    std::string slicepipelinemode;
    bool production{false};
};

struct LoadedSceneModel
{
    ModelSource source;
    SceneModel model;
};

MultiModelProductionResult Block(
    const MultiModelProductionRequest& request,
    const MultiModelProductionErrorCode code,
    const std::string& field,
    const std::string& message,
    const std::string& sceneId = {},
    const std::string& modelId = {},
    const std::string& instanceId = {})
{
    MultiModelProductionResult result;
    MultiModelProductionError error;
    error.code = code;
    error.sceneid = sceneId;
    error.modelid = modelId;
    error.instanceid = instanceId;
    error.field = field.empty()
        ? request.effectiveconfigpath.generic_string()
        : field;
    error.message = message;
    result.sceneid = sceneId;
    result.error = std::move(error);
    return result;
}

std::filesystem::path ResolvePath(
    const std::filesystem::path& path,
    const std::filesystem::path& baseDirectory)
{
    if (path.empty())
    {
        return {};
    }
    const std::filesystem::path resolved =
        path.is_absolute() ? path : baseDirectory / path;
    std::error_code error;
    const std::filesystem::path absolute =
        std::filesystem::absolute(resolved, error);
    return (error ? resolved : absolute).lexically_normal();
}

std::string ReadFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        throw std::runtime_error(
            "failed to read scene resource: "
            + path.generic_string());
    }
    return {
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()};
}

std::optional<MultiModelProductionErrorCode>
ValidateSceneIdentity(
    const Json& document,
    const MultiModelScene& scene)
{
    if (IsSceneEffectiveConfigStale(document, scene))
    {
        return MultiModelProductionErrorCode::
            EffectiveConfigStale;
    }
    return std::nullopt;
}

SceneProductionContract ReadContract(
    const Json& document,
    const std::filesystem::path& effectiveConfigPath)
{
    const Json& contract = document.at("sliceContract");
    const std::filesystem::path baseDirectory =
        effectiveConfigPath.parent_path();

    SceneProductionContract result;
    result.profileconfigpath = ResolvePath(
        contract.at("profileConfigPath").as_string(),
        baseDirectory);
    result.outputpackagedir = ResolvePath(
        contract.at("outputPackageDir").as_string(),
        baseDirectory);
    result.scenebasedirectory = ResolvePath(
        document.at("sourceScenePath").as_string(),
        baseDirectory)
        .parent_path();
    result.dpix = static_cast<int>(
        contract.at("dpiX").as_double());
    result.dpiy = static_cast<int>(
        contract.at("dpiY").as_double());
    result.layerheightmm =
        contract.at("layerHeightMm").as_double();
    result.slicepipelinemode =
        contract.at("slicePipelineMode").as_string();
    result.production = document.at("production").as_bool();
    return result;
}

const ModelSource* FindModelSource(
    const MultiModelScene& scene,
    const std::string& modelId)
{
    const auto found = std::find_if(
        scene.models.begin(),
        scene.models.end(),
        [&modelId](const ModelSource& source)
        {
            return source.modelid == modelId;
        });
    return found == scene.models.end() ? nullptr : &*found;
}

const SceneCollisionInstanceResult* FindAdmission(
    const SceneCollisionResult& admission,
    const std::string& instanceId)
{
    const auto found = std::find_if(
        admission.instances.begin(),
        admission.instances.end(),
        [&instanceId](
            const SceneCollisionInstanceResult& item)
        {
            return item.instanceid == instanceId;
        });
    return found == admission.instances.end()
        ? nullptr
        : &*found;
}

SceneRasterIdentity BuildRasterIdentity(
    const MultiModelScene& scene,
    const SceneModelInstance& item,
    const SceneCollisionResult& admission)
{
    const SceneCollisionInstanceResult* evidence =
        FindAdmission(
            admission,
            item.instance.instanceid);
    if (evidence == nullptr)
    {
        throw std::runtime_error(
            "visible scene instance has no admission evidence");
    }

    SceneRasterIdentity identity;
    identity.sceneid = scene.sceneid;
    identity.modelid = item.instance.modelid;
    identity.instanceid = item.instance.instanceid;
    identity.scenerevision = scene.scenerevision;
    identity.transformrevision =
        item.instance.transformrevision;
    identity.admittedtransformrevision =
        evidence->transformrevision;
    identity.transformhash = evidence->transformhash;
    identity.admittedtransformhash =
        evidence->transformhash;
    identity.visible = item.instance.visible;
    identity.admitted =
        evidence->admissionstatus
        == SceneInstanceAdmissionStatus::Admitted;
    identity.effectivepipelinemode =
        SlicePipelineMode::Legacy;
    return identity;
}

std::map<std::string, LoadedSceneModel> LoadSceneModels(
    const MultiModelScene& scene,
    const SliceConfig& profile,
    const std::filesystem::path& profileConfigPath,
    const std::filesystem::path& sceneBaseDirectory)
{
    std::set<std::string> visibleModelIds;
    for (const SceneModelInstance& item : scene.instances)
    {
        if (item.instance.visible)
        {
            visibleModelIds.insert(item.instance.modelid);
        }
    }

    std::map<std::string, LoadedSceneModel> loaded;
    for (const ModelSource& source : scene.models)
    {
        if (!visibleModelIds.contains(source.modelid))
        {
            continue;
        }
        const std::filesystem::path sourcePath =
            ResolvePath(
                source.sourcepath,
                sceneBaseDirectory);
        if (!std::filesystem::is_regular_file(sourcePath))
        {
            throw std::runtime_error(
                "scene model source is missing: "
                + sourcePath.generic_string());
        }

        SliceConfig modelConfig = profile;
        modelConfig.input.model_path = sourcePath;
        modelConfig.input.format = source.format;
        validate_slice_config(modelConfig);
        SceneModel model = load_model_report(
            modelConfig,
            profileConfigPath.parent_path());
        if (model.triangles.empty())
        {
            throw std::runtime_error(
                "scene model importer returned no triangles");
        }
        const std::string sourceHash =
            ComputeSha256(ReadFile(sourcePath));
        const std::string resourceHash =
            ComputeSceneResourceHash(model);
        if (sourceHash != source.sourcehash
            || resourceHash != source.resourcehash)
        {
            throw std::runtime_error(
                "scene model or adjacent resource hash mismatch: modelId="
                + source.modelid
                + " sourceHashMatch="
                + (sourceHash == source.sourcehash ? "true" : "false")
                + " resourceHashExpected=" + source.resourcehash
                + " resourceHashActual=" + resourceHash);
        }

        LoadedSceneModel item;
        item.source = source;
        item.source.sourcepath = sourcePath;
        item.model = std::move(model);
        loaded.emplace(source.modelid, std::move(item));
    }
    return loaded;
}

SceneCollisionResult AdmitScene(
    const MultiModelScene& scene,
    const SliceConfig& profile,
    const std::map<std::string, LoadedSceneModel>& models,
    const SceneValidationPurpose purpose)
{
    SceneCollisionRequest request;
    request.sceneid = scene.sceneid;
    request.currentscenerevision = scene.scenerevision;
    request.expectedscenerevision = scene.scenerevision;
    request.purpose = purpose;
    request.buildvolume = scene.buildvolume;
    request.contactepsilonmm = 0.0;

    for (const SceneModelInstance& item : scene.instances)
    {
        if (!item.instance.visible)
        {
            const ModelTransformHashResult transformHash =
                ComputeModelTransformHash(
                    item.instance.transform,
                    item.instance.sourcetransformidentity,
                    item.instance.instanceid,
                    item.instance.modelid);
            if (!transformHash.IsValid())
            {
                throw std::runtime_error(
                    "hidden scene instance transform identity is invalid");
            }
            SceneViewGeometry hiddenGeometry;
            hiddenGeometry.sceneid = scene.sceneid;
            hiddenGeometry.modelid = item.instance.modelid;
            hiddenGeometry.instanceid =
                item.instance.instanceid;
            hiddenGeometry.scenerevision =
                scene.scenerevision;
            hiddenGeometry.transformrevision =
                item.instance.transformrevision;
            hiddenGeometry.transformhash =
                transformHash.hash;
            hiddenGeometry.visible = false;
            hiddenGeometry.admissionstatus =
                SceneViewAdmissionStatus::Unknown;

            SceneCollisionItem hiddenItem;
            hiddenItem.instance = item.instance;
            hiddenItem.admissionstatus =
                item.admissionstatus;
            hiddenItem.geometry =
                std::move(hiddenGeometry);
            request.items.push_back(std::move(hiddenItem));
            continue;
        }
        const auto model = models.find(item.instance.modelid);
        if (model == models.end())
        {
            throw std::runtime_error(
                "visible scene instance references an unresolved model");
        }
        if (item.admissionstatus
            != SceneInstanceAdmissionStatus::Admitted)
        {
            throw std::runtime_error(
                "visible scene instance has no passing preflight admission");
        }

        SceneViewGeometryRequest geometryRequest;
        geometryRequest.sceneid = scene.sceneid;
        geometryRequest.scenerevision = scene.scenerevision;
        geometryRequest.expectedscenerevision =
            scene.scenerevision;
        geometryRequest.expectedtransformrevision =
            item.instance.transformrevision;
        geometryRequest.instance = item.instance;
        geometryRequest.textureoptions = {
            profile.texture.sampler,
            profile.texture.uv_address_mode,
            profile.texture.flip_v,
        };
        geometryRequest.admissionstatus =
            SceneViewAdmissionStatus::Admitted;
        geometryRequest.buildsurfacepreview = false;
        SceneViewGeometryResult geometry =
            BuildSceneViewGeometry(
                model->second.model,
                geometryRequest);
        if (!geometry.IsValid())
        {
            throw std::runtime_error(
                geometry.error->message);
        }

        SceneCollisionItem collisionItem;
        collisionItem.instance = item.instance;
        collisionItem.admissionstatus =
            item.admissionstatus;
        collisionItem.geometry =
            std::move(geometry.geometry);
        request.items.push_back(std::move(collisionItem));
    }
    return EvaluateSceneCollisionAdmission(request);
}

bool PackageIdentityMatches(
    const std::filesystem::path& packageDirectory,
    const MultiModelScene& scene)
{
    try
    {
        std::ifstream manifestInput(
            packageDirectory / "manifest.json",
            std::ios::binary);
        std::ifstream reportInput(
            packageDirectory
                / "reports/multimodel_scene_report.json",
            std::ios::binary);
        if (!manifestInput || !reportInput)
        {
            return false;
        }
        const Json manifest = Json::parse(manifestInput);
        const Json report = Json::parse(reportInput);
        const std::string sceneHash =
            ComputeMultiModelSceneHash(scene);
        return manifest.at("scene")
                   .at("sceneId")
                   .as_string()
                == scene.sceneid
            && static_cast<std::uint64_t>(
                   manifest.at("scene")
                       .at("sceneRevision")
                       .as_double())
                == scene.scenerevision
            && manifest.at("scene")
                   .at("sceneHash")
                   .as_string()
                == sceneHash
            && report.at("sceneId").as_string()
                == scene.sceneid
            && report.at("sceneHash").as_string()
                == sceneHash;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

}  // namespace

bool MultiModelProductionResult::IsValid() const
{
    return packagewritten
        && !packagedir.empty()
        && !error.has_value();
}

std::string_view MultiModelProductionErrorCodeName(
    const MultiModelProductionErrorCode code)
{
    switch (code)
    {
    case MultiModelProductionErrorCode::None:
        return "NONE";
    case MultiModelProductionErrorCode::EffectiveConfigInvalid:
        return "SCENE_EFFECTIVE_CONFIG_INVALID";
    case MultiModelProductionErrorCode::EffectiveConfigStale:
        return "SCENE_EFFECTIVE_CONFIG_STALE";
    case MultiModelProductionErrorCode::ResourceUnresolved:
        return "SCENE_RESOURCE_UNRESOLVED";
    case MultiModelProductionErrorCode::ProfileMismatch:
        return "SCENE_PROFILE_MISMATCH";
    case MultiModelProductionErrorCode::BuildVolumeUndefined:
        return "SCENE_BUILD_VOLUME_UNDEFINED";
    case MultiModelProductionErrorCode::PipelineModeNotAdmitted:
        return "SCENE_PIPELINE_MODE_NOT_ADMITTED";
    case MultiModelProductionErrorCode::ProductionPackageInvalid:
        return "SCENE_PRODUCTION_PACKAGE_INVALID";
    }
    return "SCENE_PRODUCTION_PACKAGE_INVALID";
}

MultiModelProductionResult RunMultiModelProductionService(
    const MultiModelProductionRequest& request)
{
    const ProductionClock::time_point runStart =
        ProductionClock::now();
    ProductionClock::time_point phaseStart = runStart;
    SliceRunProfile runProfile;
    runProfile.available = true;
    runProfile.profile_level = "scene_detailed";
    ReportProgress(
        request,
        runStart,
        "scene_config_load",
        0,
        1,
        0);

    if (request.effectiveconfigpath.empty())
    {
        return Block(
            request,
            MultiModelProductionErrorCode::EffectiveConfigInvalid,
            "effectiveconfigpath",
            "scene effective config path must not be empty");
    }

    const SceneEffectiveConfigResult effective =
        ReadSceneEffectiveConfig(request.effectiveconfigpath);
    if (!effective.IsValid())
    {
        return Block(
            request,
            MultiModelProductionErrorCode::EffectiveConfigInvalid,
            "effectiveconfigpath",
            effective.error.has_value()
                ? effective.error->message
                : "scene effective config is invalid");
    }

    MultiModelScene scene;
    SceneProductionContract contract;
    try
    {
        const MultiModelSceneDecodeResult decoded =
            DeserializeMultiModelScene(
                effective.document.at("sceneConfig"));
        if (!decoded.IsValid())
        {
            return Block(
                request,
                MultiModelProductionErrorCode::
                    EffectiveConfigInvalid,
                "sceneConfig",
                decoded.error->message);
        }
        scene = decoded.scene;
        contract = ReadContract(
            effective.document,
            request.effectiveconfigpath);
    }
    catch (const std::exception& exception)
    {
        return Block(
            request,
            MultiModelProductionErrorCode::EffectiveConfigInvalid,
            "sliceContract",
            exception.what());
    }

    if (ValidateSceneIdentity(
            effective.document,
            scene)
            .has_value())
    {
        return Block(
            request,
            MultiModelProductionErrorCode::EffectiveConfigStale,
            "identity",
            "scene identity, revision, or hash is stale",
            scene.sceneid);
    }
    if (contract.slicepipelinemode
        != SlicePipelineModeName(SlicePipelineMode::Legacy))
    {
        return Block(
            request,
            MultiModelProductionErrorCode::
                PipelineModeNotAdmitted,
            "sliceContract.slicePipelineMode",
            "multi-model Global production is not admitted and cannot fall back to Legacy",
            scene.sceneid);
    }
    if (scene.buildvolume.source
            == BuildVolumeSource::Unresolved
        || !scene.buildvolume.widthmm.has_value()
        || !scene.buildvolume.heightmm.has_value())
    {
        return Block(
            request,
            MultiModelProductionErrorCode::BuildVolumeUndefined,
            "sceneConfig.buildVolume",
            "scene production requires an explicit build volume",
            scene.sceneid);
    }
    if (contract.profileconfigpath.empty()
        || !std::filesystem::is_regular_file(
            contract.profileconfigpath)
        || contract.outputpackagedir.empty())
    {
        return Block(
            request,
            MultiModelProductionErrorCode::ResourceUnresolved,
            "sliceContract",
            "scene production requires explicit Profile config and output package paths",
            scene.sceneid);
    }

    const SceneValidationPurpose purpose =
        contract.production
        ? SceneValidationPurpose::Production
        : SceneValidationPurpose::FunctionalFixture;
    const SceneValidationResult sceneValidation =
        ValidateMultiModelScene(scene, purpose);
    if (!sceneValidation.IsValid())
    {
        const SceneValidationError& error =
            sceneValidation.errors.front();
        const MultiModelProductionErrorCode code =
            error.code
                    == SceneValidationErrorCode::
                        BuildVolumeUndefined
                || error.code
                    == SceneValidationErrorCode::
                        BuildVolumeFixtureNotProduction
            ? MultiModelProductionErrorCode::BuildVolumeUndefined
            : MultiModelProductionErrorCode::
                EffectiveConfigInvalid;
        return Block(
            request,
            code,
            error.field,
            error.message,
            scene.sceneid,
            error.modelid,
            error.instanceid);
    }

    const std::size_t visibleInstanceCount =
        static_cast<std::size_t>(std::count_if(
            scene.instances.begin(),
            scene.instances.end(),
            [](const SceneModelInstance& item)
            {
                return item.instance.visible;
            }));
    if (visibleInstanceCount == 0U)
    {
        return Block(
            request,
            MultiModelProductionErrorCode::EffectiveConfigInvalid,
            "sceneConfig.instances",
            "scene production requires at least one visible instance",
            scene.sceneid);
    }

    SliceConfig profile;
    try
    {
        profile = load_slice_config(
            contract.profileconfigpath);
    }
    catch (const std::exception& exception)
    {
        return Block(
            request,
            MultiModelProductionErrorCode::ResourceUnresolved,
            "sliceContract.profileConfigPath",
            exception.what(),
            scene.sceneid);
    }
    if (!profile.material_process_profile.enabled
        || profile.material_process_profile.name
            != scene.resolvedprofileid
        || profile.output.dpi_x != contract.dpix
        || profile.output.dpi_y != contract.dpiy
        || std::abs(
               profile.output.layer_thickness_mm
               - contract.layerheightmm)
            > kNumericTolerance)
    {
        return Block(
            request,
            MultiModelProductionErrorCode::ProfileMismatch,
            "sliceContract.resolvedProfileId",
            "scene Profile identity, DPI, or layer height does not match the explicit Profile config",
            scene.sceneid);
    }

    runProfile.config_load_ms =
        ElapsedMilliseconds(phaseStart);
    ReportProgress(
        request,
        runStart,
        "scene_model_load",
        0,
        static_cast<int>(visibleInstanceCount),
        10);
    phaseStart = ProductionClock::now();
    std::map<std::string, LoadedSceneModel> models;
    try
    {
        models = LoadSceneModels(
            scene,
            profile,
            contract.profileconfigpath,
            contract.scenebasedirectory);
    }
    catch (const std::exception& exception)
    {
        return Block(
            request,
            MultiModelProductionErrorCode::ResourceUnresolved,
            "sceneConfig.models",
            exception.what(),
            scene.sceneid);
    }

    runProfile.model_load_ms =
        ElapsedMilliseconds(phaseStart);
    ReportProgress(
        request,
        runStart,
        "scene_admission",
        0,
        static_cast<int>(visibleInstanceCount),
        18);
    phaseStart = ProductionClock::now();
    SceneCollisionResult admission;
    try
    {
        admission = AdmitScene(
            scene,
            profile,
            models,
            purpose);
    }
    catch (const std::exception& exception)
    {
        return Block(
            request,
            MultiModelProductionErrorCode::EffectiveConfigInvalid,
            "sceneAdmission",
            exception.what(),
            scene.sceneid);
    }
    if (!admission.IsValid())
    {
        const SceneCollisionError* error =
            admission.errors.empty()
            ? nullptr
            : &admission.errors.front();
        return Block(
            request,
            error != nullptr
                    && (error->code
                            == SceneCollisionErrorCode::
                                BuildVolumeUndefined
                        || error->code
                            == SceneCollisionErrorCode::
                                BuildVolumeFixtureNotProduction)
                ? MultiModelProductionErrorCode::
                    BuildVolumeUndefined
                : MultiModelProductionErrorCode::
                    EffectiveConfigInvalid,
            error == nullptr ? "sceneAdmission" : error->field,
            error == nullptr
                ? "scene collision admission failed"
                : error->message,
            scene.sceneid,
            error == nullptr ? std::string{} : error->modelid,
            error == nullptr
                ? std::string{}
                : error->instanceid);
    }

    runProfile.grid_setup_ms =
        ElapsedMilliseconds(phaseStart);
    ReportProgress(
        request,
        runStart,
        "scene_instance_slice",
        0,
        static_cast<int>(visibleInstanceCount),
        22);
    phaseStart = ProductionClock::now();
    std::vector<SceneInstanceRaster> rasters;
    rasters.reserve(scene.instances.size());
    std::size_t visibleInstanceOrdinal{0U};
    int lastInstanceProgressPercent{22};
    for (const SceneModelInstance& item : scene.instances)
    {
        if (!item.instance.visible)
        {
            const SceneRasterIdentity identity =
                BuildRasterIdentity(scene, item, admission);
            SceneInstanceRaster hidden;
            hidden.sceneid = identity.sceneid;
            hidden.modelid = identity.modelid;
            hidden.instanceid = identity.instanceid;
            hidden.scenerevision = identity.scenerevision;
            hidden.transformrevision =
                identity.transformrevision;
            hidden.admittedtransformrevision =
                identity.admittedtransformrevision;
            hidden.transformhash = identity.transformhash;
            hidden.admittedtransformhash =
                identity.admittedtransformhash;
            hidden.visible = false;
            hidden.admitted = false;
            hidden.effectivepipelinemode =
                SlicePipelineMode::Legacy;
            hidden.protocol = FixedSceneRasterProtocol();
            rasters.push_back(std::move(hidden));
            continue;
        }
        const auto model = models.find(item.instance.modelid);
        if (model == models.end())
        {
            return Block(
                request,
                MultiModelProductionErrorCode::ResourceUnresolved,
                "sceneConfig.instances.modelId",
                "visible instance model is unresolved",
                scene.sceneid,
                item.instance.modelid,
                item.instance.instanceid);
        }

        LegacySceneLayerAdapterRequest adapterRequest;
        adapterRequest.configpath =
            contract.profileconfigpath;
        adapterRequest.modelpathoverride =
            model->second.source.sourcepath;
        adapterRequest.modelformatoverride =
            model->second.source.format;
        adapterRequest.identity = BuildRasterIdentity(
            scene,
            item,
            admission);
        adapterRequest.instance = item.instance;
        adapterRequest.modelreportoverride =
            &model->second.model;
        const std::size_t completedBefore =
            visibleInstanceOrdinal;
        adapterRequest.progresscallback =
            [&request,
             runStart,
             completedBefore,
             visibleInstanceCount,
             &lastInstanceProgressPercent](
                const SliceRunProgress& instanceProgress)
            {
                const double instanceFraction =
                    static_cast<double>(
                        std::clamp(
                            instanceProgress.percent,
                            0,
                            100))
                    / 100.0;
                const double sceneFraction =
                    (static_cast<double>(completedBefore)
                     + instanceFraction)
                    / static_cast<double>(
                        visibleInstanceCount);
                const int percent =
                    22
                    + static_cast<int>(
                        std::lround(
                            sceneFraction * 50.0));
                if (percent <= lastInstanceProgressPercent)
                {
                    return;
                }
                lastInstanceProgressPercent = percent;
                ReportProgress(
                    request,
                    runStart,
                    "scene_instance_slice",
                    static_cast<int>(
                        completedBefore + 1U),
                    static_cast<int>(
                        visibleInstanceCount),
                    percent);
            };
        SceneRasterAdapterResult adapted =
            AdaptLegacySceneLayers(adapterRequest);
        if (!adapted.IsValid())
        {
            return Block(
                request,
                MultiModelProductionErrorCode::
                    ProductionPackageInvalid,
                adapted.error.has_value()
                    ? adapted.error->field
                    : "legacyAdapter",
                adapted.error.has_value()
                    ? adapted.error->message
                    : "Legacy scene raster production failed",
                scene.sceneid,
                item.instance.modelid,
                item.instance.instanceid);
        }
        rasters.push_back(std::move(adapted.raster));
        ++visibleInstanceOrdinal;
    }

    runProfile.layer_compute_ms =
        ElapsedMilliseconds(phaseStart);
    ReportProgress(
        request,
        runStart,
        "scene_composition",
        0,
        1,
        72);
    phaseStart = ProductionClock::now();
    MultiModelLayerComposeRequest composeRequest;
    composeRequest.admission = admission;
    composeRequest.currentscenerevision =
        scene.scenerevision;
    composeRequest.effectivepipelinemode =
        SlicePipelineMode::Legacy;
    composeRequest.instances = std::move(rasters);
    composeRequest.quantizationtolerance =
        kRasterQuantizationTolerance;
    SceneLayerComposeResult composition =
        ComposeAdmittedSceneRasters(composeRequest);
    if (!composition.IsValid())
    {
        return Block(
            request,
            MultiModelProductionErrorCode::
                ProductionPackageInvalid,
            composition.error.has_value()
                ? composition.error->field
                : "sceneComposition",
            composition.error.has_value()
                ? composition.error->message
                : "scene layer composition failed",
            scene.sceneid);
    }

    runProfile.layer_compose_ms =
        ElapsedMilliseconds(phaseStart);
    ReportProgress(
        request,
        runStart,
        "scene_package_write",
        0,
        composition.grid.layercount,
        78);
    RgbwsvProductionPackageWriteRequest writeRequest;
    writeRequest.packageDir = contract.outputpackagedir;
    writeRequest.sourceConfigPath =
        request.effectiveconfigpath;
    writeRequest.sourceModelPath =
        effective.document.at("sourceScenePath").as_string();
    writeRequest.sourceFormat = "scene";
    writeRequest.requestedPipelineMode = "legacy";
    writeRequest.effectivePipelineMode = "legacy";
    writeRequest.productionAcceptance =
        contract.production
        ? "scene_production_admitted"
        : "functional_fixture_admitted";
    writeRequest.grid.dpiX = contract.dpix;
    writeRequest.grid.dpiY = contract.dpiy;
    writeRequest.grid.pixelSizeXmm =
        kMillimetersPerInch
        / static_cast<double>(contract.dpix);
    writeRequest.grid.pixelSizeYmm =
        kMillimetersPerInch
        / static_cast<double>(contract.dpiy);
    writeRequest.grid.layerThicknessMm =
        contract.layerheightmm;
    writeRequest.outerVarnish =
        ComputeOuterVarnishDiscretization(
            profile.outer_varnish,
            writeRequest.grid.pixelSizeXmm,
            writeRequest.grid.pixelSizeYmm);
    writeRequest.storage.storageMode =
        profile.output.storage_mode;
    writeRequest.storage.rowsPerStrip =
        profile.output.rows_per_strip;
    writeRequest.storage.tileWidth =
        profile.output.tile_size.at(0U);
    writeRequest.storage.tileHeight =
        profile.output.tile_size.at(1U);
    writeRequest.preview.enabled =
        profile.preview.enabled;
    writeRequest.preview.outputpolicy =
        profile.preview.output_policy;
    writeRequest.preview.format =
        profile.preview.format;
    writeRequest.preview.interval =
        profile.preview.interval;
    int lastPackageProgressPercent{78};
    writeRequest.layerwritecallback =
        [&request,
         runStart,
         &lastPackageProgressPercent](
            const int current,
            const int total)
        {
            const double fraction = total <= 0
                ? 1.0
                : static_cast<double>(current)
                    / static_cast<double>(total);
            const int percent =
                78
                + static_cast<int>(
                    std::lround(fraction * 17.0));
            if (percent <= lastPackageProgressPercent
                && current < total)
            {
                return;
            }
            lastPackageProgressPercent = percent;
            ReportProgress(
                request,
                runStart,
                "scene_package_write",
                current,
                total,
                percent);
        };

    RgbwsvProductionPackageWriteResult written;
    try
    {
        written = WriteMultiModelSceneProductionPackage(
            std::move(writeRequest),
            std::move(composition),
            scene,
            admission);
        runProfile.tiff_write_ms =
            written.profile.tiffwritems;
        runProfile.preview_write_ms =
            written.profile.previewwritems;
        runProfile.report_build_ms =
            written.profile.reportbuildms;
        runProfile.report_write_ms =
            written.profile.reportwritems;
        runProfile.package_publish_ms =
            written.profile.packagepublishms;
        ReportProgress(
            request,
            runStart,
            "scene_package_validation",
            0,
            1,
            96);
        phaseStart = ProductionClock::now();
        if (!written.productionOutputWritten
            || !written.strictProtocolValidated
            || !PackageIdentityMatches(
                contract.outputpackagedir,
                scene))
        {
            return Block(
                request,
                MultiModelProductionErrorCode::
                    ProductionPackageInvalid,
                "package",
                "scene package failed strict protocol or identity validation",
                scene.sceneid);
        }
        runProfile.package_publish_ms +=
            ElapsedMilliseconds(phaseStart);
    }
    catch (const std::exception& exception)
    {
        return Block(
            request,
            MultiModelProductionErrorCode::
                ProductionPackageInvalid,
            "package",
            exception.what(),
            scene.sceneid);
    }

    MultiModelProductionResult result;
    result.packagewritten = true;
    result.packagedir = contract.outputpackagedir;
    result.sceneid = scene.sceneid;
    result.scenerevision = scene.scenerevision;
    result.scenehash = ComputeMultiModelSceneHash(scene);
    result.effectiveconfighash = effective.confighash;
    result.visibleinstancecount = visibleInstanceCount;
    result.layercount = written.layerCount;
    runProfile.slice_processing_ms =
        runProfile.grid_setup_ms
        + runProfile.mask_sampling_ms
        + runProfile.texture_prepare_ms
        + runProfile.support_generation_ms
        + runProfile.layer_compute_ms
        + runProfile.layer_compose_ms;
    runProfile.output_write_ms =
        runProfile.tiff_write_ms
        + runProfile.preview_write_ms
        + runProfile.report_write_ms
        + runProfile.package_publish_ms;
    runProfile.total_ms =
        ElapsedMilliseconds(runStart);
    result.profile = runProfile;
    ReportProgress(
        request,
        runStart,
        "completed",
        1,
        1,
        100);
    return result;
}

}  // namespace slicer_core
