#include "slicer_core/slicer.h"

#include <exception>
#include <iostream>
#include <string>

namespace {

void print_usage() {
    std::cout << "Usage: slicer_cli --config <path-to-slice_config.json>\n";
}

std::string parse_config_path(const int argc, char** argv) {
    for (int i{1}; i < argc; ++i) {
        const std::string arg{argv[i]};
        if (arg == "--help" || arg == "-h") {
            print_usage();
            return {};
        }
        if (arg == "--config" && i + 1 < argc) {
            return argv[i + 1];
        }
    }
    return "samples/configs/slice_config.json";
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const std::string config_path = parse_config_path(argc, argv);
        if (config_path.empty()) {
            return 0;
        }

        const slicer_core::SliceRunResult result = slicer_core::run_slicer(config_path);
        std::cout << "slicer_cli: generated package\n";
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
