#include "slicer_core/pipeline/GlobalSceneLayerAdapter.h"
#include "slicer_core/pipeline/LegacySceneLayerAdapter.h"
#include "slicer_core/pipeline/MultiModelSliceOrchestrator.h"
#include "slicer_core/config.h"
#include "slicer_core/model.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace
{

constexpr std::size_t kChannelCount{6U};

std::vector<std::string> SnapshotDirectory(
    const std::filesystem::path& directory)
{
    std::vector<std::string> snapshot;
    if (!std::filesystem::exists(directory))
    {
        return snapshot;
    }
    for (const std::filesystem::directory_entry& entry :
         std::filesystem::recursive_directory_iterator(directory))
    {
        std::ostringstream row;
        row << std::filesystem::relative(
                   entry.path(),
                   directory).generic_string()
            << '|'
            << entry.last_write_time()
                   .time_since_epoch()
                   .count();
        if (entry.is_regular_file())
        {
            row << '|' << entry.file_size();
        }
        snapshot.push_back(row.str());
    }
    std::sort(snapshot.begin(), snapshot.end());
    return snapshot;
}

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
    }
    return condition;
}

std::size_t PixelIndex(const int width, const int x, const int y)
{
    return static_cast<std::size_t>(y)
            * static_cast<std::size_t>(width)
        + static_cast<std::size_t>(x);
}

std::size_t ChannelIndex(
    const int width,
    const int x,
    const int y,
    const std::size_t channel)
{
    return PixelIndex(width, x, y) * kChannelCount + channel;
}

slicer_core::SceneRasterIdentity MakeIdentity(
    const std::string& instanceId,
    const slicer_core::SlicePipelineMode mode)
{
    slicer_core::SceneRasterIdentity identity;
    identity.sceneid = "scene";
    identity.modelid = "model-" + instanceId;
    identity.instanceid = instanceId;
    identity.scenerevision = 17U;
    identity.transformrevision = 23U;
    identity.admittedtransformrevision = 23U;
    identity.transformhash = "transform-" + instanceId;
    identity.admittedtransformhash = identity.transformhash;
    identity.visible = true;
    identity.admitted = true;
    identity.effectivepipelinemode = mode;
    return identity;
}

slicer_core::SceneRasterGrid MakeGrid(
    const int width,
    const int height,
    const int layers,
    const double originX,
    const double originY,
    const double originZ = 0.0)
{
    slicer_core::SceneRasterGrid grid;
    grid.widthpx = width;
    grid.heightpx = height;
    grid.layercount = layers;
    grid.originxmm = originX;
    grid.originymm = originY;
    grid.originzmm = originZ;
    grid.pitchxmm = 0.1;
    grid.pitchymm = 0.2;
    grid.layerthicknessmm = 0.05;
    return grid;
}

slicer_core::SceneInstanceRaster MakeRaster(
    const std::string& instanceId,
    const slicer_core::SlicePipelineMode mode,
    const slicer_core::SceneRasterGrid& grid,
    const std::array<std::uint8_t, 4>& rgbw)
{
    const slicer_core::SceneRasterIdentity identity =
        MakeIdentity(instanceId, mode);
    slicer_core::SceneInstanceRaster raster;
    raster.sceneid = identity.sceneid;
    raster.modelid = identity.modelid;
    raster.instanceid = identity.instanceid;
    raster.scenerevision = identity.scenerevision;
    raster.transformrevision = identity.transformrevision;
    raster.admittedtransformrevision =
        identity.admittedtransformrevision;
    raster.transformhash = identity.transformhash;
    raster.admittedtransformhash =
        identity.admittedtransformhash;
    raster.visible = identity.visible;
    raster.admitted = identity.admitted;
    raster.effectivepipelinemode =
        identity.effectivepipelinemode;
    raster.localgrid = grid;
    raster.protocol = slicer_core::FixedSceneRasterProtocol();

    const std::size_t pixelCount =
        static_cast<std::size_t>(grid.widthpx)
        * static_cast<std::size_t>(grid.heightpx);
    for (int layerIndex{0}; layerIndex < grid.layercount; ++layerIndex)
    {
        slicer_core::SceneInstanceRasterLayer layer;
        layer.layerindex = layerIndex;
        layer.zmm = grid.originzmm
            + (static_cast<double>(layerIndex) + 0.5)
                * grid.layerthicknessmm;
        layer.output.layerIndex = layerIndex;
        layer.output.zMm = layer.zmm;
        layer.output.widthPx = grid.widthpx;
        layer.output.heightPx = grid.heightpx;
        layer.output.channelOrder = raster.protocol.channel_order;
        layer.output.channels.assign(
            pixelCount * kChannelCount,
            raster.protocol.empty_value);
        layer.modelownership.assign(pixelCount, 0U);
        layer.modelvarnishownership.assign(pixelCount, 0U);
        layer.outervarnishownership.assign(pixelCount, 0U);
        layer.supportownership.assign(pixelCount, 0U);
        const std::size_t pixel = PixelIndex(grid.widthpx, 0, 0);
        layer.modelownership.at(pixel) = 1U;
        for (std::size_t channel{0U}; channel < rgbw.size(); ++channel)
        {
            layer.output.channels.at(
                ChannelIndex(grid.widthpx, 0, 0, channel)) =
                rgbw.at(channel);
        }
        raster.layers.push_back(std::move(layer));
    }
    return raster;
}

