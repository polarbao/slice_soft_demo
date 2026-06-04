#include "slicer_core/config.h"
#include "slicer_core/model.h"
#include "slicer_core/slicer.h"

#include <exception>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

struct CliOptions {
    std::string config_path{"samples/configs/slice_config.json"};
    bool inspect_model{false};
    bool preview_only{false};
    bool show_help{false};
};

void print_usage() {
    std::cout << "Usage: slicer_cli --config <path-to-slice_config.json> [--inspect-model] [--preview-only]\n";
}

CliOptions parse_options(const int argc, char** argv) {
    CliOptions options;
    for (int i{1}; i < argc; ++i) {
        const std::string arg{argv[i]};
        if (arg == "--help" || arg == "-h") {
            options.show_help = true;
            return options;
        }
        if (arg == "--config" && i + 1 < argc) {
            options.config_path = argv[++i];
            continue;
        }
        if (arg == "--inspect-model") {
            options.inspect_model = true;
            continue;
        }
        if (arg == "--preview-only") {
            options.preview_only = true;
            continue;
        }
    }
    return options;
}

double height_mm(const slicer_core::BoundingBox& bbox) {
    return bbox.max.z - bbox.min.z;
}

void print_bbox(const char* label, const slicer_core::BoundingBox& bbox) {
    std::cout << "  " << label << ": [" << bbox.min.x << ", " << bbox.min.y << ", " << bbox.min.z << "] - ["
              << bbox.max.x << ", " << bbox.max.y << ", " << bbox.max.z << "] height=" << height_mm(bbox)
              << "mm\n";
}

int inspect_model(const std::filesystem::path& config_path) {
    const slicer_core::SliceConfig config = slicer_core::load_slice_config(config_path);
    const std::filesystem::path config_dir =
        config_path.parent_path().empty() ? std::filesystem::current_path() : config_path.parent_path();
    const slicer_core::ModelReport report = slicer_core::load_model_report(config, config_dir);
    std::cout << "slicer_cli: model inspection\n";
    std::cout << "  modelPath: " << report.model_path.string() << '\n';
    std::cout << "  format: " << report.format << '\n';
    std::cout << "  vertices: " << report.vertex_count << '\n';
    std::cout << "  autoOrient.enabled: " << (report.auto_orient.enabled ? "true" : "false") << '\n';
    std::cout << "  autoOrient.applied: " << (report.auto_orient.applied ? "true" : "false") << '\n';
    std::cout << "  autoOrient.maxHeightMm: " << report.auto_orient.max_height_mm << '\n';
    std::cout << "  autoOrient.selectedOrientation: " << report.auto_orient.selected_orientation << '\n';
    print_bbox("originalBboxMm", report.auto_orient.original_bbox_mm);
    print_bbox("orientedBboxMm", report.bbox_mm);
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const CliOptions options = parse_options(argc, argv);
        if (options.show_help) {
            print_usage();
            return 0;
        }
        if (options.inspect_model) {
            return inspect_model(options.config_path);
        }

        slicer_core::SliceRunOptions run_options;
        run_options.write_tiff_layers = !options.preview_only;
        const slicer_core::SliceRunResult result = slicer_core::run_slicer(options.config_path, run_options);
        std::cout << "slicer_cli: generated package\n";
        if (options.preview_only) {
            std::cout << "  mode: preview-only, TIFF layers were not written\n";
        }
        std::cout << "  packageDir: " << result.package_dir.string() << '\n';
        std::cout << "  grid: " << result.width_px << " x " << result.height_px << " x " << result.layer_count
                  << '\n';
        std::cout << "  modelPixels: " << result.model_pixel_count << '\n';
        std::cout << "  supportPixels: " << result.support_pixel_count << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "slicer_cli error: " << error.what() << '\n';
        return 1;
    }
}
