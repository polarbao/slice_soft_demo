#include "slicer_core/api/implementation/ModelFacadeImplementation.h"
#include "slicer_core/api/implementation/PackageQueryFacadeImplementation.h"
#include "slicer_core/json_value.h"
#include "slicer_core/tiff_io.h"
#include "TestSupport.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{

constexpr int kWidth{4};
constexpr int kHeight{3};
constexpr int kLayerCount{2};
constexpr std::size_t kChannelCount{6U};

using slicesoft::tests::stage14b02::CancelToken;
using slicesoft::tests::stage14b02::TemporaryDirectory;

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
    }
    return condition;
}

bool ExpectErrorCode(
    const slicer_core::api::ApiError* error,
    const std::string& expected,
    const std::string& message)
{
    return ExpectTrue(error != nullptr, message + " returns an error")
        && ExpectTrue(error->code == expected, message + " has stable code");
}

void WriteJson(
    const std::filesystem::path& path,
    const slicer_core::Json& document)
{
    std::ofstream output{path, std::ios::binary | std::ios::trunc};
    if (!output)
    {
        throw std::runtime_error(
            "failed to write fixture JSON: " + path.generic_string());
    }
    output << document.dump(2) << '\n';
}

std::vector<std::uint8_t> MakePixels(const int layerIndex)
{
    std::vector<std::uint8_t> pixels(
        static_cast<std::size_t>(kWidth * kHeight) * kChannelCount,
        255U);
    pixels.at(0U) = 0U;
    pixels.at(kChannelCount + 3U) = 0U;
    pixels.at(kChannelCount * 2U + 4U) = 0U;
    pixels.at(kChannelCount * 3U + 5U) = 0U;
    pixels.at(kChannelCount * 4U) =
        static_cast<std::uint8_t>(32 + layerIndex);
    pixels.at(kChannelCount * 4U + 1U) = 64U;
    pixels.at(kChannelCount * 4U + 2U) = 96U;
    return pixels;
}

slicer_core::Json ChannelCounts(
    const int red,
    const int green,
    const int blue,
    const int white,
    const int support,
    const int varnish)
{
    return slicer_core::Json::object({
        {"R", red},
        {"G", green},
        {"B", blue},
        {"W", white},
        {"S", support},
        {"V", varnish},
    });
}

slicer_core::Json MakeManifest(
    const std::string& schema,
    const bool includeCapabilitySummary,
    const std::string& reportPath = "reports/slice_report.json")
{
    slicer_core::Json::Array layers;
    for (int layerIndex = 0; layerIndex < kLayerCount; ++layerIndex)
    {
        layers.push_back(slicer_core::Json::object({
            {"index", layerIndex},
            {"zMm", 0.019 + static_cast<double>(layerIndex) * 0.038},
            {"path", "layers/layer_" + std::to_string(layerIndex) + ".tiff"},
            {"widthPx", kWidth},
            {"heightPx", kHeight},
        }));
    }

    slicer_core::Json::Object manifest{
        {"schema", schema},
        {"grid",
         slicer_core::Json::object({
             {"widthPx", kWidth},
             {"heightPx", kHeight},
             {"layerCount", kLayerCount},
             {"dpiX", 635},
             {"dpiY", 600},
             {"originMm", slicer_core::Json::array({0.0, 0.0, 0.0})},
             {"pixelSizeXmm", 25.4 / 635.0},
             {"pixelSizeYmm", 25.4 / 600.0},
             {"pixelSizeMm",
              slicer_core::Json::array({25.4 / 635.0, 25.4 / 600.0})},
             {"layerThicknessMm", 0.038},
         })},
        {"tiff",
         slicer_core::Json::object({
             {"channelOrder",
              slicer_core::Json::array({"R", "G", "B", "W", "S", "V"})},
             {"channelCount", 6},
             {"bitDepth", 8},
             {"sampleFormat", "uint"},
             {"planarConfig", "contiguous"},
             {"storageMode", "stripped"},
             {"storage", "stripped"},
             {"tiled", false},
             {"compression", "none"},
             {"rowsPerStrip", 2},
             {"polarity", "black_is_print"},
             {"printValue", 0},
             {"emptyValue", 255},
             {"layers", slicer_core::Json{layers}},
         })},
        {"layers", slicer_core::Json{layers}},
        {"reports",
         slicer_core::Json::object({
             {"slice", reportPath},
         })},
    };

    if (includeCapabilitySummary)
    {
        manifest.emplace(
            "perInstance",
            slicer_core::Json::array({
                slicer_core::Json::object({
                    {"instanceId", "instance-01"},
                    {"modelId", "model-01"},
                    {"layerRange", slicer_core::Json::array({0, 1})},
                    {"printPixels", ChannelCounts(4, 2, 2, 2, 2, 2)},
                    {"emptyPixels", ChannelCounts(20, 22, 22, 22, 22, 22)},
                    {"bboxMm",
                     slicer_core::Json::object({
                         {"min", slicer_core::Json::array({0.0, 0.0, 0.0})},
                         {"max", slicer_core::Json::array({1.0, 1.0, 0.076})},
                     })},
                    {"transformApplied",
                     slicer_core::Json::object({
                         {"identity", true},
                     })},
                }),
            }));
        manifest.emplace(
            "profileEcho",
            slicer_core::Json::object({
                {"profileVersion", "stage14b-02-fixture.1"},
                {"profileHash", "sha256:stage14b02"},
            }));
    }
    return slicer_core::Json{std::move(manifest)};
}