slicer_core::SceneCollisionResult MakeAdmission(
    const std::vector<slicer_core::SceneInstanceRaster>& rasters)
{
    slicer_core::SceneCollisionResult admission;
    admission.sceneid = "scene";
    admission.sourcescenerevision = 17U;
    admission.purpose =
        slicer_core::SceneValidationPurpose::FunctionalFixture;
    admission.scenestatus =
        slicer_core::SceneCollisionStatus::Passed;
    admission.functionalallowed = true;
    for (const slicer_core::SceneInstanceRaster& raster : rasters)
    {
        slicer_core::SceneCollisionInstanceResult instance;
        instance.modelid = raster.modelid;
        instance.instanceid = raster.instanceid;
        instance.visible = raster.visible;
        instance.skippedhidden = !raster.visible;
        instance.admissionstatus =
            slicer_core::SceneInstanceAdmissionStatus::Admitted;
        instance.boundsvalid = true;
        instance.inbounds = true;
        instance.transformrevision = raster.transformrevision;
        instance.transformhash = raster.transformhash;
        admission.instances.push_back(std::move(instance));
    }
    admission.statistics.totalinstancecount = rasters.size();
    admission.statistics.visibleinstancecount = rasters.size();
    return admission;
}

void BindLegacyInstance(
    slicer_core::LegacySceneLayerAdapterRequest& request,
    const double translateX,
    const double translateY)
{
    const slicer_core::SliceConfig config =
        slicer_core::load_slice_config(request.configpath);
    const slicer_core::ModelReport source =
        slicer_core::load_model_report(
            config,
            request.configpath.parent_path());
    request.instance.instanceid = request.identity.instanceid;
    request.instance.modelid = request.identity.modelid;
    request.instance.sourcetransformidentity =
        source.model_path.generic_string();
    request.instance.transform.translatexmm = translateX;
    request.instance.transform.translateymm = translateY;
    request.instance.transformrevision =
        request.identity.transformrevision;
    request.instance.sourcebboxmm = source.bbox_mm;
    request.instance.effectivebboxmm = source.bbox_mm;
    request.instance.effectivebboxmm.min.x += translateX;
    request.instance.effectivebboxmm.max.x += translateX;
    request.instance.effectivebboxmm.min.y += translateY;
    request.instance.effectivebboxmm.max.y += translateY;
    const slicer_core::ModelTransformHashResult transformHash =
        slicer_core::ComputeModelTransformHash(
            request.instance.transform,
            request.instance.sourcetransformidentity,
            request.instance.instanceid,
            request.instance.modelid);
    request.identity.transformhash = transformHash.hash;
    request.identity.admittedtransformhash = transformHash.hash;
}

