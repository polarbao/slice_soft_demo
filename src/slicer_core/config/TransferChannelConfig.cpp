#include "slicer_core/config/TransferChannelConfig.h"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string>
#include <vector>

namespace slicer_core
{
namespace
{

constexpr std::array<const char*, 6> kRgbwsvOrder{"R", "G", "B", "W", "S", "V"};
constexpr std::array<const char*, 7> kRgbwsvtOrder{"R", "G", "B", "W", "S", "V", "T"};

std::uint8_t ReadU8(
    const Json& object,
    const char* key,
    const std::uint8_t fallback)
{
    if (!object.contains(key))
    {
        return fallback;
    }
    const int value = object.at(key).as_int();
    if (value < 0 || value > 255)
    {
        throw std::runtime_error(std::string{"uint8 config field out of range: "} + key);
    }
    return static_cast<std::uint8_t>(value);
}

std::vector<std::array<std::uint8_t, 3>> ReadRgbArrayList(
    const Json& object,
    const char* key)
{
    if (!object.contains(key))
    {
        return {};
    }
    const Json& values = object.at(key);
    if (!values.is_array())
    {
        throw std::runtime_error(std::string{"expected RGB array list for field: "} + key);
    }
    std::vector<std::array<std::uint8_t, 3>> result;
    result.reserve(values.size());
    for (const Json& value : values.as_array())
    {
        if (!value.is_array() || value.size() != 3U)
        {
            throw std::runtime_error(std::string{"expected 3-value RGB entry for field: "} + key);
        }
        std::array<std::uint8_t, 3> rgb{};
        for (std::size_t index = 0U; index < rgb.size(); ++index)
        {
            const int channel = value.at(index).as_int();
            if (channel < 0 || channel > 255)
            {
                throw std::runtime_error(
                    std::string{"RGB array list value out of uint8 range: "} + key);
            }
            rgb[index] = static_cast<std::uint8_t>(channel);
        }
        result.push_back(rgb);
    }
    return result;
}

template <std::size_t Size>
bool HasExactChannelOrder(
    const std::vector<std::string>& actual,
    const std::array<const char*, Size>& expected)
{
    return actual.size() == expected.size()
        && std::equal(actual.begin(), actual.end(), expected.begin());
}

}  // namespace

void LoadTransferChannelPolicy(
    const Json& root,
    TransferChannelPolicyConfig& policy)
{
    if (!root.contains("transferChannelPolicy"))
    {
        return;
    }
    const Json& value = root.at("transferChannelPolicy");
    policy.enabled = value.value("enabled", policy.enabled);
    policy.match_source = value.value("matchSource", policy.match_source);
    policy.material_diffuse_rgb_values =
        ReadRgbArrayList(value, "materialDiffuseRgbValues");
    policy.missing_region = value.value("missingRegion", policy.missing_region);
    policy.multiple_matches = value.value("multipleMatches", policy.multiple_matches);
    policy.value = ReadU8(value, "value", policy.value);
    if (value.contains("topology"))
    {
        const Json& topology = value.at("topology");
        policy.topology.self_intersection_policy = topology.value(
            "selfIntersectionPolicy", policy.topology.self_intersection_policy);
        policy.topology.max_self_intersection_pairs = topology.value(
            "maxSelfIntersectionPairs", policy.topology.max_self_intersection_pairs);
    }
}

void ValidateTransferChannelConfiguration(
    const OutputConfig& output,
    const TransferChannelPolicyConfig& policy)
{
    if (!policy.enabled)
    {
        if (output.package_protocol != "p0.rgbwsv.2")
        {
            throw std::runtime_error(
                "output.packageProtocol must remain p0.rgbwsv.2 when transferChannelPolicy is disabled");
        }
        if (!HasExactChannelOrder(output.channel_order, kRgbwsvOrder))
        {
            throw std::runtime_error("P0 channelOrder must be exactly R G B W S V");
        }
        return;
    }
    if (output.package_protocol != "p0.rgbwsvt.1")
    {
        throw std::runtime_error(
            "transferChannelPolicy requires output.packageProtocol=p0.rgbwsvt.1");
    }
    if (!HasExactChannelOrder(output.channel_order, kRgbwsvtOrder))
    {
        throw std::runtime_error(
            "p0.rgbwsvt.1 channelOrder must be exactly R G B W S V T");
    }
    if (policy.match_source != "material_diffuse_rgb")
    {
        throw std::runtime_error(
            "transferChannelPolicy.matchSource must be material_diffuse_rgb");
    }
    if (policy.material_diffuse_rgb_values.empty())
    {
        throw std::runtime_error(
            "transferChannelPolicy.materialDiffuseRgbValues must not be empty");
    }
    std::vector<std::array<std::uint8_t, 3>> seenColours;
    for (const auto& colour : policy.material_diffuse_rgb_values)
    {
        if (std::find(seenColours.begin(), seenColours.end(), colour) != seenColours.end())
        {
            throw std::runtime_error(
                "transferChannelPolicy.materialDiffuseRgbValues must be unique");
        }
        seenColours.push_back(colour);
    }
    if (policy.missing_region != "allow_empty" && policy.missing_region != "fail_closed")
    {
        throw std::runtime_error(
            "transferChannelPolicy.missingRegion must be allow_empty or fail_closed");
    }
    if (policy.multiple_matches != "fail_closed")
    {
        throw std::runtime_error(
            "transferChannelPolicy.multipleMatches must be fail_closed");
    }
    if (policy.value != 0U)
    {
        throw std::runtime_error("transferChannelPolicy.value must be 0 for black_is_print");
    }
    if (policy.topology.self_intersection_policy != "reject"
        && policy.topology.self_intersection_policy != "tolerate_closed_self_intersection")
    {
        throw std::runtime_error(
            "transferChannelPolicy.topology.selfIntersectionPolicy is invalid");
    }
    if (policy.topology.max_self_intersection_pairs <= 0)
    {
        throw std::runtime_error(
            "transferChannelPolicy.topology.maxSelfIntersectionPairs must be positive");
    }
}

}  // namespace slicer_core
