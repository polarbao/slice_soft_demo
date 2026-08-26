#include "slicer_core/config.h"
#include "slicer_core/geometry/SceneModelTriangleMeshAdapter.h"
#include "slicer_core/materials/transfer/LegacyTransferChannelSession.h"
#include "slicer_core/materials/transfer/TransferChannelError.h"
#include "slicer_core/model.h"
#include "slicer_core/model/ModelLoadConfig.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace
{

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL " << message << '\n';
    }
    return condition;
}

std::filesystem::path Reality03()
{
#ifdef SLICESOFT_SOURCE_DIR
    return std::filesystem::path{SLICESOFT_SOURCE_DIR} / "model" / "obj" / "reality"
        / "finger_suoguo" / "03.obj";
#else
    return std::filesystem::path{"model"} / "obj" / "reality" / "finger_suoguo"
        / "03.obj";
#endif
}

std::filesystem::path RealityModel(const char* fileName)
{
    return Reality03().parent_path() / fileName;
}

slicer_core::TransferChannelPolicyConfig TransferPolicy()
{
    slicer_core::TransferChannelPolicyConfig policy;
    policy.enabled = true;
    policy.material_diffuse_rgb_values.push_back({255U, 220U, 198U});
    policy.topology.self_intersection_policy = "tolerate_closed_self_intersection";
    policy.topology.max_self_intersection_pairs = 64;
    return policy;
}

slicer_core::MaterialVolumeGrid CoarseGrid(const slicer_core::ModelReport& model)
{
    constexpr double pixelMm{0.50};
    constexpr double layerMm{0.038};
    slicer_core::MaterialVolumeGrid grid;
    grid.originXMm = model.bbox_mm.min.x;
    grid.originYMm = model.bbox_mm.min.y;
    grid.pixelSizeXMm = pixelMm;
    grid.pixelSizeYMm = pixelMm;
    grid.widthPx = std::max(
        1, static_cast<int>((model.bbox_mm.max.x - model.bbox_mm.min.x) / pixelMm));
    grid.heightPx = std::max(
        1, static_cast<int>((model.bbox_mm.max.y - model.bbox_mm.min.y) / pixelMm));
    grid.layerThicknessMm = layerMm;
    grid.layerCount = std::max(
        1, static_cast<int>((model.bbox_mm.max.z - model.bbox_mm.min.z) / layerMm));
    return grid;
}

slicer_core::RgbwsvProductionLayer SolidRgbLayer(
    const slicer_core::MaterialVolumeGrid& grid,
    const int layerIndex)
{
    slicer_core::RgbwsvProductionLayer layer;
    layer.layerIndex = layerIndex;
    layer.zMm = static_cast<double>(layerIndex) * grid.layerThicknessMm;
    layer.widthPx = grid.widthPx;
    layer.heightPx = grid.heightPx;
    layer.channels.assign(
        static_cast<std::size_t>(grid.widthPx) * grid.heightPx * 6U, 255U);
    for (std::size_t pixel{0U}; pixel < layer.channels.size() / 6U; ++pixel)
    {
        layer.channels[pixel * 6U] = 10U;
        layer.channels[pixel * 6U + 1U] = 20U;
        layer.channels[pixel * 6U + 2U] = 30U;
    }
    return layer;
}

bool Reality03ComposesExclusiveTransfer()
{
    const std::filesystem::path path = Reality03();
    if (!std::filesystem::exists(path))
    {
        std::cout << "SKIP 03.obj not present\n";
        return true;
    }
    slicer_core::ModelLoadConfig config;
    config.input.model_path = path;
    config.input.format = "obj";
    config.auto_orient.enabled = true;
    const slicer_core::ModelReport model =
        slicer_core::load_model_report(config, path.parent_path());
    const slicer_core::AdaptedTriangleMesh mesh =
        slicer_core::AdaptSceneModelToTriangleMesh(model);
    const slicer_core::MaterialVolumeGrid grid = CoarseGrid(model);
    slicer_core::LegacyTransferChannelSession session =
        slicer_core::BuildLegacyTransferChannelSession(
            TransferPolicy(), mesh, grid);
    const std::vector<std::uint8_t> modelMask(session.plan.columnCount, 1U);

    for (int layerIndex{0}; layerIndex < grid.layerCount; ++layerIndex)
    {
        const slicer_core::RgbwsvProductionLayer source =
            SolidRgbLayer(grid, layerIndex);
        const slicer_core::RgbwsvtProductionLayer result =
            slicer_core::ComposeLegacyTransferChannelLayer(
                session, source, modelMask);
        bool foundTransfer{false};
        for (std::size_t offset{0U}; offset < result.channels.size(); offset += 7U)
        {
            if (result.channels[offset + 6U] != 0U)
            {
                continue;
            }
            foundTransfer = true;
            if (!ExpectTrue(
                    std::all_of(
                        result.channels.begin() + static_cast<std::ptrdiff_t>(offset),
                        result.channels.begin() + static_cast<std::ptrdiff_t>(offset + 6U),
                        [](const std::uint8_t channel) { return channel == 255U; }),
                    "03 T pixels clear all six Legacy channels"))
            {
                return false;
            }
        }
        if (!foundTransfer)
        {
            continue;
        }
        return ExpectTrue(
            session.plan.material.materialName == "02",
            "03 session resolves material 02 by configured colour");
    }
    return ExpectTrue(false, "03 session must produce at least one T pixel");
}

