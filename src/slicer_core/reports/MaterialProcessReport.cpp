#include "slicer_core/reports/MaterialProcessReport.h"

#include <stdexcept>

namespace slicer_core
{
namespace
{

double CoverageRatio(const std::uint64_t count, const std::uint64_t total)
{
    return total == 0U ? 0.0 : static_cast<double>(count) / static_cast<double>(total);
}

}  // namespace

Json BuildMaterialProcessReport(const MaterialProcessReportRequest& request)
{
    if (request.config == nullptr
        || request.layerCount < 0
        || request.layers.size() != static_cast<std::size_t>(request.layerCount))
    {
        throw std::invalid_argument("material process report request is incomplete");
    }
    const SliceConfig& config = *request.config;
    const MaterialProcessProfileConfig& profile = config.material_process_profile;
    const std::uint64_t totalPixels = static_cast<std::uint64_t>(request.widthPx)
        * static_cast<std::uint64_t>(request.heightPx)
        * static_cast<std::uint64_t>(request.layerCount);
    std::uint64_t rgbPrintPixels{0U};
    std::uint64_t unprintableWhiteCarrierPixels{0U};
    const std::uint64_t whitePrintPixels = request.channelTotals[3U].print_pixels;
    const std::uint64_t supportPrintPixels = request.channelTotals[4U].print_pixels;
    const std::uint64_t varnishPrintPixels = request.channelTotals[5U].print_pixels;

    Json::Array layers;
    Json::Array varnishActiveLayerIndices;
    for (const MaterialProcessLayerStatistics& layer : request.layers)
    {
        rgbPrintPixels += layer.rgbPrintPixels;
        unprintableWhiteCarrierPixels += layer.unprintableWhiteCarrierPixels;
        if (layer.varnishPrintPixels > 0U)
        {
            varnishActiveLayerIndices.emplace_back(layer.layerIndex);
        }
        layers.emplace_back(Json::object({
            {"layerIndex", layer.layerIndex},
            {"rgbPrintPixels", layer.rgbPrintPixels},
            {"whitePrintPixels", layer.whitePrintPixels},
            {"unprintableWhiteCarrierPixels", layer.unprintableWhiteCarrierPixels},
            {"varnishPrintPixels", layer.varnishPrintPixels},
            {"supportPrintPixels", layer.supportPrintPixels}}));
    }

    const std::uint64_t missingUnderbasePixels = whitePrintPixels < rgbPrintPixels
        ? rgbPrintPixels - whitePrintPixels : 0U;
    constexpr std::uint64_t unexpectedOverlapPixels{0U};
    Json::Array failures;
    Json::Array warnings;
    if (profile.enabled)
    {
        if (profile.validation.require_rgb_pixels && rgbPrintPixels == 0U)
            failures.emplace_back("E_MATERIAL_PROCESS_PROFILE_EMPTY_RGB");
        if (profile.validation.require_white_pixels && whitePrintPixels == 0U)
            failures.emplace_back("E_MATERIAL_PROCESS_PROFILE_EMPTY_WHITE");
        if (profile.validation.require_varnish_pixels && varnishPrintPixels == 0U)
            failures.emplace_back("E_MATERIAL_PROCESS_PROFILE_EMPTY_VARNISH");
        if (profile.validation.require_support_pixels && supportPrintPixels == 0U)
            failures.emplace_back("E_MATERIAL_PROCESS_PROFILE_EMPTY_SUPPORT");
        if (unexpectedOverlapPixels
            > static_cast<std::uint64_t>(profile.validation.max_unexpected_overlap_pixels))
            failures.emplace_back("E_MATERIAL_PROCESS_PROFILE_UNEXPECTED_OVERLAP");
        if (profile.white.enabled && profile.white.mode == "underbase"
            && missingUnderbasePixels > 0U)
            failures.emplace_back("E_MATERIAL_PROCESS_PROFILE_UNDERBASE_COVERAGE_LOW");
        if (!config.material_policy.enabled && !config.material_role_mapping.enabled)
            warnings.emplace_back(
                "materialProcessProfile is report-only; no materialPolicy or materialRoleMapping is enabled");
    }

    return Json::object({
        {"enabled", profile.enabled}, {"profileName", profile.name},
        {"target", profile.target}, {"inputFormat", request.inputFormat},
        {"sourceModel", request.sourceModel.generic_string()},
        {"grid", Json::object({
            {"widthPx", request.widthPx}, {"heightPx", request.heightPx},
            {"layerCount", request.layerCount},
            {"pixelSizeMm", Json::array({request.pixelSizeXmm, request.pixelSizeYmm})},
            {"layerThicknessMm", config.output.layer_thickness_mm}})},
        {"layerCount", request.layerCount},
        {"rgb", Json::object({
            {"enabled", profile.rgb.enabled}, {"source", profile.rgb.source},
            {"printPixels", rgbPrintPixels},
            {"coverageRatio", CoverageRatio(rgbPrintPixels, totalPixels)}})},
        {"white", Json::object({
            {"enabled", profile.white.enabled}, {"mode", profile.white.mode},
            {"coverage", profile.white.coverage}, {"value", static_cast<int>(profile.white.value)},
            {"expandPx", profile.white.expand_px}, {"shrinkPx", profile.white.shrink_px},
            {"printPixels", whitePrintPixels},
            {"unprintableWhiteCarrierPixels", unprintableWhiteCarrierPixels},
            {"coverageRatio", CoverageRatio(whitePrintPixels, totalPixels)},
            {"missingUnderbasePixels", missingUnderbasePixels}})},
        {"varnish", Json::object({
            {"enabled", profile.varnish.enabled}, {"mode", profile.varnish.mode},
            {"topLayers", profile.varnish.top_layers},
            {"value", static_cast<int>(profile.varnish.value)},
            {"coverage", profile.varnish.coverage}, {"printPixels", varnishPrintPixels},
            {"coverageRatio", CoverageRatio(varnishPrintPixels, totalPixels)},
            {"activeLayerIndices", Json{std::move(varnishActiveLayerIndices)}}})},
        {"support", Json::object({
            {"expected", profile.support.expected}, {"mode", profile.support.mode},
            {"printPixels", supportPrintPixels},
            {"coverageRatio", CoverageRatio(supportPrintPixels, totalPixels)}})},
        {"unexpectedOverlapPixels", unexpectedOverlapPixels},
        {"layers", Json{std::move(layers)}},
        {"validation", Json::object({
            {"pass", failures.empty()}, {"failures", Json{std::move(failures)}}})},
        {"warnings", Json{std::move(warnings)}}});
}

}  // namespace slicer_core
