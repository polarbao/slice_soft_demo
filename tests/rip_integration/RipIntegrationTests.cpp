#include "rip_integration/RipArtifactPublisher.h"
#include "rip_integration/RipCommandBuilder.h"
#include "rip_integration/RipInputValidator.h"
#include "rip_integration/RipOutputValidator.h"
#include "rip_integration/RipSettings.h"

#include <tiffio.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace
{

using slicesoft::rip::BuildRipCommand;
using slicesoft::rip::PublishRipArtifact;
using slicesoft::rip::RipArtifactPublishRequest;
using slicesoft::rip::RipCommand;
using slicesoft::rip::RipCommandRequest;
using slicesoft::rip::RipInputValidationRequest;
using slicesoft::rip::RipOutputValidationRequest;
using slicesoft::rip::RipOutputValidationMode;
using slicesoft::rip::RipSettings;
using slicesoft::rip::ValidateAndNormalizeRipOutput;
using slicesoft::rip::ValidateRipInput;
using slicesoft::rip::ValidateRipSettings;

class TemporaryRoot final
{
public:
    explicit TemporaryRoot(const std::string& name)
    {
        const auto ticks = std::chrono::steady_clock::now()
            .time_since_epoch().count();
        m_path = std::filesystem::temp_directory_path()
            / ("slicesoft-rip-tests-" + name + "-"
               + std::to_string(ticks));
        std::filesystem::create_directories(m_path);
    }

    ~TemporaryRoot()
    {
        std::error_code ignored;
        std::filesystem::remove_all(m_path, ignored);
    }

    [[nodiscard]] const std::filesystem::path& Path() const
    {
        return m_path;
    }

private:
    std::filesystem::path m_path;
};

bool Expect(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
    }
    return condition;
}

void Touch(const std::filesystem::path& path)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.put('\0');
}

TIFF* OpenTiffForWrite(const std::filesystem::path& path)
{
#ifdef _WIN32
    return TIFFOpenW(path.c_str(), "w");
#else
    return TIFFOpen(path.c_str(), "w");
#endif
}

struct TiffFixture
{
    std::uint32_t width{2U};
    std::uint32_t height{2U};
    std::uint16_t bits_per_sample{8U};
    std::uint16_t sample_format{SAMPLEFORMAT_UINT};
    std::uint16_t samples_per_pixel{7U};
    float dpi_x{600.0F};
    float dpi_y{600.0F};
    bool tiled{false};
    bool declare_extra_samples{true};
    bool declare_resolution{true};
    bool declare_resolution_unit{true};
    std::uint16_t compression{COMPRESSION_NONE};
    std::uint8_t white{6U};
    std::uint8_t support{9U};
    std::uint8_t varnish{9U};
};