bool LegacyAdapterExtractsMemoryLayersWithoutOutput()
{
    slicer_core::LegacySceneLayerAdapterRequest request;
    request.configpath =
        std::filesystem::path(SLICESOFT_SOURCE_DIR)
        / "samples/configs/golden/material_process_top2_fixture.json";
    request.identity = MakeIdentity(
        "legacy",
        slicer_core::SlicePipelineMode::Legacy);
    BindLegacyInstance(request, 0.0, 0.0);
    const slicer_core::SliceConfig config =
        slicer_core::load_slice_config(request.configpath);
    const std::filesystem::path packageDirectory =
        std::filesystem::absolute(config.output.package_dir);
    const bool packageExisted =
        std::filesystem::exists(packageDirectory);
    const std::vector<std::string> before =
        SnapshotDirectory(packageDirectory);

    const slicer_core::SceneRasterAdapterResult result =
        slicer_core::AdaptLegacySceneLayers(request);
    const bool packageExistsAfter =
        std::filesystem::exists(packageDirectory);
    const std::vector<std::string> after =
        SnapshotDirectory(packageDirectory);
    if (!ExpectTrue(result.IsValid(), "Legacy adapter result is valid"))
    {
        if (result.error.has_value())
        {
            std::cerr << "  "
                      << slicer_core::SceneRasterErrorCodeName(
                             result.error->code)
                      << ": " << result.error->message << '\n';
        }
        return false;
    }

    std::size_t modelPixels{0U};
    for (const slicer_core::SceneInstanceRasterLayer& layer :
         result.raster.layers)
    {
        for (const std::uint8_t value : layer.modelownership)
        {
            modelPixels += value != 0U ? 1U : 0U;
        }
    }
    return ExpectTrue(
               !result.productionoutputwritten,
               "Legacy adapter writes no package")
        && ExpectTrue(
            result.raster.effectivepipelinemode
                == slicer_core::SlicePipelineMode::Legacy,
            "Legacy mode is retained")
        && ExpectTrue(
            result.raster.layers.size()
                == static_cast<std::size_t>(
                    result.raster.localgrid.layercount),
            "Legacy adapter returns every layer")
        && ExpectTrue(
            packageExisted == packageExistsAfter
                && before == after,
            "Legacy adapter does not create or modify package files")
        && ExpectTrue(modelPixels > 0U, "Legacy model ownership is retained");
}

bool LegacyAdapterAppliesAdmittedInstanceTransform()
{
    slicer_core::LegacySceneLayerAdapterRequest baselineRequest;
    baselineRequest.configpath =
        std::filesystem::path(SLICESOFT_SOURCE_DIR)
        / "samples/configs/golden/material_process_top2_fixture.json";
    baselineRequest.identity = MakeIdentity(
        "legacy-transform",
        slicer_core::SlicePipelineMode::Legacy);
    BindLegacyInstance(baselineRequest, 0.0, 0.0);
    const slicer_core::SceneRasterAdapterResult baseline =
        slicer_core::AdaptLegacySceneLayers(baselineRequest);

    slicer_core::LegacySceneLayerAdapterRequest translatedRequest =
        baselineRequest;
    ++translatedRequest.identity.transformrevision;
    ++translatedRequest.identity.admittedtransformrevision;
    BindLegacyInstance(translatedRequest, 1.3, 2.4);
    const slicer_core::SceneRasterAdapterResult translated =
        slicer_core::AdaptLegacySceneLayers(translatedRequest);

    const bool sameLayers =
        baseline.raster.layers.size() == translated.raster.layers.size()
        && std::equal(
            baseline.raster.layers.begin(),
            baseline.raster.layers.end(),
            translated.raster.layers.begin(),
            [](const slicer_core::SceneInstanceRasterLayer& left,
               const slicer_core::SceneInstanceRasterLayer& right)
            {
                return left.output.channels == right.output.channels
                    && left.modelownership == right.modelownership
                    && left.outervarnishownership
                        == right.outervarnishownership
                    && left.supportownership
                        == right.supportownership;
            });
    return ExpectTrue(
               baseline.IsValid() && translated.IsValid(),
               "Legacy transformed adapter outputs are valid")
        && ExpectTrue(
            std::abs(
                translated.raster.localgrid.originxmm
                - baseline.raster.localgrid.originxmm
                - 1.3) < 1.0e-9
                && std::abs(
                    translated.raster.localgrid.originymm
                    - baseline.raster.localgrid.originymm
                    - 2.4) < 1.0e-9,
            "Legacy local grid follows admitted XY translation")
        && ExpectTrue(
            baseline.raster.localgrid.widthpx
                    == translated.raster.localgrid.widthpx
                && baseline.raster.localgrid.heightpx
                    == translated.raster.localgrid.heightpx
                && baseline.raster.localgrid.layercount
                    == translated.raster.localgrid.layercount
                && sameLayers,
            "translation preserves local layer bytes and dimensions");
}