std::filesystem::path MakePackage(
    const std::filesystem::path& root,
    const std::string& schema = "p0.rgbwsv.2",
    const bool includeCapabilitySummary = true,
    const std::string& reportPath = "reports/slice_report.json")
{
    const std::filesystem::path packageDir = root / "package";
    std::filesystem::create_directories(packageDir / "layers");
    std::filesystem::create_directories(packageDir / "reports");
    for (int layerIndex = 0; layerIndex < kLayerCount; ++layerIndex)
    {
        slicer_core::TiffImageSpec spec;
        spec.width = kWidth;
        spec.height = kHeight;
        spec.rows_per_strip = 2U;
        slicer_core::write_rgbwsv_tiff(
            packageDir / "layers"
                / ("layer_" + std::to_string(layerIndex) + ".tiff"),
            spec,
            MakePixels(layerIndex));
    }
    WriteJson(
        packageDir / "manifest.json",
        MakeManifest(schema, includeCapabilitySummary, reportPath));
    WriteJson(
        packageDir / "reports" / "slice_report.json",
        slicer_core::Json::object({
            {"schema", "p0.report.slice.1"},
            {"status", "fixture"},
        }));
    return packageDir;
}

std::filesystem::path RepositoryRoot()
{
    return std::filesystem::path{__FILE__}
        .parent_path()
        .parent_path()
        .parent_path();
}

bool ModelFacadeUsesAuthoritativeLoader()
{
    std::unique_ptr<slicer_core::api::ModelFacade> facade =
        slicer_core::api::implementation::CreateModelFacade();
    CancelToken active;
    slicer_core::api::ModelImportRequest request;
    request.model_path = RepositoryRoot()
        / "tests" / "fixtures" / "stage14b" / "model_with_normals.obj";

    const auto imported = facade->Import(request, active);
    if (!ExpectTrue(imported.IsOk(), "model import succeeds"))
    {
        if (imported.Error() != nullptr)
        {
            std::cerr << imported.Error()->detail << '\n';
        }
        return false;
    }
    const slicer_core::api::ModelMetadata& metadata = *imported.Value();
    const auto cached = facade->GetMetadata(metadata.model_id);
    const bool importedEvidence =
        ExpectTrue(metadata.triangle_count == 1U, "triangle count is preserved")
        && ExpectTrue(metadata.vertex_count == 3U, "vertex count is preserved")
        && ExpectTrue(metadata.has_normals, "authoritative normal flag is preserved")
        && ExpectTrue(!metadata.has_uv, "missing UV remains false")
        && ExpectTrue(metadata.source_digest.size() == 64U, "source digest is SHA-256")
        && ExpectTrue(metadata.mesh_identity.size() == 64U, "mesh identity is authoritative")
        && ExpectTrue(metadata.appearance_identity.size() == 64U, "appearance identity is authoritative")
        && ExpectTrue(cached.IsOk(), "metadata remains available by handle")
        && ExpectTrue(
            cached.Value()->source_digest == metadata.source_digest,
            "cached metadata keeps source identity");
    const auto released = facade->Release(metadata.model_id);
    const auto missing = facade->GetMetadata(metadata.model_id);
    return importedEvidence
        && ExpectTrue(released.IsOk(), "model release succeeds")
        && ExpectTrue(!missing.IsOk(), "released handle is rejected")
        && ExpectErrorCode(
            missing.Error(),
            "PM-SLICER-INPUT-0001",
            "released handle");
}