bool WriteTiff(
    const std::filesystem::path& path,
    const TiffFixture& fixture)
{
    TIFF* output = OpenTiffForWrite(path);
    if (output == nullptr)
    {
        return false;
    }
    const auto close = [&output]()
    {
        TIFFClose(output);
        output = nullptr;
    };
    bool ok = TIFFSetField(output, TIFFTAG_IMAGEWIDTH, fixture.width) == 1
        && TIFFSetField(output, TIFFTAG_IMAGELENGTH, fixture.height) == 1
        && TIFFSetField(
            output, TIFFTAG_BITSPERSAMPLE, fixture.bits_per_sample) == 1
        && TIFFSetField(
            output, TIFFTAG_SAMPLEFORMAT, fixture.sample_format) == 1
        && TIFFSetField(
            output, TIFFTAG_SAMPLESPERPIXEL, fixture.samples_per_pixel) == 1
        && TIFFSetField(output, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG) == 1
        && TIFFSetField(
            output, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_SEPARATED) == 1
        && TIFFSetField(
            output, TIFFTAG_COMPRESSION, fixture.compression) == 1;
    if (fixture.declare_resolution)
    {
        ok = ok
            && TIFFSetField(
                output, TIFFTAG_XRESOLUTION, fixture.dpi_x) == 1
            && TIFFSetField(
                output, TIFFTAG_YRESOLUTION, fixture.dpi_y) == 1;
    }
    if (fixture.declare_resolution && fixture.declare_resolution_unit)
    {
        ok = ok
            && TIFFSetField(
                output, TIFFTAG_RESOLUTIONUNIT, RESUNIT_INCH) == 1;
    }
    std::vector<std::uint16_t> extraSamples;
    if (fixture.declare_extra_samples && fixture.samples_per_pixel > 4U)
    {
        extraSamples.assign(
            fixture.samples_per_pixel - 4U, EXTRASAMPLE_UNSPECIFIED);
        ok = ok && TIFFSetField(
            output,
            TIFFTAG_EXTRASAMPLES,
            static_cast<std::uint16_t>(extraSamples.size()),
            extraSamples.data()) == 1;
    }
    if (fixture.tiled)
    {
        ok = ok
            && TIFFSetField(output, TIFFTAG_TILEWIDTH, 16U) == 1
            && TIFFSetField(output, TIFFTAG_TILELENGTH, 16U) == 1;
    }
    else
    {
        ok = ok
            && TIFFSetField(
                output, TIFFTAG_ROWSPERSTRIP, fixture.height) == 1;
    }
    if (!ok)
    {
        close();
        return false;
    }

    const std::size_t bytesPerSample = fixture.bits_per_sample / 8U;
    const std::size_t pixelBytes =
        fixture.samples_per_pixel * bytesPerSample;
    const std::uint32_t storageWidth = fixture.tiled ? 16U : fixture.width;
    const std::uint32_t storageHeight = fixture.tiled ? 16U : 1U;
    std::vector<std::uint8_t> row(
        static_cast<std::size_t>(storageWidth) * storageHeight * pixelBytes,
        0U);
    for (std::size_t pixel{0U};
         pixel < static_cast<std::size_t>(storageWidth) * storageHeight;
         ++pixel)
    {
        const std::size_t offset = pixel * pixelBytes;
        if (fixture.samples_per_pixel >= 7U)
        {
            if (fixture.bits_per_sample == 8U)
            {
                row[offset + 4U] = fixture.white;
                row[offset + 5U] = fixture.support;
                row[offset + 6U] = fixture.varnish;
            }
            else if (fixture.bits_per_sample == 16U)
            {
                const std::array<std::uint16_t, 3> values{
                    fixture.white, fixture.support, fixture.varnish};
                for (std::size_t channel{0U}; channel < values.size(); ++channel)
                {
                    std::memcpy(
                        row.data() + offset + (4U + channel) * 2U,
                        &values[channel],
                        sizeof(values[channel]));
                }
            }
        }
    }
    if (fixture.tiled)
    {
        ok = TIFFWriteEncodedTile(
            output, 0U, row.data(), static_cast<tmsize_t>(row.size())) >= 0;
    }
    else
    {
        for (std::uint32_t y{0U}; ok && y < fixture.height; ++y)
        {
            ok = TIFFWriteScanline(output, row.data(), y, 0U) >= 0;
        }
    }
    close();
    return ok;
}

bool ReadTiffDimensions(
    const std::filesystem::path& path,
    std::uint32_t* width,
    std::uint32_t* height)
{
#ifdef _WIN32
    TIFF* input = TIFFOpenW(path.c_str(), "r");
#else
    TIFF* input = TIFFOpen(path.c_str(), "r");
#endif
    if (input == nullptr)
    {
        return false;
    }
    const bool ok = TIFFGetField(input, TIFFTAG_IMAGEWIDTH, width) == 1
        && TIFFGetField(input, TIFFTAG_IMAGELENGTH, height) == 1;
    TIFFClose(input);
    return ok;
}

RipSettings ValidSettings(const std::filesystem::path& resources)
{
    RipSettings settings;
    settings.input_icc_path = resources / "CIERGB.icc";
    settings.output_icc_path = resources / "CMYK.icc";
    return settings;
}

