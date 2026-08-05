#include "slicer_core/model.h"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

int RunProbe(const std::filesystem::path& modelPath)
{
    slicer_core::ModelLoadConfig config;
    config.input.model_path = modelPath;
    config.input.format = "obj";
    config.auto_orient.enabled = false;

    const slicer_core::ModelReport report =
        slicer_core::load_model_report(config, modelPath.parent_path());
    if (report.format != "obj")
    {
        throw std::runtime_error("model import probe returned an unexpected format");
    }
    if (report.vertex_count == 0U || report.triangle_count == 0U)
    {
        throw std::runtime_error("model import probe returned empty geometry");
    }

    std::cout << "Stage 14B model import layering probe: PASS\n"
              << "vertices=" << report.vertex_count
              << " triangles=" << report.triangle_count << '\n';
    return 0;
}

}  // namespace

int main(int argc, char** argv)
{
    try
    {
        if (argc != 2)
        {
            throw std::runtime_error(
                "usage: model_import_layering_probe <fixture.obj>");
        }
        return RunProbe(std::filesystem::path(argv[1]));
    }
    catch (const std::exception& error)
    {
        std::cerr << "Stage 14B model import layering probe: FAIL: "
                  << error.what() << '\n';
        return 1;
    }
}
