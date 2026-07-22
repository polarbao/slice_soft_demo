#pragma once

#include "slicer_core/json_value.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace slicer_core
{

/**
 * @brief Read-only capability state exposed to the Stage 12E diagnostic UI.
 */
enum class TextureFillPartitionDiagnosticState
{
    Pending,
    Unavailable,
    Blocked,
    Diagnostic,
};

/**
 * @brief One report issue projected without changing its stable code or context.
 */
struct TextureFillPartitionDiagnosticIssueDto
{
    std::string code;
    std::string severity;
    std::string message;
    Json context;
};

/**
 * @brief Width metrics whose missing values remain explicitly unevaluated.
 */
struct TextureFillPartitionWidthUiDto
{
    std::optional<double> requestedwidthmm;
    std::optional<double> widthstepmm;
    std::optional<double> effectiveminimumwidthmm;
    std::optional<double> effectivewidthmm;
    std::optional<double> maxinteriordistancemm;
    std::optional<double> alltexturethresholdmm;
    std::optional<bool> alltexture;
};

/**
 * @brief Model, texture-surface, and model-fill partition statistics for UI display.
 */
struct TextureFillPartitionStatsUiDto
{
    bool evaluated{false};
    std::optional<std::uint64_t> modelvoxels;
    std::optional<std::uint64_t> texturesurfacevoxels;
    std::optional<std::uint64_t> modelfillvoxels;
    std::optional<std::uint64_t> overlapvoxels;
    std::optional<std::uint64_t> unassignedvoxels;
    std::optional<double> texturecoverageratio;
    std::optional<double> modelfillcoverageratio;
    std::optional<bool> partitionpass;
};

/**
 * @brief Read-only classification-to-raster evidence for UI display.
 */
struct TextureFillPartitionRasterMappingUiDto
{
    bool evaluated{false};
    std::string availability{"unavailable"};
    std::string status{"not_evaluated"};
    std::optional<std::uint64_t> rastervoxelcount;
    std::optional<std::uint64_t> modelrastervoxels;
    std::optional<std::uint64_t> texturesurfacerastervoxels;
    std::optional<std::uint64_t> modelfillrastervoxels;
    std::optional<std::uint64_t> overlaprastervoxels;
    std::optional<std::uint64_t> unassignedmodelrastervoxels;
    std::optional<bool> partitionpass;
    std::optional<std::uint64_t> layercount;
};

/**
 * @brief Read-only full-material closure evidence for UI display.
 */
struct TextureFillPartitionFullClosureUiDto
{
    bool evaluated{false};
    std::string availability{"unavailable"};
    std::string status{"not_evaluated"};
    std::string modelclosurestatus{"not_evaluated"};
    std::string supportclosurestatus{"not_evaluated"};
    std::string varnishclosurestatus{"not_evaluated"};
    std::optional<bool> fullclosurepass;
    std::optional<std::uint64_t> expecteddomaingappixels;
    std::optional<std::uint64_t> modeldomaingappixels;
    std::optional<std::uint64_t> supportrequiredgappixels;
    std::optional<std::uint64_t> outervarnishgappixels;
    std::optional<std::uint64_t> unexpectedoccupiedpixels;
    bool productionoutputwritten{false};
};

/**
 * @brief Core timing and memory values that preserve not-evaluated semantics.
 */
struct TextureFillPartitionPerformanceUiDto
{
    bool evaluated{false};
    std::optional<double> preflightms;
    std::optional<double> topologyms;
    std::optional<double> levelsetms;
    std::optional<double> gridsamplems;
    std::optional<double> occupancyms;
    std::optional<double> distancems;
    std::optional<double> partitionms;
    std::optional<double> texturetransferms;
    std::optional<double> rastermappingms;
    std::optional<double> fullclosurems;
    std::optional<double> totalcorems;
    std::optional<std::uint64_t> gridvoxelcount;
    std::optional<std::uint64_t> peakworkingsetbytes;
};

/**
 * @brief Immutable Stage 12E report projection consumed by the Qt UI layer.
 */
struct TextureFillPartitionDiagnosticUiDto
{
    TextureFillPartitionDiagnosticState state{
        TextureFillPartitionDiagnosticState::Pending};
    bool reportavailable{false};
    bool schemavalid{false};
    std::string schema;
    std::string availability{"unavailable"};
    std::string status{"pending"};
    std::string backend{"none"};
    std::string backendrole{"unavailable"};
    std::string productionacceptance{"not_evaluated"};
    bool productionoutputwritten{false};
    TextureFillPartitionWidthUiDto widthmetrics;
    TextureFillPartitionStatsUiDto partitionstats;
    TextureFillPartitionRasterMappingUiDto rastermapping;
    TextureFillPartitionFullClosureUiDto fullclosurelinkage;
    TextureFillPartitionPerformanceUiDto performance;
    std::vector<TextureFillPartitionDiagnosticIssueDto> issues;
};

/**
 * @brief Converts one 12E diagnostic report into a fail-closed, read-only UI DTO.
 */
class TextureFillPartitionDiagnosticFacade final
{
public:
    /**
     * @brief Build the state used before a model analysis has started.
     * @return Pending DTO with all derived metrics unevaluated.
     */
    static TextureFillPartitionDiagnosticUiDto Pending();

    /**
     * @brief Build an unavailable state when a requested report cannot be consumed.
     * @param reason Human-readable reason retained for diagnostics.
     * @return Unavailable DTO without fabricated numeric values.
     */
    static TextureFillPartitionDiagnosticUiDto Unavailable(
        const std::string& reason);

    /**
     * @brief Inspect an in-memory Stage 12E report without writing files or packages.
     * @param report Report conforming to slicesoft.texture_fill_partition.12e.1.
     * @return Pending-independent UI DTO in unavailable, blocked, or diagnostic state.
     */
    static TextureFillPartitionDiagnosticUiDto Inspect(const Json& report);

    /**
     * @brief Return the stable text representation of a diagnostic state.
     * @param state State to serialize for logs and tests.
     * @return pending, unavailable, blocked, or diagnostic.
     */
    static const char* StateName(TextureFillPartitionDiagnosticState state);
};

}  // namespace slicer_core