bool LegacyAdapterRejectsMismatchedSourceIdentity()
{
    slicer_core::LegacySceneLayerAdapterRequest request;
    request.configpath =
        std::filesystem::path(SLICESOFT_SOURCE_DIR)
        / "samples/configs/golden/material_process_top2_fixture.json";
    request.identity = MakeIdentity(
        "legacy-source",
        slicer_core::SlicePipelineMode::Legacy);
    BindLegacyInstance(request, 0.0, 0.0);
    request.instance.sourcetransformidentity =
        (std::filesystem::path(SLICESOFT_SOURCE_DIR)
         / "samples/models/fixture_cube.obj").generic_string();
    const slicer_core::ModelTransformHashResult transformHash =
        slicer_core::ComputeModelTransformHash(
            request.instance.transform,
            request.instance.sourcetransformidentity,
            request.instance.instanceid,
            request.instance.modelid);
    request.identity.transformhash = transformHash.hash;
    request.identity.admittedtransformhash = transformHash.hash;

    const slicer_core::SceneRasterAdapterResult result =
        slicer_core::AdaptLegacySceneLayers(request);
    return ExpectTrue(
        result.error.has_value()
            && result.error->code
                == slicer_core::SceneRasterErrorCode::ProducerFailed,
        "Legacy adapter rejects source identity that differs from config model");
}

bool GlobalAdapterWrapsExistingWriterReadyLayers()
{
    const slicer_core::SceneRasterGrid grid =
        MakeGrid(2, 1, 1, 0.0, 0.0);
    slicer_core::GlobalSurfaceShellProductionLayerAdapterResult source;
    source.available = true;
    source.fullClosurePass = true;
    source.status = "ready_for_writer";
    source.widthPx = grid.widthpx;
    source.heightPx = grid.heightpx;
    source.layerCount = grid.layercount;
    source.protocol = slicer_core::FixedSceneRasterProtocol();

    slicer_core::GlobalSurfaceShellProductionLayer layer;
    layer.output.layerIndex = 0;
    layer.output.zMm = 0.025;
    layer.output.widthPx = grid.widthpx;
    layer.output.heightPx = grid.heightpx;
    layer.output.channels.assign(2U * kChannelCount, 255U);
    layer.output.channels.at(0U) = 20U;
    layer.semantic.layerIndex = 0;
    layer.semantic.zMm = 0.025;
    layer.semantic.widthPx = grid.widthpx;
    layer.semantic.heightPx = grid.heightpx;
    layer.semantic.modelMaterialMask = {1U, 0U};
    layer.semantic.outerVarnishShellMask = {0U, 0U};
    layer.semantic.supportFillMask = {0U, 0U};
    source.layers.push_back(layer);

    slicer_core::GlobalSceneLayerAdapterRequest request;
    request.identity = MakeIdentity(
        "global",
        slicer_core::SlicePipelineMode::GlobalSurfaceShell);
    request.localgrid = grid;
    request.source = &source;
    const slicer_core::SceneRasterAdapterResult result =
        slicer_core::AdaptGlobalSceneLayers(request);

    return ExpectTrue(result.IsValid(), "Global wrapper result is valid")
        && ExpectTrue(
            result.raster.layers.at(0).output.channels
                == source.layers.at(0).output.channels,
            "Global writer-ready bytes are retained")
        && ExpectTrue(
            result.raster.layers.at(0).modelownership
                == source.layers.at(0).semantic.modelMaterialMask,
            "Global model ownership is retained")
        && ExpectTrue(
            result.raster.effectivepipelinemode
                == slicer_core::SlicePipelineMode::GlobalSurfaceShell,
            "Global mode is retained")
        && ExpectTrue(
            !result.productionoutputwritten,
            "Global wrapper writes no package");
}

