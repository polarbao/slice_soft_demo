#include "slicer_core/pipeline/MultiModelProductionService.h"

#include "slicer_core/api/artifacts/PackageArtifactSafety.h"
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
#include "slicer_core/system/Utf8Path.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include "slicer_core/pipeline/SceneLayerBarrier.h"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
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

#include "slicer_core/pipeline/MultiModelProductionCancellation.h"

struct SceneProductionContract
{
    std::filesystem::path profileconfigpath;
    std::filesystem::path outputpackagedir;
    std::filesystem::path scenebasedirectory;
    int dpix{0};
    int dpiy{0};
    double layerheightmm{0.0};
    std::string slicepipelinemode;
    std::string geometrysamplingstrategy;
    bool production{false};
};

struct LoadedSceneModel
{
    ModelSource source;
    SceneModel model;
    SliceRunImportProfile importprofile;
};

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
        PathFromUtf8(contract.at("profileConfigPath").as_string()),
        baseDirectory);
    result.outputpackagedir = ResolvePath(
        PathFromUtf8(contract.at("outputPackageDir").as_string()),
        baseDirectory);
    result.scenebasedirectory = ResolvePath(
        PathFromUtf8(document.at("sourceScenePath").as_string()),
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
    result.geometrysamplingstrategy =
        contract.at("geometrySamplingStrategy").as_string();
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
    const std::filesystem::path& sceneBaseDirectory,
    const MultiModelProductionRequest& request)
{
    std::set<std::string> visibleModelIds;
    for (const SceneModelInstance& item : scene.instances)
    {
        ThrowIfCancellationRequested(request, "scene_model_visibility_scan");
        if (item.instance.visible)
        {
            visibleModelIds.insert(item.instance.modelid);
        }
    }

    std::map<std::string, LoadedSceneModel> loaded;
    for (const ModelSource& source : scene.models)
    {
        ThrowIfCancellationRequested(request, "scene_model_load");
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
        modelConfig.auto_orient.enabled &= source.autoorientenabled;
        validate_slice_config(modelConfig);
        const ProductionClock::time_point parseStart =
            ProductionClock::now();
        SceneModel model = load_model_report(
            modelConfig,
            profileConfigPath.parent_path());
        const double parseMs =
            ElapsedMilliseconds(parseStart);
        ThrowIfCancellationRequested(request, "scene_model_loaded");
        if (model.triangles.empty())
        {
            throw std::runtime_error(
                "scene model importer returned no triangles");
        }
        const ProductionClock::time_point hashStart =
            ProductionClock::now();
        const std::string sourceHash =
            ComputeSha256(ReadFile(sourcePath));
        const std::string resourceHash =
            ComputeSceneResourceHash(model);
        const double hashMs =
            ElapsedMilliseconds(hashStart);
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
        item.importprofile.modelid = source.modelid;
        item.importprofile.sourcepath =
            PathToUtf8(sourcePath);
        item.importprofile.parsems = parseMs;
        item.importprofile.texturems = std::nullopt;
        item.importprofile.previewms = std::nullopt;
        item.importprofile.hashms = hashMs;
        loaded.emplace(source.modelid, std::move(item));
    }
    return loaded;
}

SceneCollisionResult AdmitScene(
    const MultiModelScene& scene,
    const SliceConfig& profile,
    const std::map<std::string, LoadedSceneModel>& models,
    const SceneValidationPurpose purpose,
    const MultiModelProductionRequest& productionRequest)
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
        ThrowIfCancellationRequested(productionRequest, "scene_admission");
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
        ThrowIfCancellationRequested(
            productionRequest, "scene_admission_geometry");
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
    case MultiModelProductionErrorCode::OutputPublicationFailed:
        return "SCENE_OUTPUT_PUBLICATION_FAILED";
    case MultiModelProductionErrorCode::PackageTargetBusy:
        return "SCENE_PACKAGE_TARGET_BUSY";
    case MultiModelProductionErrorCode::Cancelled:
        return "SCENE_PRODUCTION_CANCELLED";
    }
    return "SCENE_PRODUCTION_PACKAGE_INVALID";
}

