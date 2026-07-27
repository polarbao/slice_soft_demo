#pragma once

#include "slicer_core/preflight/ModelPreflightAdmissionPolicy.h"
#include "slicer_core/preflight/ModelPreflightService.h"
#include "slicer_core/scene/ModelInstance.h"
#include "slicer_core/scene/SceneModel.h"

#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <string>

namespace slicer_core
{

/**
 * @brief Immutable source and instance inputs for transformed preflight.
 */
struct TransformedModelPreflightRequest
{
    const SceneModel* source{nullptr};
    ModelInstance instance;
    ModelPreflightOptions options;
    ModelPreflightAdmissionContext admissioncontext;
    std::string sourcehash;
    std::string resourcehash;
    std::string sceneid;
    std::uint64_t scenerevision{0U};
    std::uint64_t expectedscenerevision{0U};
    std::uint64_t expectedtransformrevision{0U};
    std::uint64_t generation{0U};
    std::function<bool()> cancellationrequested;
};

/**
 * @brief Source and effective-instance diagnostics bound to one revision.
 */
struct TransformedModelPreflightExecution
{
    std::uint64_t generation{0U};
    std::string sceneid;
    std::string instanceid;
    std::uint64_t scenerevision{0U};
    std::uint64_t transformrevision{0U};
    std::string transformhash;
    bool cancelled{false};
    bool stale{false};
    ModelPreflightExecutionResult source;
    ModelPreflightExecutionResult transformed;

    /**
     * @brief Report whether both audits completed for the requested revision.
     * @return True for completed passed, warning, or blocked diagnostics.
     */
    bool IsValid() const;
};

/**
 * @brief Run cached source and transformed geometry preflight without Qt.
 */
class TransformedModelPreflightService final
{
public:
    /**
     * @brief Audit immutable source and effective instance geometry.
     * @param request Source, transform, revisions, options, and cancellation.
     * @return Revision-bound source and transformed admission evidence.
     */
    TransformedModelPreflightExecution Run(
        const TransformedModelPreflightRequest& request);

    /**
     * @brief Remove reusable completed geometry diagnostics.
     */
    void ClearCache();

private:
    ModelPreflightExecutionResult RunGeometry(
        const TransformedModelPreflightRequest& request,
        const ModelInstance& instance);

    mutable std::mutex m_cacheMutex;
    std::map<std::string, ModelPreflightExecutionResult> m_cache;
};

}  // namespace slicer_core
