#include "contracts/print_module_spi.h"
#include "slicer_module/BufferApi.h"
#include "slicer_module/ErrorApi.h"
#include "slicer_module/HandleRegistry.h"
#include "slicer_module/ModuleInitialization.h"
#include "slicer_module/ModuleInfo.h"
#include "slicer_module/ModuleSelfTest.h"
#include "slicer_module/SyncCapabilityAdapter.h"

#include <exception>
#include <string_view>

namespace
{

constexpr std::string_view InternalErrorCode{"PM-SLICER-INTERNAL-0099"};
constexpr std::string_view InputErrorCode{"PM-SLICER-INPUT-0002"};

void SetInternalError(const std::string_view detail) noexcept
{
    slicesoft::module::SetThreadLastError(
        InternalErrorCode,
        "slicer module internal failure",
        detail);
}

void SetInvalidRequestError(const std::string_view detail) noexcept
{
    slicesoft::module::SetThreadLastError(
        InputErrorCode,
        "invalid slicer module request",
        detail);
}

}  // namespace

extern "C" PM_API int PM_CALL pm_spi_version(void)
{
    try
    {
        return PM_SPI_VERSION;
    }
    catch (...)
    {
        return PM_ERR_FAILED;
    }
}

extern "C" PM_API int PM_CALL pm_module_info(char* jsonOut, int cap, int* outRequired)
{
    try
    {
        return slicesoft::module::WriteOut(
            slicesoft::module::GetModuleInfoJson(),
            jsonOut,
            cap,
            outRequired);
    }
    catch (...)
    {
        return PM_ERR_FAILED;
    }
}

extern "C" PM_API pm_module_t* PM_CALL pm_create(const char* optionsJson)
{
    try
    {
        (void)optionsJson;
        if (!slicesoft::module::EnsureProcessModuleInitialized())
        {
            SetInternalError("pm_create process initialization failed");
            return nullptr;
        }
        auto& registry = slicesoft::module::HandleRegistry::Instance();
        pm_module_t* const module = registry.CreateModule();
        try
        {
            if (!slicesoft::module::SyncCapabilityAdapter::Instance()
                    .RegisterModule(module))
            {
                (void)registry.DestroyModule(module);
                SetInternalError("pm_create could not register capability services");
                return nullptr;
            }
            return module;
        }
        catch (...)
        {
            (void)registry.DestroyModule(module);
            throw;
        }
    }
    catch (...)
    {
        SetInternalError("pm_create could not allocate a module handle");
        return nullptr;
    }
}

extern "C" PM_API void PM_CALL pm_destroy(pm_module_t* module)
{
    try
    {
        if (!slicesoft::module::HandleRegistry::Instance().DestroyModule(module))
        {
            SetInvalidRequestError("pm_destroy received an unknown module handle");
            return;
        }
        slicesoft::module::SyncCapabilityAdapter::Instance().RemoveModule(module);
    }
    catch (...)
    {
        SetInternalError("pm_destroy failed while retiring the module handle");
    }
}

extern "C" PM_API pm_job_t* PM_CALL pm_submit(pm_module_t* module, const char* requestJson)
{
    try
    {
        if (requestJson == nullptr)
        {
            SetInvalidRequestError("pm_submit requires request_json");
            return nullptr;
        }
        if (slicesoft::module::HandleRegistry::Instance().FindModule(module) == nullptr)
        {
            SetInvalidRequestError("pm_submit received an unknown module handle");
            return nullptr;
        }
        auto submission = slicesoft::module::SyncCapabilityAdapter::Instance()
            .Execute(module, requestJson);
        if (!submission.accepted)
        {
            slicesoft::module::SetThreadLastError(
                submission.errorcode,
                submission.errormessage,
                submission.errordetail);
            return nullptr;
        }

        auto& registry = slicesoft::module::HandleRegistry::Instance();
        pm_job_t* const job = registry.CreateJob(module);
        if (job == nullptr)
        {
            SetInternalError("pm_submit could not allocate a terminal job handle");
            return nullptr;
        }
        try
        {
            const bool succeeded = submission.output.succeeded;
            if (!slicesoft::module::SyncCapabilityAdapter::Instance().StoreJob(
                    job,
                    module,
                    std::move(submission)))
            {
                (void)registry.ReleaseJob(job);
                SetInternalError("pm_submit could not retain the terminal result");
                return nullptr;
            }
            const auto state = succeeded
                ? slicesoft::module::JobLifecycleState::Succeeded
                : slicesoft::module::JobLifecycleState::Failed;
            if (!registry.SetJobLifecycleState(job, state))
            {
                slicesoft::module::SyncCapabilityAdapter::Instance().ReleaseJob(job);
                (void)registry.ReleaseJob(job);
                SetInternalError("pm_submit could not publish terminal job state");
                return nullptr;
            }
            return job;
        }
        catch (...)
        {
            slicesoft::module::SyncCapabilityAdapter::Instance().ReleaseJob(job);
            (void)registry.ReleaseJob(job);
            throw;
        }
    }
    catch (...)
    {
        SetInternalError("pm_submit failed while validating the request");
        return nullptr;
    }
}

