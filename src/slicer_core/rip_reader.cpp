#include "slicer_core/rip_reader.h"

#include "slicer_core/config/OutputResolution.h"
#include "slicer_core/json_value.h"
#include "slicer_core/TiffReadApi.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>

namespace slicer_core {
namespace {

constexpr const char* legacy_schema = "p0.rgbwsv.1";
constexpr const char* current_schema = "p0.rgbwsv.2";

Json read_json_file(const std::filesystem::path& path) {
    std::ifstream input{path};
    if (!input) {
        throw ValidationError(
            ValidationErrorCode::ManifestMissing,
            "manifest missing: field=manifest path=" + path.string());
    }
    try {
        return Json::parse(input);
    } catch (const std::exception& error) {
        throw ValidationError(
            ValidationErrorCode::ManifestParseFailed,
            "manifest parse failed: field=manifest path=" + path.string() + " actual=" + error.what());
    }
}

[[noreturn]] void fail(
    const ValidationErrorCode code,
    const std::string& field,
    const std::string& expected,
    const std::string& actual,
    const std::filesystem::path& path = {}) {
    std::ostringstream message;
    message << field << " expected " << expected << ", actual " << actual;
    if (!path.empty()) {
        message << ", path " << path.string();
    }
    throw ValidationError(code, message.str());
}

std::string json_type_name(const Json& value) {
    if (value.is_array()) {
        return "array";
    }
    if (value.is_object()) {
        return "object";
    }
    if (value.is_string()) {
        return "string";
    }
    if (value.is_bool()) {
        return "bool";
    }
    if (value.is_number()) {
        return "number";
    }
    return "null";
}

std::string int_to_string(const int value) {
    return std::to_string(value);
}

std::string NumberToString(const double value)
{
    std::ostringstream output;
    output.precision(15);
    output << value;
    return output.str();
}

double RequireNumberValue(
    const Json& value,
    const ValidationErrorCode code,
    const std::string& field)
{
    if (!value.is_number())
    {
        fail(code, field, "number", json_type_name(value));
    }
    const double actual = value.as_double();
    if (!std::isfinite(actual))
    {
        fail(code, field, "finite number", NumberToString(actual));
    }
    return actual;
}

double RequireNumber(
    const Json& object,
    const std::string& key,
    const ValidationErrorCode code,
    const std::string& field)
{
    if (!object.contains(key))
    {
        fail(code, field, "number", "missing");
    }
    return RequireNumberValue(object.at(key), code, field);
}

int RequireIntegerValue(
    const Json& value,
    const ValidationErrorCode code,
    const std::string& field)
{
    const double actual = RequireNumberValue(value, code, field);
    if (std::floor(actual) != actual
        || actual < static_cast<double>(std::numeric_limits<int>::min())
        || actual > static_cast<double>(std::numeric_limits<int>::max()))
    {
        fail(code, field, "integer", NumberToString(actual));
    }
    return static_cast<int>(actual);
}

int RequireInteger(
    const Json& object,
    const std::string& key,
    const ValidationErrorCode code,
    const std::string& field)
{
    if (!object.contains(key))
    {
        fail(code, field, "integer", "missing");
    }
    return RequireIntegerValue(object.at(key), code, field);
}

void require_channel_order(const Json& channel_order) {
    const std::array<const char*, 6> expected{"R", "G", "B", "W", "S", "V"};
    if (!channel_order.is_array() || channel_order.size() != expected.size()) {
        fail(
            ValidationErrorCode::ChannelOrderInvalid,
            "manifest.tiff.channelOrder",
            "array[R,G,B,W,S,V]",
            json_type_name(channel_order));
    }
    for (std::size_t i{0}; i < expected.size(); ++i) {
        const std::string actual = channel_order.at(i).as_string();
        if (actual != expected.at(i)) {
            fail(
                ValidationErrorCode::ChannelOrderInvalid,
                "manifest.tiff.channelOrder[" + std::to_string(i) + "]",
                expected.at(i),
                actual);
        }
    }
}

void require_int(
    const Json& object,
    const std::string& key,
    const int expected,
    const ValidationErrorCode code,
    const std::string& field) {
    const int actual = object.at(key).as_int();
    if (actual != expected) {
        fail(code, field, int_to_string(expected), int_to_string(actual));
    }
}

void require_string(
    const Json& object,
    const std::string& key,
    const std::string& expected,
    const ValidationErrorCode code,
    const std::string& field) {
    const std::string actual = object.at(key).as_string();
    if (actual != expected) {
        fail(code, field, expected, actual);
    }
}

std::string read_manifest_storage_mode(const Json& tiff, const std::string& schema) {
    std::string storage_mode;
    if (tiff.contains("storageMode")) {
        storage_mode = tiff.at("storageMode").as_string();
    } else if (tiff.contains("storage")) {
        storage_mode = tiff.at("storage").as_string();
    } else if (tiff.contains("tiled")) {
        storage_mode = tiff.at("tiled").as_bool() ? "tiled" : "stripped";
    } else if (schema == legacy_schema) {
        storage_mode = "tiled";
    } else {
        fail(
            ValidationErrorCode::TiffStorageModeInvalid,
            "manifest.tiff.storageMode",
            "stripped|tiled",
            "missing");
    }

    if (storage_mode != "stripped" && storage_mode != "tiled") {
        fail(
            ValidationErrorCode::TiffStorageModeInvalid,
            "manifest.tiff.storageMode",
            "stripped|tiled",
            storage_mode);
    }
    if (schema == legacy_schema && storage_mode != "tiled") {
        fail(
            ValidationErrorCode::TiffStorageMismatch,
            "manifest.tiff.storageMode",
            "tiled for p0.rgbwsv.1",
            storage_mode);
    }
    if (tiff.contains("tiled")) {
        const bool tiled = tiff.at("tiled").as_bool();
        if ((storage_mode == "tiled") != tiled) {
            fail(
                ValidationErrorCode::TiffStorageMismatch,
                "manifest.tiff.tiled",
                storage_mode == "tiled" ? "true" : "false",
                tiled ? "true" : "false");
        }
    }
    return storage_mode;
}

std::string ReadManifestCompression(const Json& tiff)
{
    if (!tiff.contains("compression"))
    {
        return "none";
    }
    const Json& compression = tiff.at("compression");
    if (!compression.is_string())
    {
        fail(
            ValidationErrorCode::TiffCompressionInvalid,
            "manifest.tiff.compression",
            "none|packbits",
            json_type_name(compression));
    }
    const std::string name = compression.as_string();
    try
    {
        (void)ParseTiffCompressionMode(name);
    }
    catch (const std::invalid_argument&)
    {
        fail(
            ValidationErrorCode::TiffCompressionInvalid,
            "manifest.tiff.compression",
            "none|packbits",
            name);
    }
    return name;
}

std::optional<std::string> ReadWhiteSemantics(const Json& manifest)
{
    if (!manifest.contains("whiteSemantics"))
    {
        return std::nullopt;
    }
    const Json& value = manifest.at("whiteSemantics");
    if (!value.is_string())
    {
        fail(
            ValidationErrorCode::WhiteSemanticsInvalid,
            "manifest.whiteSemantics",
            "opaque|transparent",
            json_type_name(value));
    }
    const std::string semantics = value.as_string();
    if (semantics != "opaque" && semantics != "transparent")
    {
        fail(
            ValidationErrorCode::WhiteSemanticsInvalid,
            "manifest.whiteSemantics",
            "opaque|transparent",
            semantics);
    }
    return semantics;
}

void validate_manifest_storage_fields(const Json& tiff, const std::string& storage_mode) {
    if (storage_mode == "stripped") {
        const int rows_per_strip = tiff.contains("rowsPerStrip") ? tiff.at("rowsPerStrip").as_int() : 0;
        if (rows_per_strip <= 0) {
            fail(
                ValidationErrorCode::RowsPerStripInvalid,
                "manifest.tiff.rowsPerStrip",
                "> 0",
                int_to_string(rows_per_strip));
        }
        return;
    }
    if (!tiff.contains("tileSize") || !tiff.at("tileSize").is_array() || tiff.at("tileSize").size() != 2) {
        fail(ValidationErrorCode::TileSizeInvalid, "manifest.tiff.tileSize", "[positive,positive]", "missing");
    }
    const int tile_width = tiff.at("tileSize").at(0).as_int();
    const int tile_height = tiff.at("tileSize").at(1).as_int();
    if (tile_width <= 0 || tile_height <= 0) {
        fail(
            ValidationErrorCode::TileSizeInvalid,
            "manifest.tiff.tileSize",
            "[positive,positive]",
            "[" + int_to_string(tile_width) + "," + int_to_string(tile_height) + "]");
    }
}

std::string actual_storage_mode_string(const TiffReadResult& result) {
    return tiff_storage_mode_string(result.spec.storage_mode);
}

void validate_grid(const Json& grid, RipValidationResult& result) {
    if (!grid.is_object())
    {
        fail(
            ValidationErrorCode::GridInvalid,
            "manifest.grid",
            "object",
            json_type_name(grid));
    }
    result.width_px = RequireInteger(
        grid,
        "widthPx",
        ValidationErrorCode::GridInvalid,
        "manifest.grid.widthPx");
    result.height_px = RequireInteger(
        grid,
        "heightPx",
        ValidationErrorCode::GridInvalid,
        "manifest.grid.heightPx");
    result.layer_count = RequireInteger(
        grid,
        "layerCount",
        ValidationErrorCode::GridInvalid,
        "manifest.grid.layerCount");
    if (result.width_px <= 0) {
        fail(ValidationErrorCode::GridInvalid, "manifest.grid.widthPx", "> 0", int_to_string(result.width_px));
    }
    if (result.height_px <= 0) {
        fail(ValidationErrorCode::GridInvalid, "manifest.grid.heightPx", "> 0", int_to_string(result.height_px));
    }
    if (result.layer_count <= 0) {
        fail(ValidationErrorCode::GridInvalid, "manifest.grid.layerCount", "> 0", int_to_string(result.layer_count));
    }
    result.dpi_x = RequireInteger(
        grid,
        "dpiX",
        ValidationErrorCode::GridInvalid,
        "manifest.grid.dpiX");
    result.dpi_y = RequireInteger(
        grid,
        "dpiY",
        ValidationErrorCode::GridInvalid,
        "manifest.grid.dpiY");
    if (!IsSupportedOutputDpi(result.dpi_x))
    {
        fail(
            ValidationErrorCode::GridInvalid,
            "manifest.grid.dpiX",
            "integer "
                + int_to_string(kMinimumOutputDpi)
                + ".."
                + int_to_string(kMaximumOutputDpi),
            int_to_string(result.dpi_x));
    }
    if (!IsSupportedOutputDpi(result.dpi_y))
    {
        fail(
            ValidationErrorCode::GridInvalid,
            "manifest.grid.dpiY",
            "integer "
                + int_to_string(kMinimumOutputDpi)
                + ".."
                + int_to_string(kMaximumOutputDpi),
            int_to_string(result.dpi_y));
    }
    if (grid.contains("dpi"))
    {
        const Json& dpi = grid.at("dpi");
        if (!dpi.is_array() || dpi.size() != 2U)
        {
            fail(
                ValidationErrorCode::GridInvalid,
                "manifest.grid.dpi",
                "[dpiX,dpiY]",
                json_type_name(dpi));
        }
        const int redundantDpiX = RequireIntegerValue(
            dpi.at(0U),
            ValidationErrorCode::GridInvalid,
            "manifest.grid.dpi[0]");
        const int redundantDpiY = RequireIntegerValue(
            dpi.at(1U),
            ValidationErrorCode::GridInvalid,
            "manifest.grid.dpi[1]");
        if (redundantDpiX != result.dpi_x
            || redundantDpiY != result.dpi_y)
        {
            fail(
                ValidationErrorCode::GridInvalid,
                "manifest.grid.dpi",
                "["
                    + int_to_string(result.dpi_x)
                    + ","
                    + int_to_string(result.dpi_y)
                    + "]",
                "["
                    + int_to_string(redundantDpiX)
                    + ","
                    + int_to_string(redundantDpiY)
                    + "]");
        }
    }

    result.pixel_size_x_mm = RequireNumber(
        grid,
        "pixelSizeXmm",
        ValidationErrorCode::GridInvalid,
        "manifest.grid.pixelSizeXmm");
    result.pixel_size_y_mm = RequireNumber(
        grid,
        "pixelSizeYmm",
        ValidationErrorCode::GridInvalid,
        "manifest.grid.pixelSizeYmm");
    if (!IsOutputPixelSizeConsistent(
            result.dpi_x,
            result.pixel_size_x_mm))
    {
        fail(
            ValidationErrorCode::GridInvalid,
            "manifest.grid.pixelSizeXmm",
            NumberToString(
                kMillimetersPerInch
                / static_cast<double>(result.dpi_x)),
            NumberToString(result.pixel_size_x_mm));
    }
    if (!IsOutputPixelSizeConsistent(
            result.dpi_y,
            result.pixel_size_y_mm))
    {
        fail(
            ValidationErrorCode::GridInvalid,
            "manifest.grid.pixelSizeYmm",
            NumberToString(
                kMillimetersPerInch
                / static_cast<double>(result.dpi_y)),
            NumberToString(result.pixel_size_y_mm));
    }
    if (grid.contains("pixelSizeMm"))
    {
        const Json& pixelSize = grid.at("pixelSizeMm");
        if (!pixelSize.is_array() || pixelSize.size() != 2U)
        {
            fail(
                ValidationErrorCode::GridInvalid,
                "manifest.grid.pixelSizeMm",
                "[pixelSizeXmm,pixelSizeYmm]",
                json_type_name(pixelSize));
        }
        const double redundantPixelSizeX = RequireNumberValue(
            pixelSize.at(0U),
            ValidationErrorCode::GridInvalid,
            "manifest.grid.pixelSizeMm[0]");
        const double redundantPixelSizeY = RequireNumberValue(
            pixelSize.at(1U),
            ValidationErrorCode::GridInvalid,
            "manifest.grid.pixelSizeMm[1]");
        if (std::abs(redundantPixelSizeX - result.pixel_size_x_mm)
                > kOutputPixelSizeToleranceMm
            || std::abs(redundantPixelSizeY - result.pixel_size_y_mm)
                > kOutputPixelSizeToleranceMm)
        {
            fail(
                ValidationErrorCode::GridInvalid,
                "manifest.grid.pixelSizeMm",
                "["
                    + NumberToString(result.pixel_size_x_mm)
                    + ","
                    + NumberToString(result.pixel_size_y_mm)
                    + "]",
                "["
                    + NumberToString(redundantPixelSizeX)
                    + ","
                    + NumberToString(redundantPixelSizeY)
                    + "]");
        }
    }
    if (grid.contains("layerThicknessMm")
        && RequireNumber(
               grid,
               "layerThicknessMm",
               ValidationErrorCode::GridInvalid,
               "manifest.grid.layerThicknessMm")
            <= 0.0) {
        fail(ValidationErrorCode::GridInvalid, "manifest.grid.layerThicknessMm", "> 0", "non-positive");
    }
}

ValidationErrorCode classify_tiff_error(const std::string& message) {
    if (message.find("failed to open TIFF") != std::string::npos) {
        return ValidationErrorCode::TiffOpenFailed;
    }
    if (message.find("six-channel") != std::string::npos) {
        return ValidationErrorCode::TiffSampleCountInvalid;
    }
    if (message.find("uint8") != std::string::npos) {
        return ValidationErrorCode::TiffBitDepthInvalid;
    }
    if (message.find("planar") != std::string::npos) {
        return ValidationErrorCode::TiffPlanarConfigInvalid;
    }
    return ValidationErrorCode::TiffReadFailed;
}

void merge_channel_stats(
    std::array<TiffChannelStats, rgbwsv_channel_count>& totals,
    const std::array<TiffChannelStats, rgbwsv_channel_count>& layer_stats) {
    for (std::size_t i{0}; i < totals.size(); ++i) {
        totals.at(i).print_pixels += layer_stats.at(i).print_pixels;
        totals.at(i).full_print_pixels += layer_stats.at(i).full_print_pixels;
        totals.at(i).partial_print_pixels += layer_stats.at(i).partial_print_pixels;
        totals.at(i).empty_pixels += layer_stats.at(i).empty_pixels;
        totals.at(i).min_value = std::min(totals.at(i).min_value, layer_stats.at(i).min_value);
        totals.at(i).max_value = std::max(totals.at(i).max_value, layer_stats.at(i).max_value);
    }
}

}  // namespace

std::string validation_error_code_string(const ValidationErrorCode code) {
    switch (code) {
        case ValidationErrorCode::PackageNotFound:
            return "E_PACKAGE_NOT_FOUND";
        case ValidationErrorCode::ManifestMissing:
            return "E_MANIFEST_MISSING";
        case ValidationErrorCode::ManifestParseFailed:
            return "E_MANIFEST_PARSE_FAILED";
        case ValidationErrorCode::SchemaUnsupported:
            return "E_SCHEMA_UNSUPPORTED";
        case ValidationErrorCode::WhiteSemanticsInvalid:
            return "E_WHITE_SEMANTICS_INVALID";
        case ValidationErrorCode::ChannelOrderInvalid:
            return "E_CHANNEL_ORDER_INVALID";
        case ValidationErrorCode::ChannelCountInvalid:
            return "E_CHANNEL_COUNT_INVALID";
        case ValidationErrorCode::BitDepthInvalid:
            return "E_BIT_DEPTH_INVALID";
        case ValidationErrorCode::PolarityInvalid:
            return "E_POLARITY_INVALID";
        case ValidationErrorCode::PrintEmptyValueInvalid:
            return "E_PRINT_EMPTY_VALUE_INVALID";
        case ValidationErrorCode::GridInvalid:
            return "E_GRID_INVALID";
        case ValidationErrorCode::LayerListInvalid:
            return "E_LAYER_LIST_INVALID";
        case ValidationErrorCode::LayerCountMismatch:
            return "E_LAYER_COUNT_MISMATCH";
        case ValidationErrorCode::LayerMissing:
            return "E_LAYER_MISSING";
        case ValidationErrorCode::LayerSizeMismatch:
            return "E_LAYER_SIZE_MISMATCH";
        case ValidationErrorCode::TiffOpenFailed:
            return "E_TIFF_OPEN_FAILED";
        case ValidationErrorCode::TiffSampleCountInvalid:
            return "E_TIFF_SAMPLE_COUNT_INVALID";
        case ValidationErrorCode::TiffBitDepthInvalid:
            return "E_TIFF_BIT_DEPTH_INVALID";
        case ValidationErrorCode::TiffPlanarConfigInvalid:
            return "E_TIFF_PLANAR_CONFIG_INVALID";
        case ValidationErrorCode::TiffStorageModeInvalid:
            return "E_TIFF_STORAGE_MODE_INVALID";
        case ValidationErrorCode::TiffStorageMismatch:
            return "E_TIFF_STORAGE_MISMATCH";
        case ValidationErrorCode::TiffCompressionInvalid:
            return "E_TIFF_COMPRESSION_INVALID";
        case ValidationErrorCode::TiffCompressionMismatch:
            return "E_TIFF_COMPRESSION_MISMATCH";
        case ValidationErrorCode::RowsPerStripInvalid:
            return "E_ROWS_PER_STRIP_INVALID";
        case ValidationErrorCode::TileSizeInvalid:
            return "E_TILE_SIZE_INVALID";
        case ValidationErrorCode::TiffReadFailed:
            return "E_TIFF_READ_FAILED";
    }
    return "E_UNKNOWN";
}

ValidationError::ValidationError(const ValidationErrorCode code, const std::string& message)
    : std::runtime_error(validation_error_code_string(code) + ": " + message),
      code_(code) {}

ValidationErrorCode ValidationError::code() const noexcept {
    return code_;
}

RipValidationResult validate_slice_package(const std::filesystem::path& package_dir) {
    if (!std::filesystem::exists(package_dir) || !std::filesystem::is_directory(package_dir)) {
        throw ValidationError(
            ValidationErrorCode::PackageNotFound,
            "package directory not found: field=packageDir path=" + package_dir.string());
    }

    const std::filesystem::path manifest_path = package_dir / "manifest.json";
    const Json manifest = read_json_file(manifest_path);
    const std::string schema = manifest.value<std::string>("schema", "");
    if (schema != legacy_schema && schema != current_schema) {
        fail(
            ValidationErrorCode::SchemaUnsupported,
            "manifest.schema",
            std::string{legacy_schema} + "|" + current_schema,
            schema,
            manifest_path);
    }

    if (!manifest.contains("grid"))
    {
        fail(
            ValidationErrorCode::GridInvalid,
            "manifest.grid",
            "object",
            "missing",
            manifest_path);
    }
    const auto& grid = manifest.at("grid");
    const auto& tiff = manifest.at("tiff");
    const std::string manifest_storage_mode = read_manifest_storage_mode(tiff, schema);
    const std::string manifestCompression = ReadManifestCompression(tiff);
    require_channel_order(tiff.at("channelOrder"));
    require_int(tiff, "channelCount", 6, ValidationErrorCode::ChannelCountInvalid, "manifest.tiff.channelCount");
    require_int(tiff, "bitDepth", 8, ValidationErrorCode::BitDepthInvalid, "manifest.tiff.bitDepth");
    require_string(tiff, "sampleFormat", "uint", ValidationErrorCode::BitDepthInvalid, "manifest.tiff.sampleFormat");
    require_string(
        tiff,
        "planarConfig",
        "contiguous",
        ValidationErrorCode::TiffPlanarConfigInvalid,
        "manifest.tiff.planarConfig");
    validate_manifest_storage_fields(tiff, manifest_storage_mode);
    require_string(
        tiff,
        "polarity",
        "black_is_print",
        ValidationErrorCode::PolarityInvalid,
        "manifest.tiff.polarity");
    require_int(tiff, "printValue", 0, ValidationErrorCode::PrintEmptyValueInvalid, "manifest.tiff.printValue");
    require_int(tiff, "emptyValue", 255, ValidationErrorCode::PrintEmptyValueInvalid, "manifest.tiff.emptyValue");

    RipValidationResult result;
    result.package_dir = package_dir;
    result.schema = schema;
    result.white_semantics = ReadWhiteSemantics(manifest);
    result.storage_mode = manifest_storage_mode;
    result.compression = manifestCompression;
    result.bit_depth = tiff.at("bitDepth").as_int();
    validate_grid(grid, result);

    const Json* layers = nullptr;
    if (manifest.contains("layers")) {
        layers = &manifest.at("layers");
    } else if (tiff.contains("layers")) {
        layers = &tiff.at("layers");
    } else {
        fail(ValidationErrorCode::LayerListInvalid, "manifest.layers", "array", "missing", manifest_path);
    }

    if (!layers->is_array()) {
        fail(ValidationErrorCode::LayerListInvalid, "manifest.layers", "array", json_type_name(*layers), manifest_path);
    }
    if (layers->size() != static_cast<std::size_t>(result.layer_count)) {
        fail(
            ValidationErrorCode::LayerCountMismatch,
            "manifest.layers.size",
            int_to_string(result.layer_count),
            int_to_string(static_cast<int>(layers->size())),
            manifest_path);
    }

    for (const auto& layer : layers->as_array()) {
        const int index{layer.at("index").as_int()};
        if (index < 0 || index >= result.layer_count) {
            fail(
                ValidationErrorCode::LayerListInvalid,
                "manifest.layers[].index",
                "0.." + int_to_string(result.layer_count - 1),
                int_to_string(index),
                manifest_path);
        }
        if (layer.contains("widthPx") && layer.at("widthPx").as_int() != result.width_px) {
            fail(
                ValidationErrorCode::LayerSizeMismatch,
                "manifest.layers[].widthPx",
                int_to_string(result.width_px),
                int_to_string(layer.at("widthPx").as_int()),
                manifest_path);
        }
        if (layer.contains("heightPx") && layer.at("heightPx").as_int() != result.height_px) {
            fail(
                ValidationErrorCode::LayerSizeMismatch,
                "manifest.layers[].heightPx",
                int_to_string(result.height_px),
                int_to_string(layer.at("heightPx").as_int()),
                manifest_path);
        }

        const std::filesystem::path layer_path = package_dir / layer.at("path").as_string();
        if (!std::filesystem::exists(layer_path)) {
            throw ValidationError(
                ValidationErrorCode::LayerMissing,
                "manifest.layers[].path expected existing layer, actual missing, path " + layer_path.string());
        }

        TiffReadResult tiff_result;
        try {
            tiff_result = read_rgbwsv_tiff(layer_path);
        } catch (const ValidationError&) {
            throw;
        } catch (const std::exception& error) {
            const ValidationErrorCode code = classify_tiff_error(error.what());
            throw ValidationError(code, std::string{"TIFF validation failed: field=layer path="}
                                            + layer_path.string() + " actual=" + error.what());
        }
        const std::string actual_storage_mode = actual_storage_mode_string(tiff_result);
        if (actual_storage_mode != manifest_storage_mode) {
            fail(
                ValidationErrorCode::TiffStorageMismatch,
                "layer.tiff.storageMode",
                manifest_storage_mode,
                actual_storage_mode,
                layer_path);
        }
        const std::string actualCompression =
            TiffCompressionModeString(tiff_result.spec.compression_mode);
        if (actualCompression != manifestCompression)
        {
            fail(
                ValidationErrorCode::TiffCompressionMismatch,
                "layer.tiff.compression",
                manifestCompression,
                actualCompression,
                layer_path);
        }

        if (tiff_result.spec.width != static_cast<std::uint32_t>(result.width_px)
            || tiff_result.spec.height != static_cast<std::uint32_t>(result.height_px)) {
            fail(
                ValidationErrorCode::LayerSizeMismatch,
                "layer.tiff.dimensions",
                int_to_string(result.width_px) + "x" + int_to_string(result.height_px),
                std::to_string(tiff_result.spec.width) + "x" + std::to_string(tiff_result.spec.height),
                layer_path);
        }
        merge_channel_stats(result.total_channel_stats, tiff_result.channel_stats);
        result.layer_checksums.push_back({index, tiff_result.channel_checksums});
    }

    std::sort(result.layer_checksums.begin(), result.layer_checksums.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.index < rhs.index;
    });

    return result;
}

}  // namespace slicer_core
