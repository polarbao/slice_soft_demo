#include "slicer_core/pipeline/GlobalSurfaceShellMaterialEvidence.h"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace
{

constexpr std::size_t kChannelCount{6U};
constexpr std::size_t kSupportChannel{4U};
constexpr std::size_t kVarnishChannel{5U};

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
    }
    return condition;
}

std::size_t PixelIndex(
    const int width,
    const int x,
    const int y)
{
    return static_cast<std::size_t>(y * width + x);
}

slicer_core::TextureFillPartitionRasterMappingResult MakeMapping()
{
    slicer_core::TextureFillPartitionRasterMappingResult mapping;
    mapping.available = true;
    mapping.status = "diagnostic";
    mapping.grid.width = 7;
    mapping.grid.height = 7;
    mapping.grid.depth = 4;
    mapping.grid.pixelPitchXMm = 0.05;
    mapping.grid.pixelPitchYMm = 0.05;
    mapping.grid.layerThicknessMm = 0.05;
    mapping.stats.partitionPass = true;

    const std::size_t pixelCount{49U};
    mapping.layers.resize(4U);
    for (int layerIndex{0}; layerIndex < 4; ++layerIndex)
    {
        auto& layer = mapping.layers.at(static_cast<std::size_t>(layerIndex));
        layer.layerIndex = layerIndex;
        layer.zMm = (static_cast<double>(layerIndex) + 0.5) * 0.05;
        layer.modelMask.assign(pixelCount, 0U);
        layer.textureSurfaceMask.assign(pixelCount, 0U);
        layer.modelFillMask.assign(pixelCount, 0U);
        layer.textureRgb.assign(pixelCount, {255U, 255U, 255U});
    }

    for (const int layerIndex : {1, 3})
    {
        auto& layer = mapping.layers.at(static_cast<std::size_t>(layerIndex));
        const std::size_t center = PixelIndex(7, 3, 3);
        layer.modelMask.at(center) = 1U;
        layer.modelFillMask.at(center) = 1U;
    }
    return mapping;
}

slicer_core::SliceConfig MakeMaterialParityConfig()
{
    slicer_core::SliceConfig config;
    config.model_fill.value = 0U;
    config.support.enabled = true;
    config.support.mode = "bottom_projection";
    config.support.placement = "lower";
    config.support.placement_explicit = true;
    config.support.value = 0U;
    config.support.internal_void.enabled = true;
    config.support.internal_void.min_area_px = 1;
    config.surface_varnish.enabled = true;
    config.surface_varnish.outer_surface = true;
    config.surface_varnish.inner_surface = true;
    config.surface_varnish.thickness_px = 1;
    config.surface_varnish.value = 0U;
    config.outer_varnish.enabled = true;
    config.outer_varnish.thickness_mm = 0.05;
    config.outer_varnish.value = 0U;
    return config;
}

bool LowerSupportAndInternalVoidAreWrittenToS()
{
    const auto mapping = MakeMapping();
    const auto config = MakeMaterialParityConfig();
    const auto result =
        slicer_core::ComposeGlobalSurfaceShellMaterialEvidence(
            mapping,
            config);
    const std::size_t center = PixelIndex(7, 3, 3);

    return ExpectTrue(result.available, "material evidence is available")
        && ExpectTrue(
            result.layers.at(0U).supportFillMask.at(center) == 1U,
            "lower projection fills below the model")
        && ExpectTrue(
            result.layers.at(2U).internalVoidSupportMask.at(center) == 1U,
            "void between model intervals is classified as internal support")
        && ExpectTrue(
            result.layers.at(2U).channels.at(
                center * kChannelCount + kSupportChannel) == 0U,
            "support is written to S");
}

bool SurfaceAndOuterVarnishAreWrittenToV()
{
    const auto mapping = MakeMapping();
    const auto config = MakeMaterialParityConfig();
    const auto result =
        slicer_core::ComposeGlobalSurfaceShellMaterialEvidence(
            mapping,
            config);
    const std::size_t center = PixelIndex(7, 3, 3);
    const std::size_t neighbor = PixelIndex(7, 4, 3);

    return ExpectTrue(
               result.layers.at(1U).surfaceVarnishMask.at(center) == 1U,
               "surface varnish remains inside the model domain")
        && ExpectTrue(
            result.layers.at(1U).outerVarnishShellMask.at(neighbor) == 1U,
            "outer varnish expands outside the model in XY")
        && ExpectTrue(
            result.layers.at(1U).channels.at(
                center * kChannelCount + kVarnishChannel) == 0U,
            "surface varnish is written to V")
        && ExpectTrue(
            result.layers.at(1U).channels.at(
                neighbor * kChannelCount + kVarnishChannel) == 0U,
            "outer varnish is written to V");
}

bool MaterialPriorityRemovesSupportOverlap()
{
    const auto mapping = MakeMapping();
    const auto config = MakeMaterialParityConfig();
    const auto result =
        slicer_core::ComposeGlobalSurfaceShellMaterialEvidence(
            mapping,
            config);

    for (std::size_t layerIndex{0U};
         layerIndex < result.layers.size();
         ++layerIndex)
    {
        const auto& layer = result.layers.at(layerIndex);
        const auto& modelLayer = mapping.layers.at(layerIndex);
        for (std::size_t index{0U}; index < layer.supportFillMask.size(); ++index)
        {
            if (layer.supportFillMask.at(index) != 0U
                && (modelLayer.modelMask.at(index) != 0U
                    || layer.outerVarnishShellMask.at(index) != 0U))
            {
                return ExpectTrue(
                    false,
                    "support must not overlap model envelope or outer varnish");
            }
        }
    }
    return true;
}

}  // namespace

int main()
{
    const std::vector<std::pair<std::string, bool (*)()>> tests{
        {"lower_support_and_internal_void_are_written_to_s",
         LowerSupportAndInternalVoidAreWrittenToS},
        {"surface_and_outer_varnish_are_written_to_v",
         SurfaceAndOuterVarnishAreWrittenToV},
        {"material_priority_removes_support_overlap",
         MaterialPriorityRemovesSupportOverlap},
    };

    bool passed{true};
    for (const auto& test : tests)
    {
        std::cout << "RUN: " << test.first << '\n';
        const bool current = test.second();
        std::cout << (current ? "PASS: " : "FAIL: ")
                  << test.first << '\n';
        passed = current && passed;
    }
    return passed ? 0 : 1;
}
