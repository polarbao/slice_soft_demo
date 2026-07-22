#pragma once

#include "slicer_core/json_value.h"
#include "slicer_core/preflight/RepairedAssetIntakeTypes.h"

namespace slicer_core
{

/**
 * @brief Serialize one R4-06 required-family candidate intake result.
 * @param result Intake result.
 * @return JSON conforming to slicesoft.repaired_asset_intake.12e_08c_r4.1.
 */
Json BuildRepairedAssetIntakeReport(
    const RepairedAssetIntakeResult& result);

}  // namespace slicer_core