extern "C" PM_API int PM_CALL pm_poll(
    pm_job_t* job,
    char* progressJson,
    int cap,
    int* outRequired)
{
    try
    {
        if (slicesoft::module::HandleRegistry::Instance().FindJob(job) == nullptr)
        {
            SetInvalidRequestError("pm_poll received an unknown job handle");
            return PM_ERR_INVALID_ARG;
        }
        const std::string progress =
            slicesoft::module::SyncCapabilityAdapter::Instance().Poll(job);
        if (progress.empty())
        {
            SetInternalError("pm_poll could not resolve terminal progress");
            return PM_ERR_FAILED;
        }
        return slicesoft::module::WriteOut(
            progress,
            progressJson,
            cap,
            outRequired);
    }
    catch (...)
    {
        SetInternalError("pm_poll failed while validating the job handle");
        return PM_ERR_FAILED;
    }
}

extern "C" PM_API int PM_CALL pm_cancel(pm_job_t* job)
{
    try
    {
        if (!slicesoft::module::HandleRegistry::Instance().RequestCancel(job))
        {
            SetInvalidRequestError("pm_cancel received an unknown job handle");
            return PM_ERR_INVALID_ARG;
        }
        return PM_OK;
    }
    catch (...)
    {
        SetInternalError("pm_cancel failed while updating the job handle");
        return PM_ERR_FAILED;
    }
}

extern "C" PM_API int PM_CALL pm_result(
    pm_job_t* job,
    char* resultJson,
    int cap,
    int* outRequired)
{
    try
    {
        if (slicesoft::module::HandleRegistry::Instance().FindJob(job) == nullptr)
        {
            SetInvalidRequestError("pm_result received an unknown job handle");
            return PM_ERR_INVALID_ARG;
        }
        const auto result =
            slicesoft::module::SyncCapabilityAdapter::Instance().Result(job);
        if (!result)
        {
            SetInternalError("pm_result could not resolve terminal output");
            return PM_ERR_FAILED;
        }
        return slicesoft::module::WriteOut(
            result->bytes,
            resultJson,
            cap,
            outRequired);
    }
    catch (...)
    {
        SetInternalError("pm_result failed while validating the job handle");
        return PM_ERR_FAILED;
    }
}

extern "C" PM_API void PM_CALL pm_release(pm_job_t* job)
{
    try
    {
        auto& registry = slicesoft::module::HandleRegistry::Instance();
        if (!registry.ReleaseJob(job))
        {
            SetInvalidRequestError("pm_release received an unknown job handle");
            return;
        }
        slicesoft::module::SyncCapabilityAdapter::Instance().ReleaseJob(job);
    }
    catch (...)
    {
        SetInternalError("pm_release failed while retiring the job handle");
    }
}

extern "C" PM_API int PM_CALL pm_self_test(
    pm_module_t* module,
    char* reportJson,
    int cap,
    int* outRequired)
{
    try
    {
        if (slicesoft::module::HandleRegistry::Instance().FindModule(module) == nullptr)
        {
            SetInvalidRequestError("pm_self_test received an unknown module handle");
            return PM_ERR_INVALID_ARG;
        }
        return slicesoft::module::WriteOut(
            slicesoft::module::GetModuleSelfTestJson(),
            reportJson,
            cap,
            outRequired);
    }
    catch (...)
    {
        SetInternalError("pm_self_test failed while validating the module handle");
        return PM_ERR_FAILED;
    }
}

extern "C" PM_API int PM_CALL pm_last_error(char* jsonOut, int cap, int* outRequired)
{
    try
    {
        return slicesoft::module::WriteOut(
            slicesoft::module::GetThreadLastErrorJson(),
            jsonOut,
            cap,
            outRequired);
    }
    catch (...)
    {
        return PM_ERR_FAILED;
    }
}
