#include "slicer_core/reports/ReportWriter.h"

#include <fstream>
#include <stdexcept>

namespace slicer_core
{

void WriteReportJsonFile(const std::filesystem::path& path, const Json& value)
{
    std::ofstream output(path);
    if (!output)
    {
        throw std::runtime_error("failed to write report: " + path.string());
    }
    output << value.dump(2) << '\n';
    output.flush();
    if (!output)
    {
        throw std::runtime_error(
            "failed to flush report: " + path.string());
    }
    output.close();
    if (!output)
    {
        throw std::runtime_error(
            "failed to close report: " + path.string());
    }
}

}  // namespace slicer_core
