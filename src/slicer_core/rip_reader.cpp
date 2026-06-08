#include "slicer_core/rip_reader.h"

#include "slicer_core/json_value.h"
#include "slicer_core/tiff_io.h"

#include <algorithm>
#include <fstream>
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
    result.width_px = grid.at("widthPx").as_int();
    result.height_px = grid.at("heightPx").as_int();
    result.layer_count = grid.at("layerCount").as_int();
    if (result.width_px <= 0) {
        fail(ValidationErrorCode::GridInvalid, "manifest.grid.widthPx", "> 0", int_to_string(result.width_px));
    }
    if (result.height_px <= 0) {
        fail(ValidationErrorCode::GridInvalid, "manifest.grid.heightPx", "> 0", int_to_string(result.height_px));
    }
    if (result.layer_count <= 0) {
        fail(ValidationErrorCode::GridInvalid, "manifest.grid.layerCount", "> 0", int_to_string(result.layer_count));
    }
    if (grid.contains("dpiX") && grid.at("dpiX").as_int() != 600) {
        fail(ValidationErrorCode::GridInvalid, "manifest.grid.dpiX", "600", int_to_string(grid.at("dpiX").as_int()));
    }
    if (grid.contains("dpiY") && grid.at("dpiY").as_int() != 600) {
        fail(ValidationErrorCode::GridInvalid, "manifest.grid.dpiY", "600", int_to_string(grid.at("dpiY").as_int()));
    }
    if (grid.contains("layerThicknessMm") && grid.at("layerThicknessMm").as_double() <= 0.0) {
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

    const auto& grid = manifest.at("grid");
    const auto& tiff = manifest.at("tiff");
    const std::string manifest_storage_mode = read_manifest_storage_mode(tiff, schema);
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

        if (tiff_result.spec.width != static_cast<std::uint32_t>(result.width_px)
            || tiff_result.spec.height != static_cast<std::uint32_t>(result.height_px)) {
            fail(
                ValidationErrorCode::LayerSizeMismatch,
                "layer.tiff.dimensions",
                int_to_string(result.width_px) + "x" + int_to_string(result.height_px),
                std::to_string(tiff_result.spec.width) + "x" + std::to_string(tiff_result.spec.height),
                layer_path);
        }
        result.layer_checksums.push_back({index, tiff_result.channel_checksums});
    }

    std::sort(result.layer_checksums.begin(), result.layer_checksums.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.index < rhs.index;
    });

    return result;
}

}  // namespace slicer_core
