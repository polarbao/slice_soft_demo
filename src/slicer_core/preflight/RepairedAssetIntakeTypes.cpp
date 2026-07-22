#include "slicer_core/preflight/RepairedAssetIntakeTypes.h"

namespace slicer_core
{

std::string RepairedAssetCandidateKindName(
    const RepairedAssetCandidateKind kind)
{
    switch (kind)
    {
    case RepairedAssetCandidateKind::StrictPassOriginal:
        return "strict_pass_original";
    case RepairedAssetCandidateKind::ExternalRepaired:
        return "external_repaired";
    case RepairedAssetCandidateKind::IndependentlyRebuilt:
        return "independently_rebuilt";
    case RepairedAssetCandidateKind::Unknown:
        return "unknown";
    }
    return "unknown";
}

RepairedAssetCandidateKind ParseRepairedAssetCandidateKind(
    const std::string& value)
{
    if (value == "strict_pass_original")
    {
        return RepairedAssetCandidateKind::StrictPassOriginal;
    }
    if (value == "external_repaired")
    {
        return RepairedAssetCandidateKind::ExternalRepaired;
    }
    if (value == "independently_rebuilt")
    {
        return RepairedAssetCandidateKind::IndependentlyRebuilt;
    }
    return RepairedAssetCandidateKind::Unknown;
}

}  // namespace slicer_core
