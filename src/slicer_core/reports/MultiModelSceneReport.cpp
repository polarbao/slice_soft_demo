#include "slicer_core/reports/MultiModelSceneReport.h"

#include "slicer_core/config/SlicePipelineConfig.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace slicer_core
{
namespace
{

Json TransformToJson(const ModelTransform& transform)
{
    return Json::object({
        {"translateXmm", transform.translatexmm},
        {"translateYmm", transform.translateymm},
        {"rotateZdeg", transform.rotatezdeg},
        {"uniformScale", transform.uniformscale},
        {"mirrorX", transform.mirrorx},
        {"mirrorY", transform.mirrory},
    });
}

Json GridToJson(const SceneRasterGrid& grid)
{
    return Json::object({
        {"widthPx", grid.widthpx},
        {"heightPx", grid.heightpx},
        {"layerCount", grid.layercount},
        {"originMm",
         Json::array({
             grid.originxmm,
             grid.originymm,
             grid.originzmm})},
        {"pixelSizeMm",
         Json::array({grid.pitchxmm, grid.pitchymm})},
        {"layerThicknessMm", grid.layerthicknessmm},
    });
}

Json ProtocolToJson(const RgbwsvProtocol& protocol)
{
    Json::Array channelOrder;
    channelOrder.reserve(protocol.channel_order.size());
    for (const std::string& channel : protocol.channel_order)
    {
        channelOrder.emplace_back(channel);
    }
    return Json::object({
        {"schema", protocol.schema},
        {"channelOrder", Json{std::move(channelOrder)}},
        {"bitDepth", protocol.bit_depth},
        {"polarity", protocol.polarity},
        {"printValue", static_cast<int>(protocol.print_value)},
        {"emptyValue", static_cast<int>(protocol.empty_value)},
    });
}

Json ComposeTotalsToJson(
    const SceneLayerComposeStatistics& statistics)
{
    return Json::object({
        {"modelPixels", static_cast<std::uint64_t>(statistics.modelpixels)},
        {"outerVarnishPixels",
         static_cast<std::uint64_t>(statistics.outervarnishpixels)},
        {"supportPixels",
         static_cast<std::uint64_t>(statistics.supportpixels)},
        {"emptyPixels", static_cast<std::uint64_t>(statistics.emptypixels)},
    });
}

const SceneCollisionInstanceResult& FindAdmission(
    const std::unordered_map<
        std::string,
        const SceneCollisionInstanceResult*>& byId,
    const std::string& instanceId)
{
    const auto found = byId.find(instanceId);
    if (found == byId.end())
    {
        throw std::invalid_argument(
            "scene report instance is missing collision admission");
    }
    return *found->second;
}

SceneInstanceComposeStatistics FindComposeStatistics(
    const std::unordered_map<
        std::string,
        const SceneInstanceComposeStatistics*>& byId,
    const SceneModelInstance& instance)
{
    const auto found = byId.find(instance.instance.instanceid);
    if (found == byId.end())
    {
        if (!instance.instance.visible)
        {
            SceneInstanceComposeStatistics empty;
            empty.instanceid = instance.instance.instanceid;
            return empty;
        }
        throw std::invalid_argument(
            "visible scene report instance is missing composition statistics");
    }
    return *found->second;
}

Json CollisionIdsToJson(
    const std::vector<std::string>& collisionIds)
{
    Json::Array result;
    result.reserve(collisionIds.size());
    for (const std::string& value : collisionIds)
    {
        result.emplace_back(value);
    }
    return Json{std::move(result)};
}

Json InstanceToJson(
    const SceneModelInstance& instance,
    const SceneCollisionInstanceResult& admission,
    const SceneInstanceComposeStatistics& statistics)
{
    return Json::object({
        {"instanceId", instance.instance.instanceid},
        {"modelId", instance.instance.modelid},
        {"visible", instance.instance.visible},
        {"locked", instance.instance.locked},
        {"transformRevision",
         static_cast<std::uint64_t>(
             instance.instance.transformrevision)},
        {"transformHash", admission.transformhash},
        {"requestedTransform",
         TransformToJson(instance.requestedtransform)},
        {"effectiveTransform",
         TransformToJson(instance.effectivetransform)},
        {"admission",
         Json::object({
             {"status",
              admission.admissionstatus
                      == SceneInstanceAdmissionStatus::Admitted
                  ? "admitted"
                  : "blocked"},
             {"boundsValid", admission.boundsvalid},
             {"inBounds", admission.inbounds},
             {"skippedHidden", admission.skippedhidden},
             {"collisionIds",
              CollisionIdsToJson(admission.collisionids)},
         })},
        {"composition",
         Json::object({
             {"modelPixels",
              static_cast<std::uint64_t>(statistics.modelpixels)},
             {"outerVarnishPixels",
              static_cast<std::uint64_t>(
                  statistics.outervarnishpixels)},
             {"supportPixels",
              static_cast<std::uint64_t>(
                  statistics.supportpixels)},
         })},
    });
}

Json ModelsToJson(const MultiModelScene& scene)
{
    Json::Array result;
    result.reserve(scene.models.size());
    for (const ModelSource& model : scene.models)
    {
        result.push_back(Json::object({
            {"modelId", model.modelid},
            {"sourcePath", model.sourcepath.generic_string()},
            {"format", model.format},
            {"resourceScopeId", model.resourcescopeid},
            {"sourceHash", model.sourcehash},
            {"resourceHash", model.resourcehash},
        }));
    }
    return Json{std::move(result)};
}

bool SameBuildVolume(
    const SceneBuildVolume& first,
    const SceneBuildVolume& second)
{
    return first.source == second.source
        && first.widthmm == second.widthmm
        && first.heightmm == second.heightmm
        && first.origin == second.origin
        && first.xdirection == second.xdirection
        && first.ydirection == second.ydirection
        && first.isfixture == second.isfixture;
}

bool HasValidAdmissionEvidence(
    const MultiModelScene& scene,
    const SceneCollisionResult& admission)
{
    if (!std::isfinite(admission.contactepsilonmm)
        || admission.contactepsilonmm < 0.0
        || !SameBuildVolume(
            scene.buildvolume,
            admission.buildvolume)
        || !admission.errors.empty()
        || !admission.collisionpairs.empty()
        || admission.statistics.totalinstancecount
            != admission.instances.size()
        || admission.statistics.totalinstancecount
            != scene.instances.size()
        || admission.statistics.visibleinstancecount
                + admission.statistics.hiddeninstancecount
            != admission.statistics.totalinstancecount
        || admission.statistics.collisionpaircount != 0U
        || admission.statistics.exacttestedpaircount
            > admission.statistics.aabbcandidatepaircount)
    {
        return false;
    }

    std::size_t visibleCount{0U};
    for (const SceneModelInstance& sceneInstance : scene.instances)
    {
        const auto found = std::find_if(
            admission.instances.begin(),
            admission.instances.end(),
            [&sceneInstance](
                const SceneCollisionInstanceResult& candidate)
            {
                return candidate.instanceid
                    == sceneInstance.instance.instanceid;
            });
        if (found == admission.instances.end()
            || found->modelid != sceneInstance.instance.modelid
            || found->visible != sceneInstance.instance.visible
            || found->admissionstatus
                != sceneInstance.admissionstatus
            || !found->errors.empty()
            || !found->collisionids.empty())
        {
            return false;
        }

        if (sceneInstance.instance.visible)
        {
            ++visibleCount;
            if (found->skippedhidden
                || found->admissionstatus
                    != SceneInstanceAdmissionStatus::Admitted
                || !found->boundsvalid
                || !found->inbounds)
            {
                return false;
            }
        }
        else if (!found->skippedhidden
                 || found->boundsvalid
                 || found->inbounds)
        {
            return false;
        }
    }
    return visibleCount
            == admission.statistics.visibleinstancecount
        && scene.instances.size() - visibleCount
            == admission.statistics.hiddeninstancecount;
}

}  // namespace

MultiModelSceneReportDocument BuildMultiModelSceneReport(
    const MultiModelScene& scene,
    const SceneCollisionResult& admission,
    const SceneLayerComposeResult& composition,
    const std::string_view requestedPipelineMode,
    const std::filesystem::path& packageDir)
{
    if (!ValidateMultiModelScene(
             scene,
             admission.purpose)
             .IsValid()
        || scene.sceneid.empty()
        || scene.sceneid != admission.sceneid
        || scene.sceneid != composition.sceneid
        || scene.scenerevision != admission.sourcescenerevision
        || scene.scenerevision != composition.scenerevision
        || !admission.IsValid()
        || !HasValidAdmissionEvidence(scene, admission)
        || admission.scenestatus != SceneCollisionStatus::Passed
        || (admission.purpose
                == SceneValidationPurpose::FunctionalFixture
            && (!admission.functionalallowed
                || admission.productionallowed))
        || (admission.purpose == SceneValidationPurpose::Production
            && (admission.functionalallowed
                || !admission.productionallowed))
        || (admission.purpose == SceneValidationPurpose::Draft)
        || !composition.IsValid()
        || requestedPipelineMode.empty()
        || requestedPipelineMode
            != SlicePipelineModeName(
                composition.effectivepipelinemode)
        || scene.models.empty()
        || scene.instances.empty()
        || admission.instances.size() != scene.instances.size())
    {
        throw std::invalid_argument(
            "multi-model scene report evidence is invalid or stale");
    }

    std::unordered_set<std::string> modelIds;
    for (const ModelSource& model : scene.models)
    {
        if (model.modelid.empty()
            || !modelIds.insert(model.modelid).second)
        {
            throw std::invalid_argument(
                "multi-model scene report model identity is invalid");
        }
    }

    std::unordered_map<
        std::string,
        const SceneCollisionInstanceResult*> admissionById;
    for (const SceneCollisionInstanceResult& instance :
         admission.instances)
    {
        if (instance.instanceid.empty()
            || !admissionById.emplace(
                    instance.instanceid,
                    &instance).second)
        {
            throw std::invalid_argument(
                "multi-model scene report admission identity is invalid");
        }
    }

    std::unordered_map<
        std::string,
        const SceneInstanceComposeStatistics*> composeById;
    for (const SceneInstanceComposeStatistics& instance :
         composition.statistics.instances)
    {
        if (instance.instanceid.empty()
            || !composeById.emplace(
                    instance.instanceid,
                    &instance).second)
        {
            throw std::invalid_argument(
                "multi-model scene report composition identity is invalid");
        }
    }

    Json::Array instances;
    instances.reserve(scene.instances.size());
    std::unordered_set<std::string> instanceIds;
    std::size_t visibleCount{0U};
    for (const SceneModelInstance& instance : scene.instances)
    {
        if (instance.instance.instanceid.empty()
            || !instanceIds.insert(
                    instance.instance.instanceid).second
            || modelIds.find(instance.instance.modelid)
                == modelIds.end())
        {
            throw std::invalid_argument(
                "multi-model scene report instance identity is invalid");
        }
        const SceneCollisionInstanceResult& instanceAdmission =
            FindAdmission(
                admissionById,
                instance.instance.instanceid);
        const ModelTransformHashResult transformHash =
            ComputeModelTransformHash(
                instance.instance.transform,
                instance.instance.sourcetransformidentity,
                instance.instance.instanceid,
                instance.instance.modelid);
        if (instanceAdmission.modelid
                != instance.instance.modelid
            || instanceAdmission.transformrevision
                != instance.instance.transformrevision
            || !ModelTransformsEquivalent(
                instance.instance.transform,
                instance.effectivetransform)
            || !transformHash.IsValid()
            || instanceAdmission.transformhash
                != transformHash.hash)
        {
            throw std::invalid_argument(
                "multi-model scene report transform evidence is stale");
        }
        if (instance.instance.visible)
        {
            ++visibleCount;
        }
        instances.push_back(
            InstanceToJson(
                instance,
                instanceAdmission,
                FindComposeStatistics(composeById, instance)));
    }
    if (visibleCount
            != composition.statistics.visibleinstancecount
        || scene.instances.size()
            != composition.statistics.totalinstancecount
        || composition.statistics.outputlayercount
            != composition.layers.size()
        || composition.statistics.hiddeninstancecount
            != scene.instances.size() - visibleCount
        || composeById.size() != visibleCount)
    {
        throw std::invalid_argument(
            "multi-model scene report aggregate counts do not match");
    }

    const Json sceneJson = SerializeMultiModelScene(scene);
    const std::string sceneHash =
        ComputeMultiModelSceneHash(scene);
    const std::string reportPath =
        MultiModelSceneReportRelativePath().generic_string();
    const std::filesystem::path absolutePackageDir =
        std::filesystem::absolute(packageDir).lexically_normal();

    MultiModelSceneReportDocument document;
    document.manifestsummary = Json::object({
        {"schema", std::string{MultiModelSceneSummarySchemaName()}},
        {"sceneId", scene.sceneid},
        {"sceneRevision",
         static_cast<std::uint64_t>(scene.scenerevision)},
        {"sceneHash", sceneHash},
        {"modelCount", static_cast<int>(scene.models.size())},
        {"instanceCount", static_cast<int>(scene.instances.size())},
        {"visibleInstanceCount", static_cast<int>(visibleCount)},
        {"productionReady", admission.productionallowed},
        {"sceneReport", reportPath},
    });
    document.report = Json::object({
        {"schema", std::string{MultiModelSceneReportSchemaName()}},
        {"status",
         admission.productionallowed
             ? "production_format_written"
             : "functional_fixture_format_written"},
        {"productionOutputWritten", true},
        {"productionReady", admission.productionallowed},
        {"admissionPurpose",
         admission.purpose == SceneValidationPurpose::Production
             ? "production"
             : "functional_fixture"},
        {"sceneId", scene.sceneid},
        {"sceneRevision",
         static_cast<std::uint64_t>(scene.scenerevision)},
        {"sceneHash", sceneHash},
        {"requestedPipelineMode",
         std::string{requestedPipelineMode}},
        {"effectivePipelineMode",
         SlicePipelineModeName(
             composition.effectivepipelinemode)},
        {"resolvedProfileId", scene.resolvedprofileid},
        {"modelCount", static_cast<int>(scene.models.size())},
        {"instanceCount", static_cast<int>(scene.instances.size())},
        {"visibleInstanceCount", static_cast<int>(visibleCount)},
        {"hiddenInstanceCount",
         static_cast<int>(scene.instances.size() - visibleCount)},
        {"buildVolume", sceneJson.at("buildVolume")},
        {"layout", sceneJson.at("layout")},
        {"models", ModelsToJson(scene)},
        {"instances", Json{std::move(instances)}},
        {"admission",
         Json::object({
             {"functionalAllowed", admission.functionalallowed},
             {"productionAllowed", admission.productionallowed},
             {"aabbCandidatePairCount",
              static_cast<std::uint64_t>(
                  admission.statistics.aabbcandidatepaircount)},
             {"exactTestedPairCount",
              static_cast<std::uint64_t>(
                  admission.statistics.exacttestedpaircount)},
             {"collisionPairCount",
              static_cast<std::uint64_t>(
                  admission.statistics.collisionpaircount)},
         })},
        {"composition",
         Json::object({
             {"grid", GridToJson(composition.grid)},
             {"protocol", ProtocolToJson(composition.protocol)},
             {"layerCount",
              static_cast<int>(composition.layers.size())},
             {"totalInstanceCount",
              static_cast<std::uint64_t>(
                  composition.statistics.totalinstancecount)},
             {"visibleInstanceCount",
              static_cast<std::uint64_t>(
                  composition.statistics.visibleinstancecount)},
             {"hiddenInstanceCount",
              static_cast<std::uint64_t>(
                  composition.statistics.hiddeninstancecount)},
             {"totals",
              ComposeTotalsToJson(composition.statistics)},
             {"composeMs", composition.composems},
             {"peakWorkingBytes",
              static_cast<std::uint64_t>(
                  composition.peakworkingbytes)},
         })},
        {"package",
         Json::object({
             {"path", absolutePackageDir.generic_string()},
             {"manifest", "manifest.json"},
             {"sceneReport", reportPath},
         })},
    });
    if (!document.IsValid())
    {
        throw std::invalid_argument(
            "multi-model scene report document failed validation");
    }
    return document;
}

}  // namespace slicer_core
