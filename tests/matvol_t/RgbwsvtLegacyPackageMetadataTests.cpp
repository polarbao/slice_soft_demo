#include "slicer_core/output/rgbwsvt/RgbwsvtLegacyPackageMetadata.h"
#include "slicer_core/output/rgbwsvt/RgbwsvtTiffIo.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace
{

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL " << message << '\n';
    }
    return condition;
}

slicer_core::RgbwsvtProductionLayer Layer(const bool withTransfer)
{
    slicer_core::RgbwsvtProductionLayer layer;
    layer.widthPx = 2;
    layer.heightPx = 1;
    layer.channels = {
        10U, 20U, 30U, 255U, 255U, 255U, 255U,
        40U, 50U, 60U, 255U, 255U, 255U, 255U};
    if (withTransfer)
    {
        for (std::size_t index = 7U; index < 13U; ++index)
        {
            layer.channels[index] = 255U;
        }
        layer.channels[13U] = 0U;
    }
    return layer;
}

slicer_core::RgbwsvtChannelStatistics LayerStatistics();

bool WritesAndReadsPersistedStatistics()
{
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "slicesoft_rgbwsvt_legacy_metadata.tiff";
    std::error_code error;
    std::filesystem::remove(path, error);
    slicer_core::RgbwsvProductionStorageSpec storage;
    const slicer_core::RgbwsvtProductionLayer source = Layer(true);
    const slicer_core::RgbwsvtLegacyLayerWriteResult result =
        slicer_core::WriteRgbwsvtLegacyProductionLayerTiff(path, storage, source);
    const slicer_core::RgbwsvtTiffReadResult persisted =
        slicer_core::ReadRgbwsvtTiff(path);
    const bool passed = ExpectTrue(persisted.pixels == source.channels, "persisted bytes are exact")
        && ExpectTrue(result.channelStatistics[6U].print_pixels == 1U, "T print count is one")
        && ExpectTrue(result.channelStatistics[0U].print_pixels == 1U, "cleared T pixel is absent from R")
        && ExpectTrue(result.channelStatistics[0U].empty_pixels == 1U, "R sees cleared T pixel");
    std::filesystem::remove(path, error);
    return passed;
}

bool BuildsProtocolSpecificMetadata()
{
    slicer_core::OutputConfig output;
    const slicer_core::Json::Array layers{
        slicer_core::Json::object({{"index", 0}, {"path", "layers/layer_000000.tiff"}})};
    const slicer_core::Json legacy = slicer_core::BuildLegacyTiffManifestMetadata(
        output, layers, true);
    bool passed = ExpectTrue(legacy.at("channelCount").as_int() == 6, "legacy keeps six channels")
        && ExpectTrue(!legacy.contains("channelStats"), "legacy metadata does not gain T statistics");

    output.package_protocol = "p0.rgbwsvt.1";
    output.channel_order = {"R", "G", "B", "W", "S", "V", "T"};
    slicer_core::RgbwsvtChannelStatistics totals{};
    slicer_core::MergeRgbwsvtChannelStatistics(totals, LayerStatistics());
    const slicer_core::Json transfer = slicer_core::BuildLegacyTiffManifestMetadata(
        output, layers, true, &totals);
    passed = ExpectTrue(transfer.at("channelCount").as_int() == 7, "transfer metadata has seven channels")
        && ExpectTrue(
            transfer.at("channelOrder").at(6U).as_string() == "T",
            "transfer metadata keeps T at offset six")
        && ExpectTrue(
            transfer.at("channelStats").at("T").at("printPixels").as_int() == 1,
            "manifest T count comes from persisted statistics")
        && ExpectTrue(
            transfer.at("statisticsSource").as_string() == "persisted_tiff_bytes",
            "manifest declares persisted byte authority")
        && passed;
    return passed;
}

slicer_core::RgbwsvtChannelStatistics LayerStatistics()
{
    slicer_core::RgbwsvtChannelStatistics statistics{};
    statistics[0U].print_pixels = 1U;
    statistics[0U].empty_pixels = 1U;
    statistics[6U].print_pixels = 1U;
    statistics[6U].full_print_pixels = 1U;
    statistics[6U].empty_pixels = 1U;
    return statistics;
}

bool MissingPersistedStatisticsFailsClosed()
{
    slicer_core::OutputConfig output;
    output.package_protocol = "p0.rgbwsvt.1";
    try
    {
        (void)slicer_core::BuildLegacyTiffManifestMetadata(output, {}, true);
    }
    catch (const std::exception&)
    {
        return true;
    }
    return ExpectTrue(false, "RGBWSVT manifest requires persisted statistics");
}

bool CandidateGuardCleansFailedOutput()
{
    const std::filesystem::path package =
        std::filesystem::temp_directory_path() / "slicesoft_rgbwsvt_guard_cleanup";
    std::error_code error;
    std::filesystem::remove_all(package, error);
    {
        slicer_core::RgbwsvtCandidatePackageGuard guard{package};
        std::filesystem::create_directories(package / "layers");
        std::ofstream{package / "layers/partial.tiff"} << "partial";
    }
    return ExpectTrue(!std::filesystem::exists(package), "failed candidate leaves no partial package");
}

bool CandidateGuardPreservesExistingOutput()
{
    const std::filesystem::path package =
        std::filesystem::temp_directory_path() / "slicesoft_rgbwsvt_guard_existing";
    std::error_code error;
    std::filesystem::remove_all(package, error);
    std::filesystem::create_directories(package);
    std::ofstream{package / "sentinel.txt"} << "keep";
    bool rejected = false;
    try
    {
        slicer_core::RgbwsvtCandidatePackageGuard guard{package};
    }
    catch (const std::exception&)
    {
        rejected = true;
    }
    const bool preserved = std::filesystem::exists(package / "sentinel.txt");
    std::filesystem::remove_all(package, error);
    return ExpectTrue(rejected, "existing candidate target is rejected")
        && ExpectTrue(preserved, "existing output is never removed");
}

}  // namespace

int main()
{
    int failures = 0;
    failures += WritesAndReadsPersistedStatistics() ? 0 : 1;
    failures += BuildsProtocolSpecificMetadata() ? 0 : 1;
    failures += MissingPersistedStatisticsFailsClosed() ? 0 : 1;
    failures += CandidateGuardCleansFailedOutput() ? 0 : 1;
    failures += CandidateGuardPreservesExistingOutput() ? 0 : 1;
    if (failures != 0)
    {
        std::cerr << "FAIL RgbwsvtLegacyPackageMetadataTests " << failures << " case(s)\n";
        return 1;
    }
    std::cout << "PASS RgbwsvtLegacyPackageMetadataTests 5/5\n";
    return 0;
}
