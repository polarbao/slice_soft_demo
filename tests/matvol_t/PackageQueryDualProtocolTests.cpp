#include "slicer_core/api/implementation/PackageQueryFacadeImplementation.h"
#include "slicer_core/json_value.h"
#include "slicer_core/output/rgbwsvt/RgbwsvtTiffIo.h"
#include "slicer_core/tiff_io.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace
{

class NeverCancel final : public slicer_core::api::ICancelToken
{
public:
    bool IsCancelRequested() const noexcept override { return false; }
};

class TemporaryDirectory final
{
public:
    TemporaryDirectory()
        : path_(std::filesystem::temp_directory_path()
              / ("slicesoft_matvol_t_package_query_"
                 + std::to_string(
                     std::chrono::steady_clock::now()
                         .time_since_epoch().count())))
    {
        std::filesystem::create_directories(path_);
    }
    ~TemporaryDirectory()
    {
        std::error_code ignored;
        std::filesystem::remove_all(path_, ignored);
    }
    const std::filesystem::path& Path() const { return path_; }

private:
    std::filesystem::path path_;
};

bool Expect(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
    }
    return condition;
}

void WriteJson(const std::filesystem::path& path, const slicer_core::Json& value)
{
    std::ofstream output{path, std::ios::binary | std::ios::trunc};
    output << value.dump(2) << '\n';
}

slicer_core::Json StatsJson(const slicer_core::TiffChannelStats& stats)
{
    return slicer_core::Json::object({
        {"printPixels", stats.print_pixels},
        {"fullPrintPixels", stats.full_print_pixels},
        {"partialPrintPixels", stats.partial_print_pixels},
        {"emptyPixels", stats.empty_pixels},
        {"minValue", stats.min_value},
        {"maxValue", stats.max_value}});
}

std::filesystem::path MakeRgbwsvtPackage(
    const std::filesystem::path& root,
    const bool corruptDeclaredTransferStats)
{
    const std::filesystem::path package = root / "package";
    std::filesystem::create_directories(package / "layers");
    std::filesystem::create_directories(package / "reports");
    slicer_core::TiffImageSpec spec;
    spec.width = 2U;
    spec.height = 2U;
    spec.samples_per_pixel = 7U;
    spec.storage_mode = slicer_core::TiffStorageMode::Stripped;
    spec.rows_per_strip = 2U;
    std::vector<std::uint8_t> pixels(28U, 255U);
    pixels[0U] = 0U;
    pixels[13U] = 0U;
    const std::filesystem::path tiff = package / "layers/layer_0.tiff";
    slicer_core::write_rgbwsvt_tiff(tiff, spec, pixels);
    const slicer_core::RgbwsvtTiffReadResult decoded =
        slicer_core::ReadRgbwsvtTiff(tiff);

    static const std::vector<std::string> channels{
        "R", "G", "B", "W", "S", "V", "T"};
    slicer_core::Json::Object stats;
    for (std::size_t index = 0U; index < channels.size(); ++index)
    {
        stats.emplace(channels[index], StatsJson(decoded.channelStats[index]));
    }
    if (corruptDeclaredTransferStats)
    {
        slicer_core::Json::Object transfer = stats.at("T").as_object();
        transfer["printPixels"] = 0;
        stats["T"] = slicer_core::Json{std::move(transfer)};
    }
    const slicer_core::Json layer = slicer_core::Json::object({
        {"index", 0}, {"zMm", 0.1}, {"path", "layers/layer_0.tiff"},
        {"widthPx", 2}, {"heightPx", 2}});
    WriteJson(
        package / "manifest.json",
        slicer_core::Json::object({
            {"schema", "p0.rgbwsvt.1"},
            {"productionAcceptance", "rgbwsvt_candidate_unvalidated"},
            {"grid", slicer_core::Json::object({
                {"widthPx", 2}, {"heightPx", 2}, {"layerCount", 1},
                {"dpiX", 600}, {"dpiY", 635}, {"layerThicknessMm", 0.1}})},
            {"layers", slicer_core::Json::array({layer})},
            {"tiff", slicer_core::Json::object({
                {"channelCount", 7},
                {"channelOrder", slicer_core::Json::array(
                    {"R", "G", "B", "W", "S", "V", "T"})},
                {"bitDepth", 8}, {"sampleFormat", "uint"},
                {"planarConfig", "contiguous"},
                {"storageMode", "stripped"}, {"compression", "none"},
                {"polarity", "black_is_print"},
                {"printValue", 0}, {"emptyValue", 255},
                {"statisticsSource", "persisted_tiff_bytes"},
                {"channelStats", slicer_core::Json{std::move(stats)}},
                {"layers", slicer_core::Json::array({layer})}})},
            {"reports", slicer_core::Json::object({
                {"transferChannel", "reports/transfer_channel_report.json"}})}}));
    WriteJson(
        package / "reports/transfer_channel_report.json",
        slicer_core::Json::object({
            {"schema", "slicesoft.transfer_channel_report.1"},
            {"packageProtocol", "p0.rgbwsvt.1"}}));
    return package;
}

