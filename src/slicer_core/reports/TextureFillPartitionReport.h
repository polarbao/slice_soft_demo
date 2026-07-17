#pragma once

#include "slicer_core/config.h"
#include "slicer_core/json_value.h"
#include "slicer_core/materials/texture_application/TextureFillPartitionTypes.h"

namespace slicer_core
{

/**
 * @brief Build the unavailable Stage 12E report DTO for a validated configuration.
 * @param config Slice configuration snapshot.
 * @return Backend-neutral unavailable report data.
 */
TextureFillPartitionReportData BuildTextureFillPartitionUnavailableReportData(const SliceConfig& config);

/**
 * @brief Build the Stage 12E report skeleton before partition evidence exists.
 * @param config Slice configuration snapshot.
 * @return Report conforming to slicesoft.texture_fill_partition.12e.1.
 */
Json BuildTextureFillPartitionReportSkeleton(const SliceConfig& config);

}  // namespace slicer_core