RipOutputValidationRequest ValidationRequest(
    const std::filesystem::path& package,
    const std::filesystem::path& stage,
    const std::size_t count,
    const std::uint32_t width = 2U,
    const std::uint32_t height = 2U,
    const int grayBits = 2,
    const RipOutputValidationMode mode =
        RipOutputValidationMode::StrictS2)
{
    return RipOutputValidationRequest{
        package, stage, count, width, height, grayBits, mode};
}

bool TestSettingsAndCommand()
{
    TemporaryRoot root("command");
    const std::filesystem::path module = root.Path() / "module";
    const std::filesystem::path bin = module / "bin";
    const std::filesystem::path resources = module / "resources";
    const std::filesystem::path package = root.Path() / "package";
    const std::filesystem::path layers = package / "layers";
    const std::filesystem::path stage = package / ".rip.staging.command";
    std::filesystem::create_directories(bin);
    std::filesystem::create_directories(resources);
    std::filesystem::create_directories(layers);
    Touch(bin / "rip_cli.exe");
    Touch(bin / "RipSlicer.dll");
    Touch(resources / "CIERGB.icc");
    Touch(resources / "CMYK.icc");

    RipSettings settings = ValidSettings(resources);
    bool pass = Expect(
        !settings.auto_run_after_slice,
        "automatic RIP must be disabled by default")
        && Expect(ValidateRipSettings(settings).ok, "valid settings pass");
    settings.intent = 4;
    pass = Expect(
        !ValidateRipSettings(settings).ok,
        "intent outside 0..3 fails") && pass;
    settings = ValidSettings(resources);
    settings.transparent_mode = 5;
    pass = Expect(
        !ValidateRipSettings(settings).ok,
        "transparent color mode outside 0..4 fails") && pass;
    settings = ValidSettings(resources);
    settings.color_mode = 1;
    pass = Expect(
        !ValidateRipSettings(settings).ok,
        "undocumented color mode fails") && pass;
    settings = ValidSettings(resources);
    settings.gray_bits = 3;
    pass = Expect(
        !ValidateRipSettings(settings).ok,
        "grayBits outside 1/2 fails") && pass;
    settings = ValidSettings(resources);
    settings.output_directory_name = "other";
    pass = Expect(
        !ValidateRipSettings(settings).ok,
        "output name other than rip fails") && pass;

    settings = ValidSettings(resources);
    RipCommand command;
    RipCommandRequest request{
        {module, bin / "rip_cli.exe", bin / "RipSlicer.dll", resources},
        package,
        layers,
        stage,
        settings};
    const auto commandStatus = BuildRipCommand(request, &command);
    pass = Expect(commandStatus.ok, "absolute contained command passes")
        && Expect(command.program.is_absolute(), "program is absolute")
        && Expect(command.arguments.size() == 18U, "all batch arguments exist")
        && pass;
    const auto transparentArgument = std::find(
        command.arguments.begin(), command.arguments.end(), "--transparent");
    pass = Expect(
        transparentArgument != command.arguments.end()
            && std::next(transparentArgument) != command.arguments.end()
            && *std::next(transparentArgument) == "0",
        "default transparent color mode maps to CLI value 0") && pass;

    for (int mode = 0; mode <= 4; ++mode)
    {
        request.settings.transparent_mode = mode;
        const auto modeStatus = BuildRipCommand(request, &command);
        const auto argument = std::find(
            command.arguments.begin(), command.arguments.end(),
            "--transparent");
        pass = Expect(
            modeStatus.ok && argument != command.arguments.end()
                && std::next(argument) != command.arguments.end()
                && *std::next(argument) == std::to_string(mode),
            "transparent color mode maps losslessly to CLI") && pass;
    }

    const std::filesystem::path escaped = root.Path() / "escaped";
    std::filesystem::create_directories(escaped);
    request.input_directory = escaped;
    pass = Expect(
        !BuildRipCommand(request, &command).ok,
        "input outside Package fails containment") && pass;
    return pass;
}

