#include "slicer_core/material/MaterialChannelComposer.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <utility>
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

std::uint8_t ChannelAt(
    const slicer_core::MaterialChannelComposerResult& result,
    const std::size_t pixel,
    const slicer_core::MaterialChannelOffset channel)
{
    return result.channels.at(pixel * static_cast<std::size_t>(slicer_core::MaterialChannelCount())
                              + static_cast<std::size_t>(channel));
}

bool EmptyVoxelComposesToEmpty()
{
    slicer_core::MaterialChannelComposerInput input;
    input.width = 1;
    input.height = 1;
    const slicer_core::MaterialChannelComposerResult result = slicer_core::ComposeMaterialChannels(input);
    return ExpectTrue(result.error.empty(), "empty compose error")
        && ExpectTrue(result.channels.size() == 6U, "empty compose channel count")
        && ExpectTrue(result.stats.empty_pixels == 1, "empty pixel count")
        && ExpectTrue(ChannelAt(result, 0, slicer_core::MaterialChannelOffset::R) == 255U, "empty R")
        && ExpectTrue(ChannelAt(result, 0, slicer_core::MaterialChannelOffset::V) == 255U, "empty V");
}

bool ModelVoxelWritesRgb()
{
    slicer_core::MaterialChannelComposerInput input;
    input.width = 1;
    input.height = 1;
    input.model_mask = {1};
    input.model_rgb = {10, 20, 30};
    const slicer_core::MaterialChannelComposerResult result = slicer_core::ComposeMaterialChannels(input);
    return ExpectTrue(ChannelAt(result, 0, slicer_core::MaterialChannelOffset::R) == 10U, "model R")
        && ExpectTrue(ChannelAt(result, 0, slicer_core::MaterialChannelOffset::G) == 20U, "model G")
        && ExpectTrue(ChannelAt(result, 0, slicer_core::MaterialChannelOffset::B) == 30U, "model B")
        && ExpectTrue(ChannelAt(result, 0, slicer_core::MaterialChannelOffset::S) == 255U, "model S empty");
}

bool SupportOnlyWritesS()
{
    slicer_core::MaterialChannelComposerInput input;
    input.width = 1;
    input.height = 1;
    input.support_mask = {1};
    const slicer_core::MaterialChannelComposerResult result = slicer_core::ComposeMaterialChannels(input);
    return ExpectTrue(ChannelAt(result, 0, slicer_core::MaterialChannelOffset::S) == 0U, "support S")
        && ExpectTrue(ChannelAt(result, 0, slicer_core::MaterialChannelOffset::R) == 255U, "support R empty")
        && ExpectTrue(result.stats.support_pixels == 1, "support stats");
}

bool ModelSupportConflictUsesModelPriority()
{
    slicer_core::MaterialChannelComposerInput input;
    input.width = 1;
    input.height = 1;
    input.support_mask = {1};
    input.model_mask = {1};
    input.model_rgb = {7, 8, 9};
    const slicer_core::MaterialChannelComposerResult result = slicer_core::ComposeMaterialChannels(input);
    return ExpectTrue(ChannelAt(result, 0, slicer_core::MaterialChannelOffset::R) == 7U, "conflict model R")
        && ExpectTrue(ChannelAt(result, 0, slicer_core::MaterialChannelOffset::S) == 255U, "conflict support cleared")
        && ExpectTrue(result.stats.model_support_conflict_pixels == 1, "conflict stats");
}

bool SurfaceRgbDoesNotAffectSV()
{
    slicer_core::MaterialChannelComposerInput input;
    input.width = 1;
    input.height = 1;
    input.support_mask = {1};
    input.surface_shell_mask = {1};
    input.varnish_mask = {1};
    input.surface_rgb = {{{40, 50, 60}}};
    const slicer_core::MaterialChannelComposerResult result = slicer_core::ComposeMaterialChannels(input);
    return ExpectTrue(ChannelAt(result, 0, slicer_core::MaterialChannelOffset::R) == 40U, "surface R")
        && ExpectTrue(ChannelAt(result, 0, slicer_core::MaterialChannelOffset::S) == 0U, "surface keeps S")
        && ExpectTrue(ChannelAt(result, 0, slicer_core::MaterialChannelOffset::V) == 0U, "surface keeps V");
}

bool WhiteAndVarnishWriteChannels()
{
    slicer_core::MaterialChannelComposerInput input;
    input.width = 1;
    input.height = 1;
    input.white_mask = {1};
    input.varnish_mask = {1};
    const slicer_core::MaterialChannelComposerResult result = slicer_core::ComposeMaterialChannels(input);
    return ExpectTrue(ChannelAt(result, 0, slicer_core::MaterialChannelOffset::W) == 0U, "white W")
        && ExpectTrue(ChannelAt(result, 0, slicer_core::MaterialChannelOffset::V) == 0U, "varnish V");
}

bool ChannelOrderIsFixedRgbwsv()
{
    slicer_core::MaterialChannelComposerInput input;
    input.width = 1;
    input.height = 1;
    const slicer_core::MaterialChannelComposerResult result = slicer_core::ComposeMaterialChannels(input);
    return ExpectTrue(slicer_core::MaterialChannelCount() == 6, "channel count")
        && ExpectTrue(result.channel_order.at(0) == "R", "channel R")
        && ExpectTrue(result.channel_order.at(1) == "G", "channel G")
        && ExpectTrue(result.channel_order.at(2) == "B", "channel B")
        && ExpectTrue(result.channel_order.at(3) == "W", "channel W")
        && ExpectTrue(result.channel_order.at(4) == "S", "channel S")
        && ExpectTrue(result.channel_order.at(5) == "V", "channel V");
}

}  // namespace

int main()
{
    const std::vector<std::pair<std::string, bool (*)()>> tests{
        {"empty_voxel_composes_to_empty", EmptyVoxelComposesToEmpty},
        {"model_voxel_writes_rgb", ModelVoxelWritesRgb},
        {"support_only_writes_s", SupportOnlyWritesS},
        {"model_support_conflict_uses_model_priority", ModelSupportConflictUsesModelPriority},
        {"surface_rgb_does_not_affect_s_v", SurfaceRgbDoesNotAffectSV},
        {"white_and_varnish_write_channels", WhiteAndVarnishWriteChannels},
        {"channel_order_is_fixed_rgbwsv", ChannelOrderIsFixedRgbwsv},
    };

    for (const auto& test : tests)
    {
        std::cout << "RUN " << test.first << std::endl;
        if (!test.second())
        {
            return 1;
        }
        std::cout << "PASS " << test.first << '\n';
    }

    std::cout << "Material channel composer unit tests complete.\n";
    return 0;
}
