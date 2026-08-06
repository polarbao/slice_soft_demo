#pragma once

#include "contracts/print_module_spi.h"
#include "slicer_module/CapabilityJsonAdapter.h"

#include <array>
#include <memory>
#include <string>
#include <string_view>

namespace slicesoft::module
{

/** @brief Frozen in-process capability IDs from DEV_14 section 5. */
inline constexpr std::array<std::string_view, 13> SyncCapabilities{
    "model.import",
    "model.get_metadata",
    "model.release",
    "scene.apply_operation",
    "scene.get_snapshot",
    "scene.get_viewdata",
    "geometry.preflight",
    "geometry.collision",
    "package.verify",
    "package.get_summary",
    "package.get_layer_descriptor",
    "package.render_layer_preview",
    "package.read_report"};

/** @brief Result of validating and executing one submitted request. */
struct CapabilitySubmission
{
    bool accepted{false};
    std::string capability;
    std::string errorcode;
    std::string errormessage;
    std::string errordetail;
    CapabilityOutput output;
};

/** @brief Process-local router and terminal-result store for synchronous light capabilities. */
class SyncCapabilityAdapter final
{
public:
    /** @brief Returns the process-wide adapter. @return Shared adapter instance. */
    [[nodiscard]] static SyncCapabilityAdapter& Instance();

    /** @brief Registers one live module. @param module Opaque module handle. @return True when ready. */
    [[nodiscard]] bool RegisterModule(pm_module_t* module);

    /** @brief Removes a module and all retained terminal outputs. @param module Opaque module handle. */
    void RemoveModule(pm_module_t* module) noexcept;

    /** @brief Validates carrier routing and executes one synchronous request. @param module Owner module. @param requestText UTF-8 request. @return Submission result. */
    [[nodiscard]] CapabilitySubmission Execute(
        pm_module_t* module,
        std::string_view requestText);

    /** @brief Stores an already-terminal job output. @param job Job handle. @param module Owner. @param submission Accepted output. @return True when retained. */
    [[nodiscard]] bool StoreJob(
        pm_job_t* job,
        pm_module_t* module,
        CapabilitySubmission submission);

    /** @brief Gets the immutable terminal progress snapshot. @param job Job handle. @return Progress JSON or empty. */
    [[nodiscard]] std::string Poll(pm_job_t* job) const;

    /** @brief Gets immutable terminal result bytes. @param job Job handle. @return Output or null. */
    [[nodiscard]] std::shared_ptr<const CapabilityOutput> Result(
        pm_job_t* job) const;

    /** @brief Removes retained output for one released job. @param job Job handle. */
    void ReleaseJob(pm_job_t* job) noexcept;

private:
    struct Implementation;

    SyncCapabilityAdapter();
    ~SyncCapabilityAdapter();

    SyncCapabilityAdapter(const SyncCapabilityAdapter&) = delete;
    SyncCapabilityAdapter& operator=(const SyncCapabilityAdapter&) = delete;

    std::unique_ptr<Implementation> m_implementation;
};

}  // namespace slicesoft::module