bool TestPositiveValidationAndNumericOrdering()
{
    TemporaryRoot root("positive");
    const std::filesystem::path package = root.Path() / "package";
    const std::filesystem::path stage = package / ".rip.staging.positive";
    std::filesystem::create_directories(stage);
    TiffFixture fixture;
    bool pass{true};
    for (int index{10}; index >= 0; --index)
    {
        pass = Expect(
            WriteTiff(
                stage / ("slice." + std::to_string(index) + ".tiff"),
                fixture),
            "fixture TIFF is written") && pass;
    }
    const auto result = ValidateAndNormalizeRipOutput(
        ValidationRequest(package, stage, 11U));
    pass = Expect(result.status.ok, "valid staged output passes")
        && Expect(result.layers.size() == 11U, "all layers are returned")
        && pass;
    for (std::size_t index{0U}; index < result.layers.size(); ++index)
    {
        char name[32]{};
        std::snprintf(name, sizeof(name), "rip_%06zu.tif", index);
        pass = Expect(
            result.layers[index].layer_index == index,
            "layers use numeric ordering")
            && Expect(
                result.layers[index].path.filename() == name,
                "layer name is normalized")
            && Expect(
                std::filesystem::is_regular_file(result.layers[index].path),
                "normalized layer exists")
            && pass;
    }
    return pass;
}

bool TestVendorAlignedWidthNormalization()
{
    TemporaryRoot root("vendor-width");
    const std::filesystem::path package = root.Path() / "package";
    const std::filesystem::path stage =
        package / ".rip.staging.vendor-width";
    std::filesystem::create_directories(stage);
    TiffFixture fixture;
    fixture.width = 96U;
    fixture.height = 3U;
    fixture.compression = COMPRESSION_LZW;
    fixture.declare_extra_samples = false;
    fixture.declare_resolution_unit = false;
    const std::filesystem::path vendorPath = stage / "slice.0.tiff";
    bool pass = Expect(
        WriteTiff(vendorPath, fixture),
        "vendor-aligned fixture TIFF is written");

    const auto result = ValidateAndNormalizeRipOutput(
        ValidationRequest(package, stage, 1U, 95U, 3U));
    const std::filesystem::path normalized = stage / "rip_000000.tif";
    std::uint32_t normalizedWidth{0U};
    std::uint32_t normalizedHeight{0U};
    pass = Expect(
        result.status.ok,
        "one-to-three-pixel vendor alignment is normalized")
        && Expect(
            result.layers.size() == 1U,
            "normalized vendor output returns one layer")
        && Expect(
            !std::filesystem::exists(vendorPath),
            "padded vendor source is consumed")
        && Expect(
            ReadTiffDimensions(
                normalized, &normalizedWidth, &normalizedHeight),
            "normalized vendor output is readable")
        && Expect(
            normalizedWidth == 95U && normalizedHeight == 3U,
            "normalized vendor output matches the Package grid")
        && pass;
    return pass;
}

bool TestDpiMetadataDoesNotGateRipOutput()
{
    TemporaryRoot root("dpi-metadata");
    const std::filesystem::path package = root.Path() / "package";
    const std::filesystem::path declaredStage =
        package / ".rip.staging.dpi-declared";
    const std::filesystem::path missingStage =
        package / ".rip.staging.dpi-missing";
    std::filesystem::create_directories(declaredStage);
    std::filesystem::create_directories(missingStage);

    TiffFixture declaredFixture;
    declaredFixture.dpi_x = 635.0F;
    bool pass = Expect(
        WriteTiff(declaredStage / "slice.0.tiff", declaredFixture),
        "non-600 DPI metadata fixture is written");
    const auto declaredResult = ValidateAndNormalizeRipOutput(
        ValidationRequest(package, declaredStage, 1U));
    pass = Expect(
        declaredResult.status.ok,
        "non-600 DPI metadata does not gate RIP output") && pass;

    TiffFixture missingFixture;
    missingFixture.declare_resolution = false;
    missingFixture.declare_resolution_unit = false;
    pass = Expect(
        WriteTiff(missingStage / "slice.0.tiff", missingFixture),
        "missing DPI metadata fixture is written") && pass;
    const auto missingResult = ValidateAndNormalizeRipOutput(
        ValidationRequest(package, missingStage, 1U));
    pass = Expect(
        missingResult.status.ok,
        "missing DPI metadata does not gate RIP output") && pass;
    return pass;
}