bool GlobalAdapterRejectsProtocolMutation()
{
    const slicer_core::SceneRasterGrid grid =
        MakeGrid(1, 1, 1, 0.0, 0.0);
    slicer_core::GlobalSurfaceShellProductionLayerAdapterResult source;
    source.available = true;
    source.fullClosurePass = true;
    source.status = "ready_for_writer";
    source.widthPx = 1;
    source.heightPx = 1;
    source.layerCount = 1;
    source.protocol = slicer_core::FixedSceneRasterProtocol();
    source.protocol.schema = "mutated";

    slicer_core::GlobalSurfaceShellProductionLayer layer;
    layer.output.layerIndex = 0;
    layer.output.zMm = 0.025;
    layer.output.widthPx = 1;
    layer.output.heightPx = 1;
    layer.output.channels = {0U, 255U, 255U, 255U, 255U, 255U};
    layer.semantic.layerIndex = 0;
    layer.semantic.zMm = 0.025;
    layer.semantic.widthPx = 1;
    layer.semantic.heightPx = 1;
    layer.semantic.modelMaterialMask = {1U};
    layer.semantic.outerVarnishShellMask = {0U};
    layer.semantic.supportFillMask = {0U};
    source.layers.push_back(std::move(layer));

    slicer_core::GlobalSceneLayerAdapterRequest request;
    request.identity = MakeIdentity(
        "global-protocol",
        slicer_core::SlicePipelineMode::GlobalSurfaceShell);
    request.localgrid = grid;
    request.source = &source;
    const slicer_core::SceneRasterAdapterResult result =
        slicer_core::AdaptGlobalSceneLayers(request);
    return ExpectTrue(
        result.error.has_value()
            && result.error->code
                == slicer_core::SceneRasterErrorCode::ProtocolMismatch,
        "Global adapter rejects protocol mutation");
}

bool GlobalAdapterRejectsMalformedLayerWithoutThrowing()
{
    const slicer_core::SceneRasterGrid grid =
        MakeGrid(1, 1, 1, 0.0, 0.0);
    slicer_core::GlobalSurfaceShellProductionLayerAdapterResult source;
    source.available = true;
    source.fullClosurePass = true;
    source.status = "ready_for_writer";
    source.widthPx = 1;
    source.heightPx = 1;
    source.layerCount = 1;
    source.protocol = slicer_core::FixedSceneRasterProtocol();

    slicer_core::GlobalSurfaceShellProductionLayer layer;
    layer.output.layerIndex = 0;
    layer.output.zMm = 0.025;
    layer.output.widthPx = 1;
    layer.output.heightPx = 1;
    layer.output.channels = {0U};
    layer.semantic.layerIndex = 0;
    layer.semantic.zMm = 0.025;
    layer.semantic.widthPx = 1;
    layer.semantic.heightPx = 1;
    layer.semantic.modelMaterialMask = {1U};
    layer.semantic.outerVarnishShellMask = {0U};
    layer.semantic.supportFillMask = {0U};
    source.layers.push_back(std::move(layer));

    slicer_core::GlobalSceneLayerAdapterRequest request;
    request.identity = MakeIdentity(
        "global-malformed",
        slicer_core::SlicePipelineMode::GlobalSurfaceShell);
    request.localgrid = grid;
    request.source = &source;
    try
    {
        const slicer_core::SceneRasterAdapterResult result =
            slicer_core::AdaptGlobalSceneLayers(request);
        return ExpectTrue(
            result.error.has_value()
                && result.error->code
                    == slicer_core::SceneRasterErrorCode::
                        LayerSizeInvalid,
            "malformed Global layer returns a stable size error");
    }
    catch (...)
    {
        return ExpectTrue(
            false,
            "malformed Global layer must not throw");
    }
}

