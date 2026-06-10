#pragma once

#include "slicer_core/json_value.h"

#include <filesystem>

namespace slicer_core
{

/**
 * @brief Write a JSON report file.
 * @param path Destination path.
 * @param value JSON payload to write.
 */
void WriteReportJsonFile(const std::filesystem::path& path, const Json& value);

}  // namespace slicer_core