namespace
{

MultiModelProductionResult RunMultiModelProductionServiceImpl(
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
    if (contract.geometrysamplingstrategy != "legacy_center_sample"
        && contract.geometrysamplingstrategy
            != "layer_slab_supersample_2x2_at_least_two_candidate")
    {
        return Block(
            request,
            MultiModelProductionErrorCode::EffectiveConfigInvalid,
            "sliceContract.geometrySamplingStrategy",
            "scene geometry sampling strategy is not approved for production integration",
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
            > kNumericTolerance
        || profile.geometry_sampling.strategy
            != contract.geometrysamplingstrategy)
    {
        return Block(
            request,
            MultiModelProductionErrorCode::ProfileMismatch,
            "sliceContract.resolvedProfileId",
            "scene Profile identity, DPI, layer height, or geometry sampling does not match the explicit Profile config",
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
            contract.scenebasedirectory,
            request);
    }
    catch (const ProductionCancellation&)
    {
        throw;
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
    runProfile.imports.reserve(models.size());
    for (const auto& [modelId, loadedModel] : models)
    {
        (void)modelId;
        runProfile.imports.push_back(
            loadedModel.importprofile);
    }
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
            purpose,
            request);
    }
    catch (const ProductionCancellation&)
    {
        throw;
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
    // MF-05 步骤 4（第一步：实例侧流式）。
    //
    // 原实现按实例顺序切完再一起合成，每实例持有全部层（6 通道 + 4 张归属 mask
    // = 10 B/列/层），验证台实测双实例斜率 189 MB/层，用户 0.2+0.3 @10um 即 270 GB。
    // 合成第 L 层只需各实例的第 L 层，故改为每实例一个生产者线程 + 层屏障：
    // 同层到齐即合成，合成完放行下一层。
    //
    // 本步只解决实例侧（10 B/列/层 x N）；合成结果仍累积（6 B/列/层），
    // 由后续「写入侧流式」处理 —— 那需要把 writeRequest 的构造提前，是独立改动。
    // 见任务卡 9.5.4。
    std::vector<SceneInstanceRaster> rasters(scene.instances.size());

    /// 一个生产者实例的全部状态。地址必须稳定（回调持有引用），故用 unique_ptr 持有。
    struct ProducerSlot
    {
        LegacySceneLayerAdapterRequest adapterRequest;
        SceneRasterAdapterResult result;
        /// 当前层。屏障对齐后由 layerprovider 取用，ReleaseLayer 后即可被覆写。
        SceneInstanceRasterLayer layer;
        /// 退回 retained 时在此累积（见下方对齐判定）。
        std::vector<SceneInstanceRasterLayer> retained;
        SceneRasterGrid grid;
        std::size_t rasterindex{0U};
        std::exception_ptr failure;
        bool gridreceived{false};
        /// 本实例已上报的百分比（0-100）。只在 progressMutex 下读写。
        int progresspercent{0};
    };
    std::vector<std::unique_ptr<ProducerSlot>> producerSlots;

    for (std::size_t index{0U}; index < scene.instances.size(); ++index)
    {
        const SceneModelInstance& item = scene.instances.at(index);
        ThrowIfCancellationRequested(request, "scene_instance_slice");
        const SceneRasterIdentity identity =
            BuildRasterIdentity(scene, item, admission);
        if (!item.instance.visible)
        {
            SceneInstanceRaster hidden;
            hidden.sceneid = identity.sceneid;
            hidden.modelid = identity.modelid;
            hidden.instanceid = identity.instanceid;
            hidden.scenerevision = identity.scenerevision;
            hidden.transformrevision = identity.transformrevision;
            hidden.admittedtransformrevision =
                identity.admittedtransformrevision;
            hidden.transformhash = identity.transformhash;
            hidden.admittedtransformhash = identity.admittedtransformhash;
            hidden.visible = false;
            hidden.admitted = false;
            hidden.effectivepipelinemode = SlicePipelineMode::Legacy;
            hidden.protocol = FixedSceneRasterProtocol();
            rasters.at(index) = std::move(hidden);
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

        auto slot = std::make_unique<ProducerSlot>();
        slot->rasterindex = index;
        slot->adapterRequest.configpath = contract.profileconfigpath;
        slot->adapterRequest.modelpathoverride =
            model->second.source.sourcepath;
        slot->adapterRequest.modelformatoverride =
            model->second.source.format;
        slot->adapterRequest.identity = identity;
        slot->adapterRequest.instance = item.instance;
        slot->adapterRequest.modelreportoverride = &model->second.model;
        slot->adapterRequest.canceltoken = request.canceltoken;
        producerSlots.push_back(std::move(slot));
    }

    const std::size_t producerCount = producerSlots.size();

    // 对齐判定：屏障按全局层号推进，offsetz 由实例与全局栅格原点之差算出，而全局
    // 栅格要等所有局部栅格就位 —— 环形依赖。解法是先跑「栅格相位」：各生产者报出
    // localgrid 后等待，主线程检查所有实例 originzmm 相等。⚠ 该值在生产路径【恒为
    // 0】（slicer.cpp 写的是字面量），故判据恒真、无中途改路。详见任务卡 MF-07e。
    enum class StreamAlignment
    {
        Pending,
        Streaming,
        Rejected,
    };
    std::mutex phaseMutex;
    std::condition_variable phaseSignal;
    std::size_t gridsReady{0U};
    std::size_t producersDone{0U};
    StreamAlignment alignment{StreamAlignment::Pending};
    SceneLayerBarrier barrier(producerCount == 0U ? 1U : producerCount);
    // 进度必须在同一把锁下「先判单调、再上报」。
    // 只用原子量比较是不够的：线程 A 把 22 推到 40、线程 B 再推到 50 后，
    // 若两者的上报动作在锁外，B 可能先报 50、A 后报 40，事件序列就倒退了。
    std::mutex progressMutex;
    int reportedProgressPercent{22};

    for (std::size_t slotIndex{0U}; slotIndex < producerCount; ++slotIndex)
    {
        ProducerSlot& slot = *producerSlots.at(slotIndex);
        slot.adapterRequest.gridready =
            [&phaseMutex, &phaseSignal, &gridsReady, &slot](
                const SceneRasterGrid& grid)
            {
                {
                    std::lock_guard<std::mutex> lock(phaseMutex);
                    slot.grid = grid;
                    slot.gridreceived = true;
                    ++gridsReady;
                }
                phaseSignal.notify_all();
            };
        slot.adapterRequest.layersink =
            [&phaseMutex,
             &phaseSignal,
             &alignment,
             &barrier,
             &slot,
             slotIndex](SceneInstanceRasterLayer&& produced) -> bool
            {
                StreamAlignment decided{StreamAlignment::Pending};
                {
                    std::unique_lock<std::mutex> lock(phaseMutex);
                    // 栅格相位在第一层之前完成，故此处不会久等。
                    phaseSignal.wait(lock, [&alignment] {
                        return alignment != StreamAlignment::Pending;
                    });
                    decided = alignment;
                }
                if (decided == StreamAlignment::Rejected)
                {
                    // 退回 retained：累积整栈，不能重跑切片，故在此分流。「不能报错
                    // 否则会让能工作的场景失败」这条旧理由已不成立，见上方判定处。
                    slot.retained.push_back(std::move(produced));
                    return true;
                }
                const int layerIndex = produced.layerindex;
                slot.layer = std::move(produced);
                return barrier.DepositAndWait(slotIndex, layerIndex);
            };
        slot.adapterRequest.progresscallback =
            [&request,
             runStart,
             visibleInstanceCount,
             &progressMutex,
             &reportedProgressPercent,
             &producerSlots,
             &slot](const SliceRunProgress& instanceProgress)
            {
                std::lock_guard<std::mutex> lock(progressMutex);
                slot.progresspercent =
                    std::clamp(instanceProgress.percent, 0, 100);
                // 场景进度取【所有实例的平均】，与原实现「完成数 + 当前实例进度」
                // 同一量纲；否则并发下任一实例跑完都会把场景推到 72。
                long long total{0};
                int completed{0};
                for (const std::unique_ptr<ProducerSlot>& peer : producerSlots)
                {
                    total += peer->progresspercent;
                    if (peer->progresspercent >= 100)
                    {
                        ++completed;
                    }
                }
                const double sceneFraction =
                    static_cast<double>(total)
                    / (100.0 * static_cast<double>(visibleInstanceCount));
                const int percent =
                    22 + static_cast<int>(std::lround(sceneFraction * 50.0));
                if (percent <= reportedProgressPercent)
                {
                    return;
                }
                reportedProgressPercent = percent;
                ReportProgress(
                    request,
                    runStart,
                    "scene_instance_slice",
                    std::min(
                        completed + 1,
                        static_cast<int>(visibleInstanceCount)),
                    static_cast<int>(visibleInstanceCount),
                    percent);
            };
    }

    std::vector<std::thread> producers;
    producers.reserve(producerCount);

    /**
     * @brief 无论走哪条路径都收束生产者线程。
     *
     * std::thread 析构时若仍 joinable 会直接 std::terminate。本函数后面有多个
     * 提前 return（模型未解析、合成失败、包写失败…），合成本身也可能抛异常 ——
     * 任一路径漏掉 join 就是【整个进程被终止】，而不是报错。
     * 实测正是如此：3 实例 @0.01mm 在合成阶段以 0xC0000409 崩溃。
     *
     * 先 Fail 再 join：仍停在屏障上的生产者必须先被唤醒，否则 join 永久挂住。
     * 正常路径已手动 join 并 clear，此时这里是空操作。
     */
    struct ProducerJoinGuard
    {
        std::vector<std::thread>& threads;
        SceneLayerBarrier& barrier;

        ~ProducerJoinGuard()
        {
            if (threads.empty())
            {
                return;
            }
            barrier.Fail();
            for (std::thread& thread : threads)
            {
                if (thread.joinable())
                {
                    thread.join();
                }
            }
            threads.clear();
        }
    };
    const ProducerJoinGuard producerJoinGuard{producers, barrier};
    for (std::size_t slotIndex{0U}; slotIndex < producerCount; ++slotIndex)
    {
        ProducerSlot& slot = *producerSlots.at(slotIndex);
        producers.emplace_back(
            [&slot, &barrier, &phaseMutex, &phaseSignal, &producersDone, slotIndex]
            {
                try
                {
                    slot.result = AdaptLegacySceneLayers(slot.adapterRequest);
                }
                catch (...)
                {
                    slot.failure = std::current_exception();
                    // 任一实例失败必须唤醒全部等待者，否则整个作业挂住。
                    barrier.Fail();
                }
                barrier.Finish(slotIndex);
                {
                    std::lock_guard<std::mutex> lock(phaseMutex);
                    ++producersDone;
                }
                phaseSignal.notify_all();
            });
    }

    {
        // 等到所有实例要么报出栅格、要么已结束（含 fail-closed 与抛异常）。
        // 少了后一个条件，某实例在报栅格前失败就会让主线程死等。
        std::unique_lock<std::mutex> lock(phaseMutex);
        phaseSignal.wait(lock, [&] {
            return gridsReady + producersDone >= producerCount
                || barrier.Failed();
        });
        bool aligned = producerCount > 0U && !barrier.Failed();
        for (std::size_t slotIndex{0U};
             aligned && slotIndex < producerCount;
             ++slotIndex)
        {
            const ProducerSlot& slot = *producerSlots.at(slotIndex);
            if (!slot.gridreceived
                || slot.grid.originzmm
                    != producerSlots.at(0U)->grid.originzmm)
            {
                aligned = false;
            }
        }
        alignment = aligned
            ? StreamAlignment::Streaming
            : StreamAlignment::Rejected;
    }
    phaseSignal.notify_all();
    const bool streamingInstances = alignment == StreamAlignment::Streaming;

    // 生产者收尾：原实现在实例循环里逐个做「校验结果 + 回填 instanceProfile」，
    // 线程化后必须挪到 join 之后。回填【按 slot 序】进行，与 scene.instances 顺序
    // 一致，故 profile.instances 的次序不受线程完成先后影响。
    const auto collectProducerResults =
        [&]() -> std::optional<MultiModelProductionResult>
    {
        for (std::size_t slotIndex{0U}; slotIndex < producerCount; ++slotIndex)
        {
            ProducerSlot& slot = *producerSlots.at(slotIndex);
            if (slot.failure)
            {
                std::rethrow_exception(slot.failure);
            }
            if (!slot.result.IsValid(
                    request.canceltoken, streamingInstances))
            {
                ThrowIfCancellationRequested(request, "scene_instance_slice");
                return Block(
                    request,
                    MultiModelProductionErrorCode::ProductionPackageInvalid,
                    slot.result.error.has_value()
                        ? slot.result.error->field
                        : "legacyAdapter",
                    slot.result.error.has_value()
                        ? slot.result.error->message
                        : "Legacy scene raster production failed",
                    scene.sceneid,
                    slot.adapterRequest.identity.modelid,
                    slot.adapterRequest.identity.instanceid);
            }
            SliceRunInstanceProfile instanceProfile;
            instanceProfile.modelid = slot.adapterRequest.identity.modelid;
            instanceProfile.instanceid =
                slot.adapterRequest.identity.instanceid;
            instanceProfile.widthpx = slot.grid.widthpx;
            instanceProfile.heightpx = slot.grid.heightpx;
            instanceProfile.layercount = slot.grid.layercount;
            if (slot.result.profile.available)
            {
                runProfile.support_statistics_scan_count +=
                    slot.result.profile.support_statistics_scan_count;
                instanceProfile.coreslicems =
                    slot.result.profile.slice_processing_ms;
                instanceProfile.composems =
                    slot.result.profile.layer_compose_ms;
                instanceProfile.totalms = slot.result.profile.total_ms;
            }
            runProfile.instances.push_back(std::move(instanceProfile));
        }
        return std::nullopt;
    };

    // 骨架：identity 与 localgrid 已由栅格相位就位；layers 留空由 provider 现取
    // （退回模式下在 join 后补进整栈）。
    for (std::size_t slotIndex{0U}; slotIndex < producerCount; ++slotIndex)
    {
        const ProducerSlot& slot = *producerSlots.at(slotIndex);
        SceneInstanceRaster& skeleton = rasters.at(slot.rasterindex);
        skeleton.sceneid = slot.adapterRequest.identity.sceneid;
        skeleton.modelid = slot.adapterRequest.identity.modelid;
        skeleton.instanceid = slot.adapterRequest.identity.instanceid;
        skeleton.scenerevision = slot.adapterRequest.identity.scenerevision;
        skeleton.transformrevision =
            slot.adapterRequest.identity.transformrevision;
        skeleton.admittedtransformrevision =
            slot.adapterRequest.identity.admittedtransformrevision;
        skeleton.transformhash = slot.adapterRequest.identity.transformhash;
        skeleton.admittedtransformhash =
            slot.adapterRequest.identity.admittedtransformhash;
        skeleton.visible = true;
        skeleton.admitted = true;
        skeleton.effectivepipelinemode = SlicePipelineMode::Legacy;
        skeleton.protocol = FixedSceneRasterProtocol();
        skeleton.localgrid = slot.grid;
    }

    // 退回模式：生产者仍在把层累积到 slot.retained，等它们跑完再补进整栈，
    // 之后一切照旧走 retained 合成。
    if (!streamingInstances)
    {
        for (std::thread& producer : producers)
        {
            producer.join();
        }
        producers.clear();
        if (const std::optional<MultiModelProductionResult> blocked =
                collectProducerResults())
        {
            return blocked.value();
        }
        for (std::size_t slotIndex{0U}; slotIndex < producerCount; ++slotIndex)
        {
            ProducerSlot& slot = *producerSlots.at(slotIndex);
            rasters.at(slot.rasterindex).layers = std::move(slot.retained);
        }
    }

    runProfile.layer_compute_ms =
        ElapsedMilliseconds(phaseStart);
    {
        // 生产者线程在合成期间仍会跑完剩余动作并上报进度。主线程马上要报
        // scene_composition 的 72，若不先把实例阶段封顶，生产者随后报出的
        // <=72 就会让事件序列倒退（实例进度最大恰为 72）。
        std::lock_guard<std::mutex> lock(progressMutex);
        reportedProgressPercent = 72;
    }
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
    composeRequest.canceltoken = request.canceltoken;
    // 流式：合成按 (实例, 本地层号) 现取层。offsetz 全为 0（对齐判定已保证），
    // 故本地层号即全局层号，屏障可直接按它推进。
    int barrierLayer{-1};
    if (streamingInstances)
    {
        composeRequest.layerprovider =
            [&producerSlots, &barrier, &barrierLayer](
                const SceneInstanceRaster& instance,
                const int localLayerIndex) -> const SceneInstanceRasterLayer*
            {
                if (localLayerIndex != barrierLayer)
                {
                    if (barrierLayer >= 0)
                    {
                        barrier.ReleaseLayer(barrierLayer);
                    }
                    if (!barrier.AwaitLayer(localLayerIndex).has_value())
                    {
                        return nullptr;
                    }
                    barrierLayer = localLayerIndex;
                }
                for (const std::unique_ptr<ProducerSlot>& slot : producerSlots)
                {
                    if (slot->adapterRequest.identity.instanceid
                        != instance.instanceid)
                    {
                        continue;
                    }
                    return slot->layer.layerindex == localLayerIndex
                        ? &slot->layer
                        : nullptr;
                }
                return nullptr;
            };
    }
    // MF-05 步骤4 第二步：写请求与发布会话都要在合成【之前】就位 ——
    // layersink 每合成完一层就立刻写盘，会话必须已建好 staging 与租约。
    // 本块原在合成之后，实测其字段全部来自 contract 与 request、不依赖
    // composition，故可整体上移；依赖 composition 的 grid/scene/能力摘要
    // 由合成后的 WriteValidatedMultiModelSceneProductionPackage 补齐 ——
    // 会话持有 writeRequest 的引用，那时补齐对 Finish 可见。
    RgbwsvProductionPackageWriteRequest writeRequest;
    writeRequest.packageDir = contract.outputpackagedir;
    writeRequest.jobId = request.jobid;
    writeRequest.attemptId = request.attemptid;
    writeRequest.sourceConfigPath =
        request.effectiveconfigpath;
    writeRequest.sourceModelPath =
        effective.document.at("sourceScenePath").as_string();
    writeRequest.sourceFormat = "scene";
    writeRequest.requestedPipelineMode = "legacy";
    writeRequest.effectivePipelineMode = "legacy";
    writeRequest.productionAcceptance =
        contract.production
        ? "admitted"
        : "functional_fixture_admitted";
    writeRequest.manifestWhiteSemantics =
        profile.output.white_semantics;
    writeRequest.profileWhiteSemantics =
        profile.white_semantics;
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
    writeRequest.storage.compression =
        profile.output.tiff_compression;
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
    writeRequest.canceltoken = request.canceltoken;
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
            // total <= 0 时原条件恒不成立、每层都会上报；此处兜底。
            if (percent <= lastPackageProgressPercent
                && current < std::max(total, 1))
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

    // 逐层发布：每合成完一层立刻写盘并放行屏障，合成结果不再累积。
    // 会话此刻建立（staging 与租约就位），依赖 composition 的补齐留到合成之后。
    // 已启用（MF-05，`7d7a627`）：峰值与层数脱钩，双模型 10um 实测 1.91 GB。
    // 启用前挡住它的十一项回归已全部定位修复，根因是一条以「进度分母为 0」
    // 起头的因果链（分母 0 -> 协议判违规 -> 写取消标记 -> 包写一半被取消 ->
    // RAII 回滚清 staging -> 包从未发布）。修法是下方的 SetExpectedLayerCount
    // 与 78% 锚点上移；完整链条见 REPORT §9.5.6 与任务卡 9.5.6。
    constexpr bool kStreamingPackageWriteEnabled = true;
    // 全局层数：对齐判定已保证各 offsetz 为 0，故取各实例 localgrid 的最大层数。
    // 会话在合成前建立，grid.layerCount 此刻还是 0，进度分母必须由此处补上。
    int streamingLayerCount{0};
    for (const std::unique_ptr<ProducerSlot>& slot : producerSlots)
    {
        streamingLayerCount =
            std::max(streamingLayerCount, slot->grid.layercount);
    }
    std::optional<RgbwsvProductionPackageSession> packageSession;
    if (streamingInstances && kStreamingPackageWriteEnabled)
    {
        // 78% 锚点必须先于逐层回调发出，否则事件序列会是 72 -> 95 -> … -> 78，
        // Worker 协议判定为「percent 倒退」。
        ReportProgress(
            request,
            runStart,
            "scene_package_write",
            0,
            streamingLayerCount,
            78);
        packageSession.emplace(writeRequest);
        packageSession->SetExpectedLayerCount(streamingLayerCount);
        composeRequest.layersink =
            [&packageSession, &barrier](
                const int globalLayerIndex,
                RgbwsvProductionLayer&& layer,
                const RgbwsvProductionLayerStatistics&)
            {
                packageSession->AppendLayer(layer);
                // provider 也会在进入下一层前放行上一层；两处都调是幂等的
                // （ReleaseLayer 只清 pendingLayer 恰等于该层的实例）。
                barrier.ReleaseLayer(globalLayerIndex);
            };
    }
    ValidatedSceneLayerComposeResult composition =
        ComposeAdmittedSceneRastersValidated(std::move(composeRequest));
    if (streamingInstances)
    {
        // 放行最后一层并收束生产者。屏障失效也要 join，否则线程对象析构即终止进程。
        if (barrierLayer >= 0)
        {
            barrier.ReleaseLayer(barrierLayer);
        }
        // 合成成功即正常收尾（Drain 放行生产者跑完）；失败才中止（Fail 让其退出）。
        // 两者都必须在 join 之前，否则仍在等待的生产者会让 join 永久挂住。
        if (composition.IsValid())
        {
            barrier.Drain();
        }
        else
        {
            barrier.Fail();
        }
        for (std::thread& producer : producers)
        {
            producer.join();
        }
        producers.clear();
    }
    // 生产者结果在【合成判定之后】才收：合成失败时我们会主动 Fail 屏障，
    // 生产者随即抛「被取消」——先收生产者就会用那个异常盖住合成的真实错误。
    if (composition.IsValid() && streamingInstances)
    {
        if (const std::optional<MultiModelProductionResult> blocked =
                collectProducerResults())
        {
            return blocked.value();
        }
    }
    if (!composition.IsValid())
    {
        const SceneLayerComposeResult& blockedComposition =
            composition.Value();
        ThrowIfCancellationRequested(request, "scene_composition");
        return Block(
            request,
            MultiModelProductionErrorCode::
                ProductionPackageInvalid,
            blockedComposition.error.has_value()
                ? blockedComposition.error->field
                : "sceneComposition",
            blockedComposition.error.has_value()
                ? blockedComposition.error->message
                : "scene layer composition failed",
            scene.sceneid);
    }

    runProfile.layer_compose_ms =
        ElapsedMilliseconds(phaseStart);
    if (!packageSession.has_value())
    {
        // 流式路径已在会话建立前报过该锚点，此处只服务非流式。
        ReportProgress(
            request,
            runStart,
            "scene_package_write",
            0,
            composition.Value().grid.layercount,
            78);
    }

    RgbwsvProductionPackageWriteResult written;
    try
    {
        written = WriteValidatedMultiModelSceneProductionPackage(
            writeRequest,
            std::move(composition),
            scene,
            admission,
            contract.profileconfigpath,
            packageSession.has_value() ? &packageSession.value() : nullptr);
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
    catch (const api::artifacts::PackageArtifactLeaseConflict& exception)
    {
        return Block(
            request,
            MultiModelProductionErrorCode::PackageTargetBusy,
            "packageDir",
            exception.what(),
            scene.sceneid);
    }
    catch (const api::artifacts::PackageArtifactOutputError& exception)
    {
        return Block(
            request,
            MultiModelProductionErrorCode::OutputPublicationFailed,
            "package",
            exception.what(),
            scene.sceneid);
    }
    catch (const std::exception& exception)
    {
        ThrowIfCancellationRequested(request, "scene_package_write");
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

}  // namespace

MultiModelProductionResult RunMultiModelProductionService(
    const MultiModelProductionRequest& request)
{
    try
    {
        return RunMultiModelProductionServiceImpl(request);
    }
    catch (const ProductionCancellation& cancellation)
    {
        return Block(
            request,
            MultiModelProductionErrorCode::Cancelled,
            "canceltoken",
            "scene production stopped at cooperative checkpoint: "
                + std::string(cancellation.what()));
    }
}

}  // namespace slicer_core