bool SevenChannelQueriesSucceed()
{
    TemporaryDirectory directory;
    const std::filesystem::path package =
        MakeRgbwsvtPackage(directory.Path(), false);
    const std::unique_ptr<slicer_core::api::PackageQueryFacade> facade =
        slicer_core::api::implementation::CreatePackageQueryFacade();
    NeverCancel cancel;
    const auto summary = facade->GetSummary(package);
    const auto layer = facade->GetLayerDescriptor(package, 0);
    const auto verified = facade->Verify(package, cancel);
    const auto report = facade->ReadReport(package, "transferChannel");
    slicer_core::api::PreviewRequest preview;
    preview.package_dir = package;
    preview.layer_index = 0;
    preview.mode = "single_channel";
    preview.channels = {"T"};
    preview.max_width_px = 16;
    preview.output_path = directory.Path() / "transfer.png";
    const auto rendered = facade->RenderLayerPreview(preview, cancel);

    return Expect(summary.IsOk(), "RGBWSVT summary succeeds")
        && Expect(summary.Value()->channels.size() == 7U, "summary has seven channels")
        && Expect(summary.Value()->channels.back() == "T", "summary ends with T")
        && Expect(layer.IsOk(), "RGBWSVT descriptor succeeds")
        && Expect(layer.Value()->print_pixels.size() == 7U, "descriptor has seven counts")
        && Expect(layer.Value()->print_pixels[6U] == 1U, "descriptor sees T print")
        && Expect(verified.IsOk() && verified.Value()->valid, "RGBWSVT verify succeeds")
        && Expect(verified.Value()->per_layer_checksum[0U].size() == 7U, "verify has seven checksums")
        && Expect(report.IsOk(), "RGBWSVT report read succeeds")
        && Expect(rendered.IsOk(), "T single-channel preview succeeds")
        && Expect(std::filesystem::is_regular_file(preview.output_path), "T preview is written")
        && Expect(rendered.Value()->cache_key.find("|ch:T|") != std::string::npos,
                  "T preview cache key is protocol-aware");
}

bool BadSevenChannelStatisticsFailClosed()
{
    TemporaryDirectory directory;
    const std::filesystem::path package =
        MakeRgbwsvtPackage(directory.Path(), true);
    const std::unique_ptr<slicer_core::api::PackageQueryFacade> facade =
        slicer_core::api::implementation::CreatePackageQueryFacade();
    NeverCancel cancel;
    const auto verified = facade->Verify(package, cancel);
    return Expect(verified.IsOk(), "bad RGBWSVT returns structured verify result")
        && Expect(!verified.Value()->valid, "bad RGBWSVT is rejected")
        && Expect(!verified.Value()->errors.empty(), "bad RGBWSVT reports an error");
}

}  // namespace

int main()
{
    const bool passed = SevenChannelQueriesSucceed()
        && BadSevenChannelStatisticsFailClosed();
    if (!passed)
    {
        return 1;
    }
    std::cout << "MATVOL-T Package Query dual-protocol tests passed\n";
    return 0;
}
