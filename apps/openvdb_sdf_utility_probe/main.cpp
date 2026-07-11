#include "slicer_core/geometry/MeshTopologyDiagnostics.h"
#include "slicer_core/geometry/OpenVdbGeometryKernelService.h"
#include "slicer_core/geometry/OpenVdbSdfUtilityReport.h"
#include "slicer_core/geometry/TriangleMeshData.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{

struct ProbeOptions
{
    std::filesystem::path output_path{"output/benchmarks/12b_r2_openvdb_sdf_utility_report.json"};
    std::string build_dir;
    std::string build_type{"Debug"};
    double voxel_size_mm{0.05};
    double shell_thickness_mm{0.10};
};

std::string RequireValue(const int argc, char** argv, int& index, const std::string& name)
{
    if (index + 1 >= argc)
    {
        throw std::runtime_error(name + " requires a value");
    }
    ++index;
    return argv[index];
}

ProbeOptions ParseOptions(const int argc, char** argv)
{
    ProbeOptions options;
    for (int index{1}; index < argc; ++index)
    {
        const std::string argument{argv[index]};
        if (argument == "--output")
        {
            options.output_path = RequireValue(argc, argv, index, argument);
        }
        else if (argument == "--build-dir")
        {
            options.build_dir = RequireValue(argc, argv, index, argument);
        }
        else if (argument == "--build-type")
        {
            options.build_type = RequireValue(argc, argv, index, argument);
        }
        else if (argument == "--voxel-size-mm")
        {
            options.voxel_size_mm = std::stod(RequireValue(argc, argv, index, argument));
        }
        else if (argument == "--shell-thickness-mm")
        {
            options.shell_thickness_mm = std::stod(RequireValue(argc, argv, index, argument));
        }
        else if (argument == "--help" || argument == "-h")
        {
            std::cout
                << "openvdb_sdf_utility_probe --output <report.json> --build-dir <dir>"
                << " --build-type <type> --voxel-size-mm <mm> --shell-thickness-mm <mm>\n";
            std::exit(0);
        }
        else
        {
            throw std::runtime_error("unknown argument: " + argument);
        }
    }
    if (options.voxel_size_mm <= 0.0 || options.shell_thickness_mm <= 0.0)
    {
        throw std::runtime_error("voxel size and shell thickness must be positive");
    }
    return options;
}

std::string BuildCommandLine(const int argc, char** argv)
{
    std::string commandLine;
    for (int index{0}; index < argc; ++index)
    {
        if (!commandLine.empty())
        {
            commandLine += ' ';
        }
        commandLine += argv[index];
    }
    return commandLine;
}

int RunProbe(const ProbeOptions& options, const std::string& commandLine)
{
    slicer_core::TriangleMeshData mesh = slicer_core::MakeGeneratedBoxMesh(1.0, 1.0, 0.4);
    const slicer_core::MeshTopologyReport topology = slicer_core::AnalyzeMeshTopology(mesh);

    slicer_core::GeometryKernelRequest request;
    request.mesh = &mesh;
    request.level_set_options.voxel_size_mm = options.voxel_size_mm;
    request.shell_options.shell_thickness_mm = options.shell_thickness_mm;

    slicer_core::OpenVdbGeometryKernelService service;
    const slicer_core::GeometryKernelResult geometryResult = service.BuildSurfaceShell(request);

    slicer_core::OpenVdbSdfUtilityReportInput reportInput;
    reportInput.build_type = options.build_type;
    reportInput.build_dir = options.build_dir;
    reportInput.command_line = commandLine;
    reportInput.model_path = "generated://closed-box";
    reportInput.input_format = "generated";
    reportInput.admission_mode = "strict_closed";
    reportInput.requested_shell_thickness_mm = options.shell_thickness_mm;
    reportInput.geometry_result = &geometryResult;
    reportInput.topology_report = &topology;

    const slicer_core::Json report = slicer_core::BuildOpenVdbSdfUtilityReport(reportInput);
    if (!options.output_path.parent_path().empty())
    {
        std::filesystem::create_directories(options.output_path.parent_path());
    }
    std::ofstream output(options.output_path, std::ios::binary);
    if (!output)
    {
        throw std::runtime_error("failed to open report output: " + options.output_path.string());
    }
    output << report.dump(2);

    std::cout << "openvdb_sdf_utility_probe: generated diagnostic report\n"
              << "  report: " << options.output_path.generic_string() << '\n'
              << "  openVdbAvailable: "
              << (report.at("build").at("openVdbAvailable").as_bool() ? "true" : "false") << '\n'
              << "  writesProductionTiff: false\n";
    return 0;
}

}  // namespace

int main(const int argc, char** argv)
{
    try
    {
        return RunProbe(ParseOptions(argc, argv), BuildCommandLine(argc, argv));
    }
    catch (const std::exception& error)
    {
        std::cerr << "openvdb_sdf_utility_probe: " << error.what() << '\n';
        return 1;
    }
}
