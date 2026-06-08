#include "slicer_core/rip_reader.h"

#include <array>
#include <cstddef>
#include <exception>
#include <iostream>
#include <string>

namespace {

enum class OutputMode {
    Verbose,
    Summary,
    Quiet
};

struct CliOptions {
    std::string package_path{"output/SlicePackage"};
    bool expect_error{false};
    std::string expected_message;
    std::string expected_code;
    OutputMode output_mode{OutputMode::Verbose};
    bool show_help{false};
};

void print_usage() {
    std::cout
        << "Usage: rip_reader_test --package <SlicePackage-dir> [--summary|--quiet] [--expect-error] [--expect-message <text>] [--expect-code <code>]\n";
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
        } else if (arg == "--summary") {
            options.output_mode = OutputMode::Summary;
        } else if (arg == "--quiet") {
            options.output_mode = OutputMode::Quiet;
        } else if (arg == "--expect-error") {
            options.expect_error = true;
        } else if (arg == "--expect-message" && i + 1 < argc) {
            options.expected_message = argv[++i];
        } else if (arg == "--expect-code" && i + 1 < argc) {
            options.expected_code = argv[++i];
        }
    }
    return options;
}

void print_verbose_result(const slicer_core::RipValidationResult& result) {
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
}

void print_summary_result(const slicer_core::RipValidationResult& result) {
    constexpr std::array<const char*, 6> channels{"R", "G", "B", "W", "S", "V"};
    std::cout << "rip_reader_test: PASS\n";
    std::cout << "  packageDir: " << result.package_dir.string() << '\n';
    std::cout << "  schema: " << result.schema << '\n';
    std::cout << "  storageMode: " << result.storage_mode << '\n';
    std::cout << "  grid: " << result.width_px << " x " << result.height_px << " x " << result.layer_count
              << '\n';
    std::cout << "  bitDepth: " << result.bit_depth << '\n';
    std::cout << "  channelOrder: R G B W S V\n";
    std::cout << "  channelPrintPixels:";
    for (std::size_t i{0}; i < channels.size(); ++i) {
        std::cout << ' ' << channels.at(i) << '=' << result.total_channel_stats.at(i).print_pixels;
    }
    std::cout << '\n';
    std::cout << "  warnings: " << result.warnings_count << '\n';
}

void print_success(const CliOptions& options, const slicer_core::RipValidationResult& result) {
    switch (options.output_mode) {
        case OutputMode::Quiet:
            std::cout << "PASS " << result.package_dir.string() << '\n';
            return;
        case OutputMode::Summary:
            print_summary_result(result);
            return;
        case OutputMode::Verbose:
            print_verbose_result(result);
            return;
    }
}

void print_expected_error_success(const CliOptions& options, const std::string& code, const std::string& message) {
    if (options.output_mode == OutputMode::Quiet) {
        std::cout << "PASS expected-error " << code << '\n';
        return;
    }
    std::cout << "rip_reader_test: expected error\n";
    if (!code.empty()) {
        std::cout << "  code: " << code << '\n';
    }
    std::cout << "  message: " << message << '\n';
}

void print_failure(const CliOptions& options, const std::string& message) {
    if (options.output_mode == OutputMode::Quiet) {
        std::cerr << "FAIL " << message << '\n';
        return;
    }
    std::cerr << "rip_reader_test error: " << message << '\n';
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
            print_failure(options, "expected validation error, but package was valid");
            return 1;
        }
        print_success(options, result);
        return 0;
    } catch (const slicer_core::ValidationError& error) {
        if (options.expect_error) {
            const std::string message{error.what()};
            const std::string code = slicer_core::validation_error_code_string(error.code());
            if (!options.expected_code.empty() && options.expected_code != code) {
                print_failure(
                    options,
                    "expected code '" + options.expected_code + "', got: " + code + " message: " + message);
                return 1;
            }
            if (!options.expected_message.empty() && message.find(options.expected_message) == std::string::npos) {
                print_failure(
                    options,
                    "expected message containing '" + options.expected_message + "', got: " + message);
                return 1;
            }
            print_expected_error_success(options, code, message);
            return 0;
        }
        print_failure(options, error.what());
        return 1;
    } catch (const std::exception& error) {
        if (options.expect_error) {
            const std::string message{error.what()};
            if (!options.expected_message.empty() && message.find(options.expected_message) == std::string::npos) {
                print_failure(
                    options,
                    "expected message containing '" + options.expected_message + "', got: " + message);
                return 1;
            }
            print_expected_error_success(options, "", message);
            return 0;
        }
        print_failure(options, error.what());
        return 1;
    }
}
