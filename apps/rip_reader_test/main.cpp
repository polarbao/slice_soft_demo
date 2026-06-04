#include "slicer_core/rip_reader.h"

#include <exception>
#include <iostream>
#include <string>

namespace {

void print_usage() {
    std::cout << "Usage: rip_reader_test --package <SlicePackage-dir>\n";
}

std::string parse_package_path(const int argc, char** argv) {
    for (int i{1}; i < argc; ++i) {
        const std::string arg{argv[i]};
        if (arg == "--help" || arg == "-h") {
            print_usage();
            return {};
        }
        if (arg == "--package" && i + 1 < argc) {
            return argv[i + 1];
        }
    }
    return "output/SlicePackage";
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const std::string package_path = parse_package_path(argc, argv);
        if (package_path.empty()) {
            return 0;
        }

        const slicer_core::RipValidationResult result = slicer_core::validate_slice_package(package_path);
        std::cout << "rip_reader_test: package valid\n";
        std::cout << "  packageDir: " << result.package_dir.string() << '\n';
        std::cout << "  grid: " << result.width_px << " x " << result.height_px << " x " << result.layer_count
                  << '\n';
        std::cout << "  channelOrder: R G B W S V\n";
        for (const auto& layer : result.layer_checksums) {
            std::cout << "  layer " << layer.index << " checksum:";
            for (const std::uint64_t checksum : layer.channels) {
                std::cout << ' ' << checksum;
            }
            std::cout << '\n';
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "rip_reader_test error: " << error.what() << '\n';
        return 1;
    }
}
