#pragma once

#include "slicer_core/TiffReadApi.h"
#include "slicer_core/config.h"
#include "slicer_core/json_value.h"
#include "slicer_core/output/rgbwsv/RgbwsvPackageWriter.h"
#include "slicer_core/output/rgbwsvt/RgbwsvtProtocol.h"
#include "slicer_core/materials/transfer/TransferMaterialVolumePlan.h"

#include <array>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace slicer_core
{

using RgbwsvtChannelStatistics = std::array<TiffChannelStats, kRgbwsvtChannelCount>;

struct RgbwsvtMaterialStatistics
{
    std::uint64_t rgbPrintPixels{0U};
    std::uint64_t whitePrintPixels{0U};
    std::uint64_t supportPrintPixels{0U};
    std::uint64_t varnishPrintPixels{0U};
    std::uint64_t transferPrintPixels{0U};
    std::uint64_t unexpectedOverlapPixels{0U};
};

struct RgbwsvtLegacyLayerStatistics
{
    int layerIndex{0};
    RgbwsvtChannelStatistics channels{};
    RgbwsvtMaterialStatistics materials{};
};

struct RgbwsvtLegacyLayerWriteResult
{
    RgbwsvtChannelStatistics channelStatistics{};
    RgbwsvtMaterialStatistics materialStatistics{};
};

[[nodiscard]] RgbwsvtLegacyLayerWriteResult WriteRgbwsvtLegacyProductionLayerTiff(
    const std::filesystem::path& path,
    const RgbwsvProductionStorageSpec& storage,
    const RgbwsvtProductionLayer& layer);

void MergeRgbwsvtChannelStatistics(
    RgbwsvtChannelStatistics& totals,
    const RgbwsvtChannelStatistics& layer);

void MergeRgbwsvtMaterialStatistics(
    RgbwsvtMaterialStatistics& totals,
    const RgbwsvtMaterialStatistics& layer);

[[nodiscard]] Json ChannelStatisticsToJson(
    std::span<const TiffChannelStats> statistics,
    std::span<const std::string> channelOrder);

[[nodiscard]] Json BuildRgbwsvtMaterialProcessReport(
    const Json& legacyReport,
    const MaterialProcessProfileConfig& profile,
    std::span<const RgbwsvtLegacyLayerStatistics> layers,
    const RgbwsvtMaterialStatistics& totals,
    std::uint64_t totalPixels);

[[nodiscard]] Json BuildRgbwsvtSliceReport(
    const Json& legacyReport,
    std::span<const RgbwsvtLegacyLayerStatistics> layers,
    const RgbwsvtChannelStatistics& channelTotals,
    const RgbwsvtMaterialStatistics& materialTotals);

[[nodiscard]] Json BuildLegacyTransferChannelReport(
    const TransferChannelPolicyConfig& policy,
    const TransferMaterialVolumePlan& plan,
    std::span<const RgbwsvtLegacyLayerStatistics> layers,
    const RgbwsvtChannelStatistics& channelTotals,
    const RgbwsvtMaterialStatistics& materialTotals);

class RgbwsvtCandidatePackageGuard final
{
public:
    explicit RgbwsvtCandidatePackageGuard(std::filesystem::path packageDirectory);
    ~RgbwsvtCandidatePackageGuard();
    RgbwsvtCandidatePackageGuard(const RgbwsvtCandidatePackageGuard&) = delete;
    RgbwsvtCandidatePackageGuard& operator=(const RgbwsvtCandidatePackageGuard&) = delete;
    void Commit() noexcept;

private:
    std::filesystem::path m_packageDirectory;
    bool m_cleanupRequired{true};
};

[[nodiscard]] Json BuildLegacyTiffManifestMetadata(
    const OutputConfig& output,
    const Json::Array& layers,
    bool writeTiffLayers,
    const RgbwsvtChannelStatistics* rgbwsvtStatistics = nullptr);

}  // namespace slicer_core