bool OrchestratorUsesBedZeroAndCheckedExtents()
{
    std::vector<slicer_core::SceneInstanceRaster> positiveZ{
        MakeRaster(
            "raised",
            slicer_core::SlicePipelineMode::Legacy,
            MakeGrid(1, 1, 1, 0.0, 0.0, 0.10),
            {11U, 22U, 33U, 255U}),
    };
    slicer_core::MultiModelLayerComposeRequest request;
    request.admission = MakeAdmission(positiveZ);
    request.currentscenerevision = 17U;
    request.effectivepipelinemode =
        slicer_core::SlicePipelineMode::Legacy;
    request.instances = positiveZ;
    const slicer_core::SceneLayerComposeResult raised =
        slicer_core::ComposeAdmittedSceneRasters(request);

    request.instances = {
        MakeRaster(
            "negative",
            slicer_core::SlicePipelineMode::Legacy,
            MakeGrid(1, 1, 1, 0.0, 0.0, -0.05),
            {11U, 22U, 33U, 255U}),
    };
    request.admission = MakeAdmission(request.instances);
    const slicer_core::SceneLayerComposeResult negative =
        slicer_core::ComposeAdmittedSceneRasters(request);

    request.instances = {
        MakeRaster(
            "origin",
            slicer_core::SlicePipelineMode::Legacy,
            MakeGrid(1, 1, 1, 0.0, 0.0),
            {11U, 22U, 33U, 255U}),
        MakeRaster(
            "overflow",
            slicer_core::SlicePipelineMode::Legacy,
            MakeGrid(
                1,
                1,
                1,
                static_cast<double>(
                    std::numeric_limits<int>::max()) * 0.1,
                0.0),
            {44U, 55U, 66U, 255U}),
    };
    request.admission = MakeAdmission(request.instances);
    const slicer_core::SceneLayerComposeResult overflow =
        slicer_core::ComposeAdmittedSceneRasters(request);

    return ExpectTrue(
               raised.IsValid()
                   && raised.grid.originzmm == 0.0
                   && raised.grid.layercount == 3
                   && raised.layers.at(2U).channels.at(0U) == 11U,
               "shared scene grid is anchored at bed Z zero")
        && ExpectTrue(
            negative.error.has_value()
                && negative.error->code
                    == slicer_core::SceneRasterErrorCode::
                        OffsetNotIntegral,
            "negative local Z fails closed")
        && ExpectTrue(
            overflow.error.has_value()
                && overflow.error->code
                    == slicer_core::SceneRasterErrorCode::GridInvalid,
            "aligned extent integer overflow fails closed");
}

bool OrchestratorRejectsStaleTransformEvidence()
{
    std::vector<slicer_core::SceneInstanceRaster> rasters{
        MakeRaster(
            "stale",
            slicer_core::SlicePipelineMode::Legacy,
            MakeGrid(1, 1, 1, 0.0, 0.0),
            {11U, 22U, 33U, 255U}),
    };
    slicer_core::MultiModelLayerComposeRequest request;
    request.admission = MakeAdmission(rasters);
    request.currentscenerevision = 17U;
    request.effectivepipelinemode =
        slicer_core::SlicePipelineMode::Legacy;
    request.instances = rasters;
    request.instances.front().transformrevision += 1U;
    request.instances.front().admittedtransformrevision += 1U;
    request.instances.front().transformhash = "new-transform";

    const slicer_core::SceneLayerComposeResult result =
        slicer_core::ComposeAdmittedSceneRasters(request);
    return ExpectTrue(
        result.error.has_value()
            && result.error->code
                == slicer_core::SceneRasterErrorCode::RevisionStale,
        "raster transform cannot bypass independent admission evidence");
}

bool OrchestratorRejectsDuplicateAdmissionIdentity()
{
    std::vector<slicer_core::SceneInstanceRaster> rasters{
        MakeRaster(
            "duplicate",
            slicer_core::SlicePipelineMode::Legacy,
            MakeGrid(1, 1, 1, 0.0, 0.0),
            {11U, 22U, 33U, 255U}),
    };
    slicer_core::MultiModelLayerComposeRequest request;
    request.admission = MakeAdmission(rasters);
    request.admission.instances.push_back(
        request.admission.instances.front());
    request.admission.statistics.totalinstancecount = 2U;
    request.admission.statistics.visibleinstancecount = 2U;
    request.currentscenerevision = 17U;
    request.effectivepipelinemode =
        slicer_core::SlicePipelineMode::Legacy;
    request.instances = rasters;

    const slicer_core::SceneLayerComposeResult result =
        slicer_core::ComposeAdmittedSceneRasters(request);
    return ExpectTrue(
        result.error.has_value()
            && result.error->code
                == slicer_core::SceneRasterErrorCode::
                    InstanceIdentityInvalid,
        "duplicate admission instance identity fails closed");
}

