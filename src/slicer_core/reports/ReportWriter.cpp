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
}

}  // namespace slicer_core