bool ModelFacadeFailsClosed()
{
    std::unique_ptr<slicer_core::api::ModelFacade> facade =
        slicer_core::api::implementation::CreateModelFacade();
    slicer_core::api::ModelImportRequest request;
    request.model_path = RepositoryRoot()
        / "tests" / "fixtures" / "stage14b" / "model_with_normals.obj";
    CancelToken cancelled{true};
    const auto cancelledResult = facade->Import(request, cancelled);
    request.model_path = RepositoryRoot() / "missing-stage14b-02.obj";
    CancelToken active;
    const auto missingResult = facade->Import(request, active);
    return ExpectTrue(!cancelledResult.IsOk(), "cancelled import fails")
        && ExpectErrorCode(
            cancelledResult.Error(),
            "PM-SLICER-CANCELLED-0070",
            "cancelled import")
        && ExpectTrue(!missingResult.IsOk(), "missing model fails")
        && ExpectErrorCode(
            missingResult.Error(),
            "PM-SLICER-INPUT-0001",
            "missing model");
}

bool PackageSummaryAndLayerDescriptorAreAuthoritative()
{
    TemporaryDirectory directory{"package_queries"};
    const std::filesystem::path packageDir =
        MakePackage(directory.Path());
    std::unique_ptr<slicer_core::api::PackageQueryFacade> facade =
        slicer_core::api::implementation::CreatePackageQueryFacade();

    const auto summary = facade->GetSummary(packageDir);
    const auto layer = facade->GetLayerDescriptor(packageDir, 0);
    if (!ExpectTrue(summary.IsOk(), "package summary succeeds")
        || !ExpectTrue(layer.IsOk(), "layer descriptor succeeds"))
    {
        if (summary.Error() != nullptr)
        {
            std::cerr << summary.Error()->detail << '\n';
        }
        if (layer.Error() != nullptr)
        {
            std::cerr << layer.Error()->detail << '\n';
        }
        return false;
    }

    const auto missingLayer = facade->GetLayerDescriptor(packageDir, 9);
    return ExpectTrue(
               summary.Value()->schema == "p0.rgbwsv.2",
               "summary schema is current")
        && ExpectTrue(summary.Value()->layer_count == 2, "summary layer count")
        && ExpectTrue(summary.Value()->grid.dpi_x == 635, "summary dpiX")
        && ExpectTrue(summary.Value()->grid.dpi_y == 600, "summary dpiY")
        && ExpectTrue(!summary.Value()->package_identity.empty(), "package identity")
        && ExpectTrue(summary.Value()->per_instance.size() == 1U, "per-instance evidence")
        && ExpectTrue(
            summary.Value()->profile_echo.utf8_json.find("profileHash")
                != std::string::npos,
            "profile echo is returned")
        && ExpectTrue(layer.Value()->width_px == kWidth, "layer width")
        && ExpectTrue(layer.Value()->height_px == kHeight, "layer height")
        && ExpectTrue(layer.Value()->print_pixels.at(0U) == 2U, "R print pixels")
        && ExpectTrue(layer.Value()->print_pixels.at(3U) == 1U, "W print pixels")
        && ExpectTrue(layer.Value()->print_pixels.at(4U) == 1U, "S print pixels")
        && ExpectTrue(layer.Value()->empty_pixels.at(5U) == 11U, "V empty pixels")
        && ExpectTrue(layer.Value()->storage_mode == "stripped", "storage mode")
        && ExpectTrue(!missingLayer.IsOk(), "unlisted layer fails")
        && ExpectErrorCode(
            missingLayer.Error(),
            "PM-SLICER-INPUT-0001",
            "unlisted layer");
}

bool PackagePreviewUsesProductionTiff()
{
    TemporaryDirectory directory{"preview"};
    const std::filesystem::path packageDir =
        MakePackage(directory.Path());
    std::unique_ptr<slicer_core::api::PackageQueryFacade> facade =
        slicer_core::api::implementation::CreatePackageQueryFacade();
    CancelToken active;

    slicer_core::api::PreviewRequest request;
    request.package_dir = packageDir;
    request.layer_index = 0;
    request.mode = "composite";
    request.channels = {"R", "G", "B", "W", "S", "V"};
    request.max_width_px = 2;
    request.output_path = directory.Path() / "preview.bmp";
    const auto bmp = facade->RenderLayerPreview(request, active);

    request.output_path = directory.Path() / "preview.png";
    const auto png = facade->RenderLayerPreview(request, active);
    std::array<unsigned char, 8> pngSignature{};
    if (png.IsOk())
    {
        std::ifstream input{png.Value()->output_path, std::ios::binary};
        input.read(
            reinterpret_cast<char*>(pngSignature.data()),
            static_cast<std::streamsize>(pngSignature.size()));
    }

    CancelToken cancelled{true};
    const auto cancelledResult =
        facade->RenderLayerPreview(request, cancelled);
    return ExpectTrue(bmp.IsOk(), "BMP preview is rendered")
        && ExpectTrue(png.IsOk(), "PNG preview is rendered")
        && ExpectTrue(bmp.Value()->width_px == 2, "preview max width is honored")
        && ExpectTrue(bmp.Value()->height_px == 2, "preview aspect ratio is retained")
        && ExpectTrue(
            bmp.Value()->cache_key.find("|layer:0|") != std::string::npos,
            "cache key contains real layer index")
        && ExpectTrue(
            bmp.Value()->cache_key.find("|ch:RGBWSV|") != std::string::npos,
            "cache key contains canonical channels")
        && ExpectTrue(
            pngSignature
                == std::array<unsigned char, 8>{
                    0x89U, 0x50U, 0x4eU, 0x47U,
                    0x0dU, 0x0aU, 0x1aU, 0x0aU},
            "PNG signature is valid")
        && ExpectTrue(!cancelledResult.IsOk(), "cancelled preview fails")
        && ExpectErrorCode(
            cancelledResult.Error(),
            "PM-SLICER-CANCELLED-0070",
            "cancelled preview");
}

