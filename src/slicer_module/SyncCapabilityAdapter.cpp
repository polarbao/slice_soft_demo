#include "slicer_module/SyncCapabilityAdapter.h"

#include "slicer_module/ModelCapabilityAdapter.h"
#include "slicer_module/PackageCapabilityAdapter.h"
#include "slicer_module/SceneCapabilityAdapter.h"

#include <algorithm>
#include <exception>
#include <iterator>
#include <map>
#include <mutex>
#include <utility>

namespace slicesoft::module
{

struct SyncCapabilityAdapter::Implementation
{
    struct ModuleContext
    {
        std::mutex operationmutex;
        ModelCapabilityAdapter models;
        PackageCapabilityAdapter packages;
        SceneCapabilityAdapter scenes{models};
    };

    struct StoredJob
    {
        pm_module_t* owner{nullptr};
        CapabilityOutput output;
        std::string progress;
    };

    mutable std::mutex mutex;
    std::map<pm_module_t*, std::shared_ptr<ModuleContext>> modules;
    std::map<pm_job_t*, StoredJob> jobs;
};

SyncCapabilityAdapter& SyncCapabilityAdapter::Instance()
{
    static SyncCapabilityAdapter adapter;
    return adapter;
}

SyncCapabilityAdapter::SyncCapabilityAdapter()
    : m_implementation{std::make_unique<Implementation>()}
{
}

SyncCapabilityAdapter::~SyncCapabilityAdapter() = default;

bool SyncCapabilityAdapter::RegisterModule(pm_module_t* const module)
{
    if (module == nullptr)
    {
        return false;
    }

    auto context = std::make_shared<Implementation::ModuleContext>();
    std::scoped_lock lock{m_implementation->mutex};
    return m_implementation->modules.emplace(module, std::move(context)).second;
}

void SyncCapabilityAdapter::RemoveModule(pm_module_t* const module) noexcept
{
    try
    {
        std::scoped_lock lock{m_implementation->mutex};
        m_implementation->modules.erase(module);
        for (auto entry = m_implementation->jobs.begin();
             entry != m_implementation->jobs.end();)
        {
            entry = entry->second.owner == module
                ? m_implementation->jobs.erase(entry)
                : std::next(entry);
        }
    }
    catch (...)
    {
    }
}

CapabilitySubmission SyncCapabilityAdapter::Execute(
    pm_module_t* const module,
    const std::string_view requestText)
{
    const auto reject = [](
                            std::string capability,
                            std::string code,
                            std::string message,
                            std::string detail)
    {
        CapabilitySubmission result;
        result.capability = std::move(capability);
        result.errorcode = std::move(code);
        result.errormessage = std::move(message);
        result.errordetail = std::move(detail);
        return result;
    };

    try
    {
        const slicer_core::Json request = ParseCapabilityRequest(requestText);
        const std::string capability = RequireString(request, "capability");
        if (capability == "geometry.preflight")
        {
            const std::string mode = RequireString(request, "mode");
            if (mode != "fast")
            {
                return reject(
                    capability,
                    mode == "full"
                        ? "PM-SLICER-PROFILE-0031"
                        : "PM-SLICER-INPUT-0002",
                    mode == "full"
                        ? "full preflight requires the Worker carrier"
                        : "geometry.preflight mode is invalid",
                    "geometry.preflight uses the same capability ID and only "
                    "mode=fast is synchronous");
            }
        }

        if (std::find(
                SyncCapabilities.begin(),
                SyncCapabilities.end(),
                capability) == SyncCapabilities.end())
        {
            const bool workerCapability = capability == "geometry.repair"
                || capability == "slice.rgbwsv";
            return reject(
                capability,
                workerCapability
                    ? "PM-SLICER-PROFILE-0031"
                    : "PM-SLICER-INPUT-0002",
                workerCapability
                    ? "capability requires the Worker carrier"
                    : "unknown capability",
                capability);
        }

        std::shared_ptr<Implementation::ModuleContext> context;
        {
            std::scoped_lock lock{m_implementation->mutex};
            const auto entry = m_implementation->modules.find(module);
            if (entry != m_implementation->modules.end())
            {
                context = entry->second;
            }
        }
        if (!context)
        {
            return reject(
                capability,
                "PM-SLICER-INPUT-0002",
                "module context is not registered",
                "pm_submit module");
        }

        CapabilityOutput output;
        std::scoped_lock operationLock{context->operationmutex};
        if (capability.starts_with("model.")
            || capability == "geometry.preflight")
        {
            output = MakeCapabilityOutput(
                context->models.Execute(capability, request));
        }
        else if (capability.starts_with("package."))
        {
            output = MakeCapabilityOutput(
                context->packages.Execute(capability, request));
        }
        else
        {
            output = context->scenes.Execute(capability, request);
        }

        CapabilitySubmission submission;
        submission.accepted = true;
        submission.capability = capability;
        submission.output = std::move(output);
        return submission;
    }
    catch (const CapabilityRequestError& error)
    {
        return reject(
            {},
            "PM-SLICER-INPUT-0002",
            "invalid synchronous capability request",
            std::string{error.what()}
                + " (Stage 14C-04/14D routing boundary)");
    }
    catch (const std::exception& error)
    {
        return reject(
            {},
            "PM-SLICER-INTERNAL-0099",
            "synchronous capability dispatch failed",
            error.what());
    }
    catch (...)
    {
        return reject(
            {},
            "PM-SLICER-INTERNAL-0099",
            "synchronous capability dispatch failed",
            "unknown exception");
    }
}

bool SyncCapabilityAdapter::StoreJob(
    pm_job_t* const job,
    pm_module_t* const module,
    CapabilitySubmission submission)
{
    if (job == nullptr || module == nullptr || !submission.accepted)
    {
        return false;
    }

    Implementation::StoredJob stored;
    stored.owner = module;
    stored.output = std::move(submission.output);
    const std::string state = stored.output.succeeded ? "succeeded" : "failed";
    stored.progress = slicer_core::Json::object({
        {"state", state},
        {"phase", "completed"},
        {"current", 1},
        {"total", 1},
        {"percent", 100},
        {"capability", submission.capability}}).dump(0);
    std::scoped_lock lock{m_implementation->mutex};
    return m_implementation->jobs.emplace(job, std::move(stored)).second;
}

std::string SyncCapabilityAdapter::Poll(pm_job_t* const job) const
{
    std::scoped_lock lock{m_implementation->mutex};
    const auto entry = m_implementation->jobs.find(job);
    return entry == m_implementation->jobs.end()
        ? std::string{}
        : entry->second.progress;
}

std::shared_ptr<const CapabilityOutput> SyncCapabilityAdapter::Result(
    pm_job_t* const job) const
{
    std::scoped_lock lock{m_implementation->mutex};
    const auto entry = m_implementation->jobs.find(job);
    return entry == m_implementation->jobs.end()
        ? nullptr
        : std::make_shared<const CapabilityOutput>(entry->second.output);
}

void SyncCapabilityAdapter::ReleaseJob(pm_job_t* const job) noexcept
{
    try
    {
        std::scoped_lock lock{m_implementation->mutex};
        m_implementation->jobs.erase(job);
    }
    catch (...)
    {
    }
}

}  // namespace slicesoft::module
