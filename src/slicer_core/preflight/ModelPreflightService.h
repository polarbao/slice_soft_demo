#pragma once

#include "slicer_core/geometry/repair/MeshRepairTypes.h"
#include "slicer_core/preflight/ModelPreflightTypes.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <mutex>
#include <string>

namespace slicer_core
{

/**
 * @brief Deterministic limits used by the two-stage model preflight service.
 */
struct ModelPreflightOptions
{
    double voxelMm{0.10};
    std::size_t maxSelfIntersectionPairs{128U};
    std::size_t maxTrianglePairChecks{250000U};
    std::uint64_t maxCompleteSelfIntersectionCandidatePairs{5000000U};
    double positionEpsilonMm{1.0e-6};
    double degenerateAreaEpsilonMm2{1.0e-12};
};

/**
 * @brief One synchronous model preflight request.
 */
struct ModelPreflightRequest
{
    std::filesystem::path configPath;
    ModelPreflightOptions options;
    std::uint64_t generation{0U};
    std::function<bool()> cancellationRequested;
};

/**
 * @brief Full topology evidence retained with one completed preflight run.
 *
 * This execution-only evidence is not part of the stable model-preflight JSON
 * schema. Downstream diagnostic tools may consume it without re-running the
 * complete self-intersection audit.
 */
struct ModelPreflightFullAuditEvidence
{
    bool available{false};
    MeshRepairDiagnosticsSummary diagnostics;
    MeshCompleteSelfIntersectionAnalysis self_intersection;
};

/**
 * @brief Execution metadata and immutable preflight result.
 */
struct ModelPreflightExecutionResult
{
    std::uint64_t generation{0U};
    bool fastComplete{false};
    bool fullComplete{false};
    bool cacheHit{false};
    bool cancelled{false};
    bool stale{false};
    ModelPreflightFullAuditEvidence full_audit;
    ModelPreflightResult result;
};

/**
 * @brief Run fast import validation and full transformed geometry diagnostics.
 *
 * The service owns only an in-process immutable-result cache. It does not create
 * threads, start a slice pipeline, repair geometry, or write production output.
 */
class ModelPreflightService
{
public:
    /**
     * @brief Execute preflight or reuse a fresh complete cached result.
     * @param request Config path, diagnostic options, generation and cancellation callback.
     * @return Deterministic execution metadata and preflight result.
     */
    ModelPreflightExecutionResult Run(const ModelPreflightRequest& request);

    /**
     * @brief Remove all in-process reusable results.
     */
    void ClearCache();

    /**
     * @brief Return the current number of reusable complete results.
     * @return Cached result count.
     */
    std::size_t CacheSize() const;

private:
    struct CachedPreflightResult
    {
        ModelPreflightResult result;
        ModelPreflightFullAuditEvidence full_audit;
    };

    mutable std::mutex m_cacheMutex;
    std::map<std::string, CachedPreflightResult> m_cache;
};

}  // namespace slicer_core
