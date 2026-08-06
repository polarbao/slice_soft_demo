#pragma once

#include "slicer_core/json_value.h"

#include <chrono>
#include <string>
#include <string_view>

namespace slicesoft::module
{

/** @brief Execution carrier selected for one public capability request. */
enum class CapabilityCarrier
{
    InProcess,
    Worker
};

/** @brief Validated carrier decision and normalized Worker request payload. */
struct CapabilityRoute
{
    bool accepted{false};
    CapabilityCarrier carrier{CapabilityCarrier::InProcess};
    std::string publicCapability;
    std::string workerCapability;
    std::string jobId;
    std::string correlationId;
    std::chrono::milliseconds timeout{3600000};
    slicer_core::Json workerPayload;
    std::string errorCode;
    std::string errorMessage;
    std::string errorDetail;
};

/**
 * @brief Classifies public capability requests without executing algorithms.
 *
 * The router freezes the Stage 14 carrier boundary: light capabilities stay
 * in-process while full preflight, repair, and RGBWSV slicing use the Worker.
 */
class CapabilityCarrierRouter final
{
public:
    /**
     * @brief Parses and classifies one UTF-8 public capability request.
     * @param requestText Public SPI request JSON.
     * @return Accepted carrier decision or a stable fail-closed diagnostic.
     */
    [[nodiscard]] static CapabilityRoute Route(std::string_view requestText);
};

}  // namespace slicesoft::module
