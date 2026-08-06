#pragma once

#include "slicer_core/api/SliceFacade.h"

#include <filesystem>
#include <functional>
#include <string>

namespace slicer_core::engine
{

/** @brief Resolved immutable identities required before production slicing. */
struct SliceSubmissionContract
{
    std::string scenehash;
    std::filesystem::path packagedir;
};

/** @brief Resolves the committed scene and output identities from an effective config. */
using SliceSubmissionContractResolver = std::function<
    api::ApiResult<SliceSubmissionContract>(
        const std::filesystem::path&)>;

/** @brief Invokes the existing production scene slicer and returns its package summary. */
using SliceProductionRunner = std::function<api::ApiResult<api::SliceResult>(
    const std::filesystem::path&,
    const api::ProgressSink&)>;

/** @brief Invokes production slicing with an explicit synchronous cancel token. */
using CancellableSliceProductionRunner =
    std::function<api::ApiResult<api::SliceResult>(
        const api::SliceRequest&,
        const api::ICancelToken&,
        const api::ProgressSink&)>;

/**
 * @brief Engine-side SliceFacade adapter over the existing production entry.
 *
 * The adapter validates caller-owned identities, forwards monotonic progress,
 * and turns cooperative cancellation requests into the frozen PM error. It
 * does not own material, raster, TIFF, or publication policy.
 */
class SliceFacadeAdapter final : public api::SliceFacade
{
public:
    /**
     * @brief Creates an adapter with explicit production bindings.
     * @param contractResolver Effective-config identity resolver.
     * @param productionRunner Existing production slicing entry adapter.
     */
    SliceFacadeAdapter(
        SliceSubmissionContractResolver contractResolver,
        SliceProductionRunner productionRunner);

    /**
     * @brief Creates an adapter with a cancellation-aware production binding.
     * @param contractResolver Effective-config identity resolver.
     * @param productionRunner Production entry receiving the caller token.
     */
    SliceFacadeAdapter(
        SliceSubmissionContractResolver contractResolver,
        CancellableSliceProductionRunner productionRunner);

    /**
     * @brief Runs one committed production scene slice.
     * @param request Caller-owned job, scene, config, and package identities.
     * @param cancelToken Cooperative cancellation source.
     * @param progressSink Monotonic progress observer.
     * @return Package summary or a stable PM-SLICER error.
     */
    [[nodiscard]] api::ApiResult<api::SliceResult> Run(
        const api::SliceRequest& request,
        const api::ICancelToken& cancelToken,
        const api::ProgressSink& progressSink) noexcept override;

private:
    SliceSubmissionContractResolver m_contractResolver;
    CancellableSliceProductionRunner m_productionRunner;
};

}  // namespace slicer_core::engine