bool TestDiagnosticOutputRecordsDropViolations()
{
    TemporaryRoot root("diagnostic-drop");
    const std::filesystem::path package = root.Path() / "package";
    const std::filesystem::path stage =
        package / ".rip.staging.diagnostic-drop";
    std::filesystem::create_directories(stage);
    TiffFixture fixture;
    fixture.white = 255U;
    bool pass = Expect(
        WriteTiff(stage / "slice.0.tiff", fixture),
        "diagnostic high-drop fixture is written");

    const auto result = ValidateAndNormalizeRipOutput(
        ValidationRequest(
            package, stage, 1U, 2U, 2U, 2,
            RipOutputValidationMode::DiagnosticUnvalidated));
    pass = Expect(
        result.status.ok,
        "diagnostic mode preserves structurally valid high-drop output")
        && Expect(
            !result.s2_drop_limits_passed,
            "diagnostic result records that S2 drop limits failed")
        && Expect(
            result.samples_exceeding_drop_limit[0] == 4U
                && result.samples_exceeding_drop_limit[1] == 0U
                && result.samples_exceeding_drop_limit[2] == 0U,
            "diagnostic result counts W/S/V violations")
        && Expect(
            result.first_drop_violation.has_value()
                && result.first_drop_violation->layer_index == 0U
                && result.first_drop_violation->channel == "W"
                && result.first_drop_violation->value == 255U
                && result.first_drop_violation->limit == 6U,
            "diagnostic result records the first violation")
        && Expect(
            std::filesystem::is_regular_file(stage / "rip_000000.tif"),
            "diagnostic output is normalized for downstream inspection")
        && pass;
    return pass;
}

bool TestInputValidation()
{
    TemporaryRoot root("input");
    const std::filesystem::path package = root.Path() / "package";
    const std::filesystem::path layers = package / "layers";
    std::filesystem::create_directories(layers);
    TiffFixture fixture;
    fixture.samples_per_pixel = 6U;
    fixture.dpi_x = 635.0F;
    const std::filesystem::path valid = layers / "layer_000000.tiff";
    bool pass = Expect(WriteTiff(valid, fixture), "input fixture is written");
    RipInputValidationRequest request{
        package, layers, {valid}, 2U, 2U};
    pass = Expect(ValidateRipInput(request).ok, "valid S1 input passes")
        && pass;

    request.is_cancelled = []() { return true; };
    const auto cancelledInputStatus = ValidateRipInput(request);
    pass = Expect(
        !cancelledInputStatus.ok
            && cancelledInputStatus.code == "RIP_VALIDATION_CANCELLED",
        "cancelled S1 input validation stops")
        && pass;
    request.is_cancelled = {};

    const std::filesystem::path tiled = layers / "layer_000001.tiff";
    fixture.width = 16U;
    fixture.height = 16U;
    fixture.tiled = true;
    pass = Expect(WriteTiff(tiled, fixture), "tiled input fixture is written")
        && pass;
    request.layer_paths = {tiled};
    request.expected_width_px = 16U;
    request.expected_height_px = 16U;
    const auto tiledStatus = ValidateRipInput(request);
    pass = Expect(
        !tiledStatus.ok
            && tiledStatus.code == "RIP_INPUT_STORAGE_UNSUPPORTED",
        "tiled S1 input fails closed")
        && pass;

    const std::filesystem::path signedSamples =
        layers / "layer_000002.tiff";
    fixture.width = 2U;
    fixture.height = 2U;
    fixture.tiled = false;
    fixture.sample_format = SAMPLEFORMAT_INT;
    pass = Expect(
        WriteTiff(signedSamples, fixture),
        "signed input fixture is written") && pass;
    request.layer_paths = {signedSamples};
    request.expected_width_px = 2U;
    request.expected_height_px = 2U;
    const auto signedStatus = ValidateRipInput(request);
    pass = Expect(
        !signedStatus.ok
            && signedStatus.code == "RIP_INPUT_SAMPLE_LAYOUT_INVALID",
        "signed 8-bit S1 input fails closed")
        && pass;
    return pass;
}

