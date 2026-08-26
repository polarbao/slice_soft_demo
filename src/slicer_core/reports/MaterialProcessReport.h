#pragma once

#include "slicer_core/TiffReadApi.h"
#include "slicer_core/config.h"
#include "slicer_core/json_value.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>

namespace slicer_core
{

struct MaterialProcessLayerStatistics
{
    int layerIndex{0};
    std::uint64_t rgbPrintPixels{0U};
    std::uint64_t whitePrintPixels{0U};
    std::uint64_t supportPrintPixels{0U};
    std::uint64_t varnishPrintPixels{0U};
    std::uint64_t unprintableWhiteCarrierPixels{0U};
};

struct MaterialProcessReportRequest
{
    const SliceConfig* config{nullptr};
    std::string inputFormat;
    std::filesystem::path sourceModel;
    int widthPx{0};
    int heightPx{0};
    int layerCount{0};
    double pixelSizeXmm{0.0};
    double pixelSizeYmm{0.0};
    std::span<const MaterialProcessLayerStatistics> layers;
    std::array<TiffChannelStats, 6> channelTotals{};
};

[[nodiscard]] Json BuildMaterialProcessReport(
    const MaterialProcessReportRequest& request);

}  // namespace slicer_core
