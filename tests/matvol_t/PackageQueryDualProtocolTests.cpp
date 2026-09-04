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
        && Expect(
            summary.Value()->production_acceptance
                == "rgbwsvt_candidate_unvalidated",
            "summary exposes candidate production acceptance")
        && Expect(layer.IsOk(), "RGBWSVT descriptor succeeds")
        && Expect(layer.Value()->print_pixels.size() == 7U, "descriptor has seven counts")
        && Expect(layer.Value()->print_pixels[6U] == 1U, "descriptor sees T print")
        && Expect(verified.IsOk() && !verified.Value()->valid,
                  "candidate RGBWSVT is not production valid")
        && Expect(
            verified.Value()->production_acceptance
                == "rgbwsvt_candidate_unvalidated",
            "verify exposes candidate production acceptance")
        && Expect(!verified.Value()->errors.empty(),
                  "candidate verify explains production rejection")
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

// 七通道【组合】预览必须可渲染，且缩裹参与合成。
//
// 此前只有 single_channel + T 被覆盖，组合含 T 一条用例都没有——
// 而恰恰有一道守卫写着「T is supported only by single_channel preview」，
// 把宿主的「全通道并集（七通道·含缩裹）」整个挡在渲染之前。
// 用户切完片看到的是空预览框，而通道统计图里 T 明明有数据。
// 组合视图才是默认视图，只测单通道等于没测用户真正会看到的那一个。
bool SevenChannelCompositePreviewRenders()
{
    TemporaryDirectory directory;
    const std::filesystem::path package =
        MakeRgbwsvtPackage(directory.Path(), false);
    const std::unique_ptr<slicer_core::api::PackageQueryFacade> facade =
        slicer_core::api::implementation::CreatePackageQueryFacade();
    NeverCancel cancel;

    slicer_core::api::PreviewRequest composite;
    composite.package_dir = package;
    composite.layer_index = 0;
    composite.mode = "composite";
    composite.channels = {"R", "G", "B", "W", "S", "V", "T"};
    composite.max_width_px = 16;
    composite.output_path = directory.Path() / "composite_with_transfer.png";
    const auto rendered = facade->RenderLayerPreview(composite, cancel);

    // 同层的六通道组合作为对照：证明失败不是包本身的问题。
    slicer_core::api::PreviewRequest sixChannel = composite;
    sixChannel.channels = {"R", "G", "B", "W", "S", "V"};
    sixChannel.output_path = directory.Path() / "composite_six.png";
    const auto sixRendered = facade->RenderLayerPreview(sixChannel, cancel);

    return Expect(
               rendered.IsOk(),
               std::string{"seven-channel composite preview renders: "}
                   + (rendered.IsOk() ? "" : rendered.Error()->message))
        && Expect(
            sixRendered.IsOk(),
            "six-channel composite preview still renders")
        && Expect(
            rendered.Value()->width_px > 0 && rendered.Value()->height_px > 0,
            "seven-channel composite emits a sized image")
        && Expect(
            std::filesystem::exists(rendered.Value()->output_path),
            "seven-channel composite writes the preview file")
        && Expect(
            rendered.Value()->cache_key != sixRendered.Value()->cache_key,
            "seven-channel composite differs from the six-channel view");
}

int main()
{
    const bool passed = SevenChannelQueriesSucceed()
        && SevenChannelCompositePreviewRenders()
        && BadSevenChannelStatisticsFailClosed();
    if (!passed)
    {
        return 1;
    }
    std::cout << "MATVOL-T Package Query dual-protocol tests passed\n";
    return 0;
}
