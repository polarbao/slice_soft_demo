#include "slicer_core/materials/transfer/TransferMaterialResolver.h"

#include "slicer_core/materials/transfer/TransferChannelError.h"

#include <algorithm>
#include <vector>

namespace slicer_core
{

TransferMaterialMatch ResolveTransferMaterial(
    const TransferChannelPolicyConfig& policy,
    const std::span<const MaterialInfo> materialInfos)
{
    if (!policy.enabled)
    {
        return {};
    }
    if (policy.match_source != "material_diffuse_rgb"
        || policy.material_diffuse_rgb_values.empty())
    {
        throw TransferChannelError(
            TransferChannelErrorCode::ConfigInvalid,
            "exact material diffuse RGB matching requires at least one configured colour");
    }
    if (policy.missing_region != "allow_empty"
        && policy.missing_region != "fail_closed")
    {
        throw TransferChannelError(
            TransferChannelErrorCode::ConfigInvalid,
            "missingRegion must be allow_empty or fail_closed");
    }
    if (policy.multiple_matches != "fail_closed")
    {
        throw TransferChannelError(
            TransferChannelErrorCode::ConfigInvalid,
            "multipleMatches must be fail_closed");
    }

    std::vector<const MaterialInfo*> matches;
    for (const MaterialInfo& info : materialInfos)
    {
        if (!info.has_diffuse)
        {
            continue;
        }
        if (std::find(
                policy.material_diffuse_rgb_values.begin(),
                policy.material_diffuse_rgb_values.end(),
                info.diffuse_rgb)
            != policy.material_diffuse_rgb_values.end())
        {
            matches.push_back(&info);
        }
    }

    if (matches.empty())
    {
        if (policy.missing_region == "allow_empty")
        {
            return {};
        }
        throw TransferChannelError(
            TransferChannelErrorCode::RegionMissing,
            "no material matches the configured transfer colours");
    }
    if (matches.size() != 1U)
    {
        throw TransferChannelError(
            TransferChannelErrorCode::MatchAmbiguous,
            "configured transfer colours match more than one material");
    }

    TransferMaterialMatch result;
    result.present = true;
    result.materialName = matches.front()->name;
    result.diffuseRgb = matches.front()->diffuse_rgb;
    return result;
}

}  // namespace slicer_core
