#pragma once

#include "slicer_core/json_value.h"

#include <string>

namespace slicer_core
{

/**
 * @brief Create a report object with common R2 base fields.
 * @param schema Report schema name.
 * @param source Report source object.
 * @param configSnapshot Config snapshot object.
 * @return Report JSON with base fields.
 */
Json MakeReportBase(const std::string& schema, const Json& source, const Json& configSnapshot);

/**
 * @brief Append a warning message into a report object.
 * @param report Report JSON object.
 * @param warning Warning message.
 * @return Updated report JSON.
 */
Json AppendReportWarning(const Json& report, const std::string& warning);

/**
 * @brief Append an error message into a report object.
 * @param report Report JSON object.
 * @param error Error message.
 * @return Updated report JSON.
 */
Json AppendReportError(const Json& report, const std::string& error);

/**
 * @brief Append a timing entry into a report object.
 * @param report Report JSON object.
 * @param name Timing entry name.
 * @param milliseconds Timing value in milliseconds.
 * @return Updated report JSON.
 */
Json AppendReportTiming(const Json& report, const std::string& name, double milliseconds);

}  // namespace slicer_core
