#pragma once

#include "slicer_core/json_value.h"

#include <string>

namespace slicer_core
{

/**
 * @brief Validate that a report object has a specific schema value.
 * @param report Report JSON object.
 * @param expectedSchema Expected schema value.
 * @param error Output validation error text.
 * @return True when the schema matches.
 */
bool ValidateReportSchema(const Json& report, const std::string& expectedSchema, std::string& error);

/**
 * @brief Validate R2 base report fields.
 * @param report Report JSON object.
 * @param error Output validation error text.
 * @return True when the report contains base fields.
 */
bool ValidateReportBaseFields(const Json& report, std::string& error);

}  // namespace slicer_core
