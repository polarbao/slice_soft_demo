#pragma once

#include "slicer_core/geometry/GeometryKernelTypes.h"

namespace slicer_core
{

/**
 * @brief Return OpenVDB compile/runtime status for the experimental adapter.
 * @return OpenVDB status.
 */
OpenVdbStatus GetOpenVdbStatus();

/**
 * @brief Run a minimal OpenVDB smoke case.
 * @return Smoke result; returns graceful skip when OpenVDB is disabled.
 */
OpenVdbSmokeResult RunOpenVdbSmokeCase();

}  // namespace slicer_core
