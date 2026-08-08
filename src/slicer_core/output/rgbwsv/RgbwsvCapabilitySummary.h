#pragma once

#include "slicer_core/output/rgbwsv/RgbwsvPackageWriter.h"

namespace slicer_core
{

/**
 * @brief Validate paired capability-v1.2 package-summary evidence.
 * @param request Production package write request.
 * @throws std::invalid_argument When evidence is partial or malformed.
 */
void ValidateRgbwsvCapabilitySummary(
    const RgbwsvProductionPackageWriteRequest& request);

/**
 * @brief Append validated capability summary fields to a manifest object.
 * @param manifestObject Mutable production manifest object.
 * @param request Production package write request.
 */
void AppendRgbwsvCapabilitySummary(
    Json::Object& manifestObject,
    const RgbwsvProductionPackageWriteRequest& request);

}  // namespace slicer_core