bool ExpectValidationFailure(
    const std::string& caseName,
    const TiffFixture& fixture,
    const RipOutputValidationRequest& baseRequest,
    const std::string& expectedCode,
    const std::size_t fileIndex = 0U)
{
    std::filesystem::create_directories(baseRequest.staging_directory);
    if (!WriteTiff(
            baseRequest.staging_directory
                / ("slice." + std::to_string(fileIndex) + ".tiff"),
            fixture))
    {
        return Expect(false, caseName + " fixture write succeeds");
    }
    const auto result = ValidateAndNormalizeRipOutput(baseRequest);
    return Expect(
        !result.status.ok && result.status.code == expectedCode,
        caseName + " fails with " + expectedCode);
}

bool TestNegativeOutputValidation()
{
    TemporaryRoot root("negative");
    const std::filesystem::path package = root.Path() / "package";
    std::filesystem::create_directories(package);
    bool pass{true};

    TiffFixture dropFixture;
    dropFixture.white = 3U;
    pass = ExpectValidationFailure(
        "grayBits1 W limit",
        dropFixture,
        ValidationRequest(
            package, package / ".rip.staging.drop", 1U,
            2U, 2U, 1),
        "RIP_OUTPUT_DROP_LIMIT_EXCEEDED") && pass;

    TiffFixture cancelledFixture;
    auto cancelledRequest = ValidationRequest(
        package, package / ".rip.staging.cancelled", 1U);
    cancelledRequest.is_cancelled = []() { return true; };
    pass = ExpectValidationFailure(
        "cancelled output validation",
        cancelledFixture,
        cancelledRequest,
        "RIP_VALIDATION_CANCELLED") && pass;

    TiffFixture bitsFixture;
    bitsFixture.bits_per_sample = 16U;
    pass = ExpectValidationFailure(
        "16-bit samples",
        bitsFixture,
        ValidationRequest(
            package, package / ".rip.staging.bits", 1U),
        "RIP_OUTPUT_SAMPLE_LAYOUT_INVALID") && pass;

    TiffFixture sampleFixture;
    sampleFixture.samples_per_pixel = 6U;
    pass = ExpectValidationFailure(
        "six samples",
        sampleFixture,
        ValidationRequest(
            package, package / ".rip.staging.samples", 1U),
        "RIP_OUTPUT_SAMPLE_LAYOUT_INVALID") && pass;

    TiffFixture signedFixture;
    signedFixture.sample_format = SAMPLEFORMAT_INT;
    pass = ExpectValidationFailure(
        "signed 8-bit samples",
        signedFixture,
        ValidationRequest(
            package, package / ".rip.staging.signed", 1U),
        "RIP_OUTPUT_SAMPLE_LAYOUT_INVALID") && pass;

    TiffFixture tiledFixture;
    tiledFixture.width = 16U;
    tiledFixture.height = 16U;
    tiledFixture.tiled = true;
    pass = ExpectValidationFailure(
        "tiled output",
        tiledFixture,
        ValidationRequest(
            package, package / ".rip.staging.tiled", 1U,
            16U, 16U),
        "RIP_OUTPUT_STORAGE_INVALID") && pass;

    TiffFixture dimensionFixture;
    pass = ExpectValidationFailure(
        "dimension mismatch",
        dimensionFixture,
        ValidationRequest(
            package, package / ".rip.staging.dimension", 1U,
            3U, 2U),
        "RIP_OUTPUT_DIMENSION_MISMATCH") && pass;

    TiffFixture excessiveWidthFixture;
    excessiveWidthFixture.width = 97U;
    pass = ExpectValidationFailure(
        "non-alignment width mismatch",
        excessiveWidthFixture,
        ValidationRequest(
            package, package / ".rip.staging.excessive-width", 1U,
            95U, 2U),
        "RIP_OUTPUT_DIMENSION_MISMATCH") && pass;

    TiffFixture gapFixture;
    pass = ExpectValidationFailure(
        "nonzero first index",
        gapFixture,
        ValidationRequest(
            package, package / ".rip.staging.gap", 1U),
        "RIP_OUTPUT_LAYER_INDEX_INVALID",
        1U) && pass;

    const std::filesystem::path outside = root.Path() / ".rip.staging.outside";
    std::filesystem::create_directories(outside);
    pass = Expect(
        !ValidateAndNormalizeRipOutput(
            ValidationRequest(package, outside, 1U)).status.ok,
        "validator rejects staging outside Package") && pass;
    return pass;
}