bool OrchestratorBuildsSharedGridAndComposes()
{
    std::vector<slicer_core::SceneInstanceRaster> rasters{
        MakeRaster(
            "left",
            slicer_core::SlicePipelineMode::Legacy,
            MakeGrid(2, 2, 1, 1.0, 2.0),
            {10U, 20U, 30U, 255U}),
        MakeRaster(
            "right",
            slicer_core::SlicePipelineMode::Legacy,
            MakeGrid(1, 1, 2, 1.4, 2.4),
            {40U, 50U, 60U, 255U}),
    };
    slicer_core::MultiModelLayerComposeRequest request;
    request.admission = MakeAdmission(rasters);
    request.currentscenerevision = 17U;
    request.effectivepipelinemode =
        slicer_core::SlicePipelineMode::Legacy;
    request.instances = rasters;

    const slicer_core::SceneLayerComposeResult result =
        slicer_core::ComposeAdmittedSceneRasters(request);
    return ExpectTrue(result.IsValid(), "admitted scene composes")
        && ExpectTrue(
            result.effectivepipelinemode
                == slicer_core::SlicePipelineMode::Legacy,
            "effective pipeline mode is retained for the writer boundary")
        && ExpectTrue(
            result.grid.widthpx == 5
                && result.grid.heightpx == 3
                && result.grid.layercount == 2,
            "global grid is the aligned union")
        && ExpectTrue(
            result.layers.at(0).channels.at(
                ChannelIndex(result.grid.widthpx, 4, 2, 0U))
                == 40U,
            "right instance maps to global offset");
}

bool MixedModeAndMissingAdmissionFailClosed()
{
    std::vector<slicer_core::SceneInstanceRaster> rasters{
        MakeRaster(
            "legacy",
            slicer_core::SlicePipelineMode::Legacy,
            MakeGrid(1, 1, 1, 0.0, 0.0),
            {1U, 2U, 3U, 255U}),
        MakeRaster(
            "global",
            slicer_core::SlicePipelineMode::GlobalSurfaceShell,
            MakeGrid(1, 1, 1, 0.2, 0.0),
            {4U, 5U, 6U, 255U}),
    };
    slicer_core::MultiModelLayerComposeRequest request;
    request.admission = MakeAdmission(rasters);
    request.currentscenerevision = 17U;
    request.effectivepipelinemode =
        slicer_core::SlicePipelineMode::Legacy;
    request.instances = rasters;
    const slicer_core::SceneLayerComposeResult mixed =
        slicer_core::ComposeAdmittedSceneRasters(request);

    request.instances.pop_back();
    request.admission.instances.clear();
    const slicer_core::SceneLayerComposeResult missing =
        slicer_core::ComposeAdmittedSceneRasters(request);
    return ExpectTrue(
               mixed.error.has_value()
                   && mixed.error->code
                       == slicer_core::SceneRasterErrorCode::
                           PipelineModeMismatch,
               "mixed modes fail closed")
        && ExpectTrue(
            missing.error.has_value()
                && missing.error->code
                    == slicer_core::SceneRasterErrorCode::
                        AdmissionRequired,
            "missing per-instance admission fails closed")
        && ExpectTrue(
            mixed.layers.empty() && missing.layers.empty(),
            "failed orchestration returns no partial layers");
}

}  // namespace

int main()
{
    const std::vector<std::pair<std::string, bool (*)()>> tests{
        {"legacy_adapter_extracts_memory_layers_without_output", LegacyAdapterExtractsMemoryLayersWithoutOutput},
        {"legacy_adapter_applies_admitted_instance_transform", LegacyAdapterAppliesAdmittedInstanceTransform},
        {"legacy_adapter_rejects_mismatched_source_identity", LegacyAdapterRejectsMismatchedSourceIdentity},
        {"global_adapter_wraps_existing_writer_ready_layers", GlobalAdapterWrapsExistingWriterReadyLayers},
        {"global_adapter_rejects_protocol_mutation", GlobalAdapterRejectsProtocolMutation},
        {"global_adapter_rejects_malformed_layer_without_throwing", GlobalAdapterRejectsMalformedLayerWithoutThrowing},
        {"orchestrator_builds_shared_grid_and_composes", OrchestratorBuildsSharedGridAndComposes},
        {"orchestrator_uses_bed_zero_and_checked_extents", OrchestratorUsesBedZeroAndCheckedExtents},
        {"orchestrator_rejects_stale_transform_evidence", OrchestratorRejectsStaleTransformEvidence},
        {"orchestrator_rejects_duplicate_admission_identity", OrchestratorRejectsDuplicateAdmissionIdentity},
        {"mixed_mode_and_missing_admission_fail_closed", MixedModeAndMissingAdmissionFailClosed},
    };

    bool passed{true};
    for (const auto& test : tests)
    {
        const bool current = test.second();
        std::cout << (current ? "PASS: " : "FAIL: ")
                  << test.first << '\n';
        passed = current && passed;
    }
    return passed ? 0 : 1;
}