bool VerifyAndReportReadingAreStrict()
{
    TemporaryDirectory directory{"verify_report"};
    const std::filesystem::path packageDir =
        MakePackage(directory.Path());
    std::unique_ptr<slicer_core::api::PackageQueryFacade> facade =
        slicer_core::api::implementation::CreatePackageQueryFacade();
    CancelToken active;
    const auto verified = facade->Verify(packageDir, active);
    const auto report = facade->ReadReport(packageDir, "slice");

    TemporaryDirectory invalidDirectory{"invalid_schema"};
    const std::filesystem::path invalidPackage = MakePackage(
        invalidDirectory.Path(),
        "p0.invalid.1");
    const auto invalid = facade->Verify(invalidPackage, active);

    TemporaryDirectory incompleteDirectory{"incomplete_summary"};
    const std::filesystem::path incompletePackage = MakePackage(
        incompleteDirectory.Path(),
        "p0.rgbwsv.2",
        false);
    const auto incomplete = facade->GetSummary(incompletePackage);

    TemporaryDirectory escapeDirectory{"report_escape"};
    const std::filesystem::path escapedPackage = MakePackage(
        escapeDirectory.Path(),
        "p0.rgbwsv.2",
        true,
        "../outside.json");
    WriteJson(
        escapeDirectory.Path() / "outside.json",
        slicer_core::Json::object({
            {"schema", "p0.report.escape.1"},
        }));
    const auto escaped = facade->ReadReport(escapedPackage, "slice");

    return ExpectTrue(verified.IsOk(), "valid package verification returns")
        && ExpectTrue(verified.Value()->valid, "valid package is accepted")
        && ExpectTrue(verified.Value()->layer_count == 2, "verify layer count")
        && ExpectTrue(
            verified.Value()->per_layer_checksum.size() == 2U,
            "verify returns per-layer checksums")
        && ExpectTrue(report.IsOk(), "listed report is read")
        && ExpectTrue(
            report.Value()->report_schema == "p0.report.slice.1",
            "report schema is authoritative")
        && ExpectTrue(invalid.IsOk(), "invalid package returns verify result")
        && ExpectTrue(!invalid.Value()->valid, "invalid schema is rejected")
        && ExpectTrue(!invalid.Value()->errors.empty(), "verify error is structured")
        && ExpectTrue(!incomplete.IsOk(), "missing v1.2 summary evidence fails")
        && ExpectErrorCode(
            incomplete.Error(),
            "PM-SLICER-CONTRACT-0060",
            "incomplete summary")
        && ExpectTrue(!escaped.IsOk(), "escaping report path fails")
        && ExpectErrorCode(
            escaped.Error(),
            "PM-SLICER-CONTRACT-0060",
            "escaping report path");
}

}  // namespace

int main()
{
    const std::vector<std::pair<std::string, bool (*)()>> tests{
        {"model_facade_uses_authoritative_loader", ModelFacadeUsesAuthoritativeLoader},
        {"model_facade_fails_closed", ModelFacadeFailsClosed},
        {"package_summary_and_layer_descriptor_are_authoritative", PackageSummaryAndLayerDescriptorAreAuthoritative},
        {"package_preview_uses_production_tiff", PackagePreviewUsesProductionTiff},
        {"verify_and_report_reading_are_strict", VerifyAndReportReadingAreStrict},
    };

    int failures{0};
    for (const auto& [name, test] : tests)
    {
        try
        {
            if (test())
            {
                std::cout << "PASS: " << name << '\n';
            }
            else
            {
                ++failures;
            }
        }
        catch (const std::exception& error)
        {
            ++failures;
            std::cerr << "FAILED: " << name << ": " << error.what() << '\n';
        }
    }

    if (failures != 0)
    {
        std::cerr << failures << " Stage 14B-02 test(s) failed\n";
        return 1;
    }
    std::cout << "All Stage 14B-02 tests passed\n";
    return 0;
}