bool TestArtifactPublication()
{
    TemporaryRoot root("publish");
    const std::filesystem::path package = root.Path() / "package";
    const std::filesystem::path stage = package / ".rip.staging.first";
    std::filesystem::create_directories(stage);
    Touch(stage / "rip_000000.tif");
    const auto published = PublishRipArtifact(
        RipArtifactPublishRequest{package, stage, "rip"});
    bool pass = Expect(published.status.ok, "validated stage publishes")
        && Expect(
            published.output_directory == package / "rip",
            "published directory is Package/rip")
        && Expect(
            !std::filesystem::exists(stage),
            "same-parent rename consumes staging");

    const std::filesystem::path second = package / ".rip.staging.second";
    std::filesystem::create_directories(second);
    const auto collision = PublishRipArtifact(
        RipArtifactPublishRequest{package, second, "rip"});
    pass = Expect(
        !collision.status.ok
            && collision.status.code == "RIP_PUBLISH_OUTPUT_ALREADY_EXISTS",
        "existing RIP output fails closed")
        && Expect(
            std::filesystem::is_directory(second),
            "failed publication preserves owned staging")
        && pass;

    const std::filesystem::path diagnostic =
        package / ".rip.staging.diagnostic";
    std::filesystem::create_directories(diagnostic);
    Touch(diagnostic / "rip_000000.tif");
    const auto diagnosticPublished = PublishRipArtifact(
        RipArtifactPublishRequest{package, diagnostic, "rip_diagnostic"});
    pass = Expect(
        diagnosticPublished.status.ok
            && diagnosticPublished.output_directory
                == package / "rip_diagnostic",
        "diagnostic output publishes to its isolated sibling") && pass;

    const std::filesystem::path unapproved =
        package / ".rip.staging.unapproved";
    std::filesystem::create_directories(unapproved);
    pass = Expect(
        !PublishRipArtifact(
            RipArtifactPublishRequest{package, unapproved, "other"}).status.ok,
        "unapproved publication directory still fails closed") && pass;

    const std::filesystem::path outside = root.Path() / ".rip.staging.outside";
    std::filesystem::create_directories(outside);
    pass = Expect(
        !PublishRipArtifact(
            RipArtifactPublishRequest{package, outside, "rip"}).status.ok,
        "publisher rejects staging outside Package") && pass;
    return pass;
}

}  // namespace

int main()
{
    const bool pass = TestSettingsAndCommand()
        && TestInputValidation()
        && TestPositiveValidationAndNumericOrdering()
        && TestVendorAlignedWidthNormalization()
        && TestDpiMetadataDoesNotGateRipOutput()
        && TestDiagnosticOutputRecordsDropViolations()
        && TestNegativeOutputValidation()
        && TestArtifactPublication();
    if (pass)
    {
        std::cout << "RIP_INTEGRATION_TESTS_PASS\n";
        return 0;
    }
    return 1;
}