bool MissingOptionalRegionPreservesLegacyBytes()
{
    slicer_core::AdaptedTriangleMesh mesh;
    mesh.material_infos.push_back(
        slicer_core::MaterialInfo{"01", {63U, 190U, 126U}, true});
    slicer_core::MaterialVolumeGrid grid;
    grid.widthPx = 2;
    grid.heightPx = 1;
    grid.layerCount = 1;
    grid.pixelSizeXMm = 1.0;
    grid.pixelSizeYMm = 1.0;
    grid.layerThicknessMm = 1.0;
    slicer_core::LegacyTransferChannelSession session =
        slicer_core::BuildLegacyTransferChannelSession(
            TransferPolicy(), mesh, grid);
    const slicer_core::RgbwsvProductionLayer source = SolidRgbLayer(grid, 0);
    const std::vector<std::uint8_t> modelMask(2U, 1U);
    const slicer_core::RgbwsvtProductionLayer result =
        slicer_core::ComposeLegacyTransferChannelLayer(
            session, source, modelMask);
    for (std::size_t pixel{0U}; pixel < modelMask.size(); ++pixel)
    {
        for (std::size_t channel{0U}; channel < 6U; ++channel)
        {
            if (!ExpectTrue(
                    result.channels[pixel * 7U + channel]
                        == source.channels[pixel * 6U + channel],
                    "no-region session preserves Legacy bytes"))
            {
                return false;
            }
        }
        if (!ExpectTrue(
                result.channels[pixel * 7U + 6U] == 255U,
                "no-region session writes empty T"))
        {
            return false;
        }
    }
    return true;
}

bool OpenRealityTransferFailsClosed(const char* fileName)
{
    const std::filesystem::path path = RealityModel(fileName);
    if (!std::filesystem::exists(path))
    {
        std::cout << "SKIP " << fileName << " not present\n";
        return true;
    }
    slicer_core::ModelLoadConfig config;
    config.input.model_path = path;
    config.input.format = "obj";
    config.auto_orient.enabled = true;
    const slicer_core::ModelReport model =
        slicer_core::load_model_report(config, path.parent_path());
    const slicer_core::AdaptedTriangleMesh mesh =
        slicer_core::AdaptSceneModelToTriangleMesh(model);
    slicer_core::TransferChannelPolicyConfig policy = TransferPolicy();
    policy.material_diffuse_rgb_values = {{255U, 255U, 0U}};
    try
    {
        (void)slicer_core::BuildLegacyTransferChannelSession(
            policy, mesh, CoarseGrid(model));
    }
    catch (const slicer_core::TransferChannelError& error)
    {
        return ExpectTrue(
            error.Code() == slicer_core::TransferChannelErrorCode::TopologyInvalid,
            std::string{fileName} + " open transfer region fails with topology error");
    }
    return ExpectTrue(false, std::string{fileName} + " open transfer region must fail closed");
}

}  // namespace

int main()
{
    int failures{0};
    failures += Reality03ComposesExclusiveTransfer() ? 0 : 1;
    failures += MissingOptionalRegionPreservesLegacyBytes() ? 0 : 1;
    failures += OpenRealityTransferFailsClosed("08.obj") ? 0 : 1;
    failures += OpenRealityTransferFailsClosed("09.obj") ? 0 : 1;
    if (failures != 0)
    {
        std::cerr << "FAIL LegacyTransferChannelSessionTests "
                  << failures << " case(s)\n";
        return 1;
    }
    std::cout << "PASS LegacyTransferChannelSessionTests 4/4\n";
    return 0;
}
