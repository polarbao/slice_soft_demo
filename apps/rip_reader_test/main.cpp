#include "slicer_core/rip_reader.h"

#include <exception>
#include <iostream>
#include <string>

namespace {

struct CliOptions {
    std::string package_path{"output/SlicePackage"};
    bool expect_error{false};
    std::string expected_message;
    bool show_help{false};
};

void print_usage() {
    std::cout << "Usage: rip_reader_test --package <SlicePackage-dir> [--expect-error] [--expect-message <text>]\n";
}

CliOptions parse_options(const int argc, char** argv) {
    CliOptions options;
    for (int i{1}; i < argc; ++i) {
        const std::string arg{argv[i]};
        if (arg == "--help" || arg == "-h") {
            options.show_help = true;
            return options;
        }
        if (arg == "--package" && i + 1 < argc) {
            options.package_path = argv[++i];
        } else if (arg == "--expect-error") {
            options.expect_error = true;
        } else if (arg == "--expect-message" && i + 1 < argc) {
            options.expected_message = argv[++i];
        }
    }
    return options;
}

}  // namespace

int main(int argc, char** argv) {
    const CliOptions options = parse_options(argc, argv);
    if (options.show_help) {
        print_usage();
        return 0;
    }

    try {
        const slicer_core::RipValidationResult result = slicer_core::validate_slice_package(options.package_path);
        if (options.expect_error) {
            std::cerr << "rip_reader_test error: expected validation error, but package was valid\n";
            return 1;
        }
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
        if (options.expect_error) {
            const std::string message{error.what()};
            if (!options.expected_message.empty() && message.find(options.expected_message) == std::string::npos) {
                std::cerr << "rip_reader_test error: expected message containing '" << options.expected_message
                          << "', got: " << message << '\n';
                return 1;
            }
            std::cout << "rip_reader_test: expected error\n";
            std::cout << "  message: " << message << '\n';
            return 0;
        }
        std::cerr << "rip_reader_test error: " << error.what() << '\n';
        return 1;
    }
}
