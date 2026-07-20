#pragma once

#include "slicer_core/config.h"
#include "slicer_core/json_value.h"
#include "slicer_core/materials/texture_application/TextureFillPartitionTypes.h"

namespace slicer_core
{

struct TextureFillPartitionDiagnosticComposerResult;
struct TextureFillPartitionClosureAdapterResult;
struct TextureFillPartitionRasterMappingResult;
struct TextureFillPartitionTextureTransferResult;

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

/**
 * @brief Build a successful or blocked Stage 12E diagnostic partition report.
 * @param config Effective slice configuration snapshot.
 * @param result Validated backend-neutral partition result.
 * @param conformance Optional CPU/OpenVDB diagnostic comparison.
 * @param transfer Optional backend-neutral texture-transfer evidence.
 * @param composer Optional in-memory diagnostic-composer evidence.
 * @param closure Optional exact 12D model-domain closure linkage evidence.
 * @param rasterMapping Optional deterministic classification-to-raster evidence.
 * @return Report conforming to slicesoft.texture_fill_partition.12e.1.
 */
Json BuildTextureFillPartitionReport(
    const SliceConfig& config,
    const GlobalTextureFillPartitionResult& result,
    const TextureFillPartitionConformanceResult* conformance = nullptr,
    const TextureFillPartitionTextureTransferResult* transfer = nullptr,
    const TextureFillPartitionDiagnosticComposerResult* composer = nullptr,
    const TextureFillPartitionClosureAdapterResult* closure = nullptr,
    const TextureFillPartitionRasterMappingResult* rasterMapping = nullptr);

/**
 * @brief Serialize deterministic Stage 12E width-sweep evidence.
 * @param sweep Validated width-sweep result.
 * @return Backend-neutral summary suitable for a diagnostic report section.
 */
Json BuildTextureFillPartitionWidthSweepSummary(
    const TextureFillPartitionWidthSweepResult& sweep);

}  // namespace slicer_core
