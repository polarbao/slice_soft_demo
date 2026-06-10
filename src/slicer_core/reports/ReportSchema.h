#pragma once

#include <string>

namespace slicer_core
{

/**
 * @brief Reference to a report schema and default file name.
 */
struct ReportSchemaRef
{
    std::string schema;
    std::string file_name;
};

/**
 * @brief Return the current preview report schema reference.
 * @return Preview report schema descriptor.
 */
ReportSchemaRef PreviewReportSchemaRef();

}  // namespace slicer_core
