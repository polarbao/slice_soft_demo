#pragma once

#include "slicer_core/materials/texture_application/TextureFillPartitionTypes.h"

namespace slicer_core
{

/**
 * @brief Backend boundary for diagnostic global texture/fill partition candidates.
 */
class IGlobalTextureFillPartitionBackend
{
public:
    /** @brief Destroy the backend interface. */
    virtual ~IGlobalTextureFillPartitionBackend() = default;

    /**
     * @brief Evaluate a global partition request without writing production output.
     * @param request Backend-neutral partition options.
     * @return Candidate masks and backend diagnostics.
     */
    virtual GlobalTextureFillPartitionCandidate Evaluate(
        const GlobalTextureFillPartitionRequest& request) const = 0;
};

/**
 * @brief Validate backend candidate masks against the Stage 12E partition invariants.
 */
class GlobalTextureFillPartitionService
{
public:
    /**
     * @brief Construct a diagnostic partition service.
     * @param backend Non-owning backend pointer; null represents an unavailable backend.
     */
    explicit GlobalTextureFillPartitionService(
        const IGlobalTextureFillPartitionBackend* backend = nullptr);

    /**
     * @brief Evaluate and validate a global texture/fill partition candidate.
     * @param request Backend-neutral partition request.
     * @return Deterministic diagnostic result with recomputed statistics.
     */
    GlobalTextureFillPartitionResult Evaluate(
        const GlobalTextureFillPartitionRequest& request) const;

private:
    const IGlobalTextureFillPartitionBackend* m_backend;
};

/**
 * @brief Compare validated CPU and OpenVDB partition results without production admission.
 * @param cpuResult Validated legacy CPU candidate result.
 * @param openVdbResult Validated OpenVDB conformance candidate result.
 * @return Structural and quantitative conformance evidence.
 */
TextureFillPartitionConformanceResult CompareTextureFillPartitionResults(
    const GlobalTextureFillPartitionResult& cpuResult,
    const GlobalTextureFillPartitionResult& openVdbResult);

}  // namespace slicer_core
