#include "slicer_core/rip_reader.h"

#include "slicer_core/json_value.h"
#include "slicer_core/tiff_io.h"

#include <fstream>
#include <stdexcept>

namespace slicer_core {
namespace {

Json read_json_file(const std::filesystem::path& path) {
    std::ifstream input{path};
    if (!input) {
        throw std::runtime_error("failed to open JSON file: " + path.string());
    }
    return Json::parse(input);
}

void require_channel_order(const Json& channel_order) {
    const std::array<const char*, 6> expected{"R", "G", "B", "W", "S", "V"};
    if (!channel_order.is_array() || channel_order.size() != expected.size()) {
        throw std::runtime_error("manifest tiff.channelOrder must contain six channels");
    }
    for (std::size_t i{0}; i < expected.size(); ++i) {
        if (channel_order.at(i).as_string() != expected.at(i)) {
            throw std::runtime_error("manifest channelOrder must be exactly R G B W S V");
        }
    }
}

}  // namespace

RipValidationResult validate_slice_package(const std::filesystem::path& package_dir) {
    const std::filesystem::path manifest_path = package_dir / "manifest.json";
    if (!std::filesystem::exists(manifest_path)) {
        throw std::runtime_error("manifest does not exist: " + manifest_path.string());
    }
    const Json manifest = read_json_file(manifest_path);
    if (manifest.value<std::string>("schemaVersion", "") != "p0.rgbwsv.1") {
        throw std::runtime_error("unsupported manifest schemaVersion");
    }

    const auto& grid = manifest.at("grid");
    const auto& tiff = manifest.at("tiff");
    require_channel_order(tiff.at("channelOrder"));
    if (tiff.at("channelCount").as_int() != 6) {
        throw std::runtime_error("manifest tiff.channelCount must be 6");
    }
    if (tiff.at("bitDepth").as_int() != 8) {
        throw std::runtime_error("manifest tiff.bitDepth must be 8");
    }
    if (tiff.at("planarConfig").as_string() != "contiguous") {
        throw std::runtime_error("manifest tiff.planarConfig must be contiguous");
    }
    if (tiff.at("storage").as_string() != "tiled") {
        throw std::runtime_error("manifest tiff.storage must be tiled");
    }
    if (tiff.at("polarity").as_string() != "black_is_print") {
        throw std::runtime_error("manifest tiff.polarity must be black_is_print");
    }
    if (tiff.at("printValue").as_int() != 0) {
        throw std::runtime_error("manifest tiff.printValue must be 0");
    }
    if (tiff.at("emptyValue").as_int() != 255) {
        throw std::runtime_error("manifest tiff.emptyValue must be 255");
    }

    RipValidationResult result;
    result.package_dir = package_dir;
    result.width_px = grid.at("widthPx").as_int();
    result.height_px = grid.at("heightPx").as_int();
    result.layer_count = grid.at("layerCount").as_int();

    const auto& layers = tiff.at("layers");
    if (!layers.is_array() || layers.size() != static_cast<std::size_t>(result.layer_count)) {
        throw std::runtime_error("manifest layer count does not match grid.layerCount");
    }

    for (const auto& layer : layers.as_array()) {
        const int index{layer.at("index").as_int()};
        const std::filesystem::path layer_path = package_dir / layer.at("path").as_string();
        if (!std::filesystem::exists(layer_path)) {
            throw std::runtime_error("layer TIFF does not exist: " + layer_path.string());
        }
        const TiffReadResult tiff_result = read_rgbwsv_tiled_tiff(layer_path);
        if (tiff_result.spec.width != static_cast<std::uint32_t>(result.width_px)
            || tiff_result.spec.height != static_cast<std::uint32_t>(result.height_px)) {
            throw std::runtime_error("layer TIFF dimensions do not match manifest: " + layer_path.string());
        }
        result.layer_checksums.push_back({index, tiff_result.channel_checksums});
    }

    return result;
}

}  // namespace slicer_core
