#include "slicer_module/WorkerJobService.h"

#include "slicer_core/api/artifacts/PackageArtifactSafety.h"
#include "slicer_module/HandleRegistry.h"
#include "slicer_module/WorkerClient.h"
#include "slicer_module/WorkerContract.h"
#include "slicer_module/WorkerProcessWindows.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <Windows.h>
extern "C" IMAGE_DOS_HEADER __ImageBase;
#endif

namespace slicesoft::module
{
namespace
{

constexpr std::string_view SuccessCode{"PM-SLICER-OK-0000"};
constexpr std::string_view CancelledCode{"PM-SLICER-CANCELLED-0070"};
constexpr std::string_view ContractCode{"PM-SLICER-CONTRACT-0060"};
constexpr std::string_view OutputCode{"PM-SLICER-OUTPUT-0050"};
constexpr std::string_view InternalCode{"PM-SLICER-INTERNAL-0099"};
constexpr std::string_view ResourceCode{"PM-SLICER-RESOURCE-0041"};

std::filesystem::path ResolveWorkerExecutable()
{
#if defined(_WIN32)
    std::vector<wchar_t> buffer(1024U, L'\0');
    const HMODULE module = reinterpret_cast<HMODULE>(&__ImageBase);
    for (;;)
    {
        const DWORD length = GetModuleFileNameW(
            module,
            buffer.data(),
            static_cast<DWORD>(buffer.size()));
        if (length == 0U)
        {
            throw std::runtime_error("slicer module deployment path is unavailable");
        }
        if (length + 1U < buffer.size())
        {
            return (std::filesystem::path(buffer.data(), buffer.data() + length)
                    .parent_path()
                / "slicer_worker.exe")
                .lexically_normal();
        }
        buffer.resize(buffer.size() * 2U, L'\0');
    }
#else
    return (std::filesystem::current_path() / "slicer_worker").lexically_normal();
#endif
}

std::filesystem::path MakePrivateJobRoot(
    const std::uint64_t moduleId,
    const std::uint64_t jobId)
{
    return (std::filesystem::temp_directory_path()
        / "slicesoft-module"
#if defined(_WIN32)
        / std::to_string(GetCurrentProcessId())
#else
        / "process"
#endif
        / ("module-" + std::to_string(moduleId))
        / ("job-" + std::to_string(jobId)))
        .lexically_normal();
}

void WriteJsonAtomically(
    const std::filesystem::path& path,
    const slicer_core::Json& document)
{
    std::filesystem::create_directories(path.parent_path());
    const std::filesystem::path temporary = path.parent_path() / "request.tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output)
        {
            throw std::runtime_error("Worker request file could not be created");
        }
        output << document.dump(2) << '\n';
        output.flush();
        if (!output)
        {
            throw std::runtime_error("Worker request file could not be written completely");
        }
    }
    std::filesystem::rename(temporary, path);
}

slicer_core::Json ReadJson(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        throw std::runtime_error("Worker result file could not be opened");
    }
    return slicer_core::Json::parse(input);
}

std::string LifecycleName(const JobLifecycleState state)
{
    switch (state)
    {
    case JobLifecycleState::Queued:
        return "queued";
    case JobLifecycleState::Running:
        return "running";
    case JobLifecycleState::Cancelling:
        return "cancelling";
    case JobLifecycleState::Succeeded:
        return "succeeded";
    case JobLifecycleState::Failed:
        return "failed";
    case JobLifecycleState::Cancelled:
        return "cancelled";
    }
    return "failed";
}

CapabilityOutput FailureOutput(
    const std::string_view code,
    const std::string_view message,
    const std::string_view detail = {})
{
    return MakeCapabilityOutput(MakeFailure(code, message, detail));
}

struct PublishedRepairAsset
{
    std::filesystem::path temporary;
    std::filesystem::path destination;
    bool published{false};
};

void RollBackRepairAssets(std::vector<PublishedRepairAsset>& assets) noexcept
{
    std::error_code ignored;
    for (auto iterator = assets.rbegin(); iterator != assets.rend(); ++iterator)
    {
        if (iterator->published)
        {
            std::filesystem::remove(iterator->destination, ignored);
        }
        std::filesystem::remove(iterator->temporary, ignored);
    }
}

void PublishRepairAssets(
    const std::filesystem::path& workerOutput,
    const std::filesystem::path& requestedOutput,
    const std::string& jobId)
{
    if (!requestedOutput.is_absolute() || requestedOutput.filename().empty())
    {
        throw std::runtime_error("public repair outputPath must be an absolute file path");
    }
    const std::filesystem::path sourceDirectory = workerOutput.parent_path();
    if (!std::filesystem::is_regular_file(workerOutput))
    {
        throw std::runtime_error("Worker repair result did not publish the primary OBJ asset");
    }
    std::filesystem::create_directories(requestedOutput.parent_path());

    std::vector<PublishedRepairAsset> assets;
    for (const auto& entry : std::filesystem::directory_iterator(sourceDirectory))
    {
        if (!entry.is_regular_file())
        {
            throw std::runtime_error("Worker repair output contains a non-file entry");
        }
        const std::filesystem::path destination = entry.path() == workerOutput
            ? requestedOutput
            : requestedOutput.parent_path() / entry.path().filename();
        if (std::filesystem::exists(destination))
        {
            throw std::runtime_error(
                "public repair output would overwrite an existing asset: "
                + destination.generic_string());
        }
        const std::filesystem::path temporary = destination.parent_path()
            / (destination.filename().generic_string() + ".staging." + jobId);
        if (std::filesystem::exists(temporary)
            || !std::filesystem::copy_file(entry.path(), temporary))
        {
            RollBackRepairAssets(assets);
            throw std::runtime_error("public repair asset staging failed");
        }
        assets.push_back({temporary, destination, false});
    }

    try
    {
        for (auto& asset : assets)
        {
            std::filesystem::rename(asset.temporary, asset.destination);
            asset.published = true;
        }
    }
    catch (...)
    {
        RollBackRepairAssets(assets);
        throw;
    }
}

}  // namespace

struct WorkerJobService::Implementation
{
    struct JobExecution
    {
        pm_job_t* job{nullptr};
        pm_module_t* module{nullptr};
        CapabilityRoute route;
        std::filesystem::path jobRoot;
        std::filesystem::path requestPath;
        std::filesystem::path resultPath;
        std::filesystem::path cancelPath;
        std::filesystem::path workerExecutable;
        std::optional<std::filesystem::path> requestedRepairOutput;
        std::optional<std::filesystem::path> workerRepairOutput;
        std::optional<WorkerPackageArtifactContext> packageArtifacts;
        std::unique_ptr<WorkerClient> client{std::make_unique<WorkerClient>()};
        std::thread thread;
        mutable std::mutex mutex;
        WorkerProgressEvent progress;
        std::optional<CapabilityOutput> output;
        bool cancellationRequested{false};
        bool workCompleted{false};
        std::atomic_bool terminal{false};
    };

    mutable std::mutex mutex;
    std::map<pm_job_t*, std::shared_ptr<JobExecution>> jobs;

    [[nodiscard]] std::shared_ptr<JobExecution> Find(pm_job_t* job) const
    {
        std::scoped_lock lock{mutex};
        const auto entry = jobs.find(job);
        return entry == jobs.end() ? nullptr : entry->second;
    }

    static slicer_core::Json MakeWorkerRequest(JobExecution& execution)
    {
        slicer_core::Json::Object document = execution.route.workerPayload.as_object();
        document.emplace("contract", "file_contract");
        document.emplace("major", 1);
        document.emplace("minor", 0);
        document.emplace("jobId", execution.route.jobId);
        document.emplace("correlationId", execution.route.correlationId);
        document.emplace("capability", execution.route.workerCapability);
        document.emplace(
            "timeoutMs",
            static_cast<double>(execution.route.timeout.count()));

        if (execution.route.workerCapability == "geometry.repair")
        {
            slicer_core::Json::Object payload = std::move(document);
            slicer_core::Json::Object input = payload.at("input").as_object();
            const std::filesystem::path requested{
                input.at("outputPath").as_string()};
            const std::filesystem::path fileName = requested.filename().empty()
                ? std::filesystem::path{"repaired.obj"}
                : requested.filename();
            execution.requestedRepairOutput =
                std::filesystem::absolute(requested).lexically_normal();
            execution.workerRepairOutput =
                execution.jobRoot / "repair" / fileName;
            input.insert_or_assign(
                "outputPath",
                execution.workerRepairOutput->generic_string());
            payload.insert_or_assign("input", slicer_core::Json{std::move(input)});
            return slicer_core::Json{std::move(payload)};
        }

        if (execution.route.workerCapability == "slice.rgbwsv")
        {
            const slicer_core::Json& output = document.at("output");
            const std::filesystem::path packageDirectory{
                output.at("packageDir").as_string()};
            const auto identity =
                slicer_core::api::artifacts::MakePackageArtifactIdentity(
                    packageDirectory,
                    execution.route.jobId,
                    slicer_core::api::artifacts::MakePackageAttemptId(
                        execution.route.correlationId));
            execution.packageArtifacts = WorkerPackageArtifactContext{
                identity.package_directory,
                identity.job_id,
                identity.attempt_id};
        }
        return slicer_core::Json{std::move(document)};
    }

    static CapabilityOutput ParseResult(JobExecution& execution)
    {
        const slicer_core::Json result = ReadJson(execution.resultPath);
        if (!result.is_object()
            || !result.contains("contract")
            || result.at("contract").as_string() != "file_contract"
            || result.at("major").as_int() != 1
            || result.at("minor").as_int() != 0
            || result.at("jobId").as_string() != execution.route.jobId
            || result.at("correlationId").as_string()
                != execution.route.correlationId
            || result.at("capability").as_string()
                != execution.route.workerCapability
            || !result.contains("ok")
            || !result.at("ok").is_bool()
            || !result.contains("code")
            || !result.at("code").is_string())
        {
            return FailureOutput(
                ContractCode,
                "Worker result identity does not close against the request");
        }

        if (!result.at("ok").as_bool())
        {
            if (!result.contains("error") || !result.at("error").is_object()
                || !result.at("error").contains("message")
                || !result.at("error").at("message").is_string())
            {
                return FailureOutput(
                    ContractCode,
                    "Worker failure result does not contain a valid error object");
            }
            const slicer_core::Json& error = result.at("error");
            const std::string detail = error.contains("detail")
                && error.at("detail").is_string()
                ? error.at("detail").as_string()
                : std::string{};
            return FailureOutput(
                result.at("code").as_string(),
                error.at("message").as_string(),
                detail);
        }

        if (result.at("code").as_string() != SuccessCode
            || !result.contains("output")
            || !result.at("output").is_object())
        {
            return FailureOutput(
                ContractCode,
                "Worker success result does not satisfy file_contract_v1");
        }

        slicer_core::Json::Object output = result.at("output").as_object();
        if (execution.requestedRepairOutput.has_value()
            && execution.workerRepairOutput.has_value())
        {
            PublishRepairAssets(
                *execution.workerRepairOutput,
                *execution.requestedRepairOutput,
                execution.route.jobId);
            output.insert_or_assign(
                "outputPath",
                execution.requestedRepairOutput->generic_string());
        }
        return MakeCapabilityOutput(MakeSuccess(std::move(output)));
    }

    static void Finalize(
        const std::shared_ptr<JobExecution>& execution,
        CapabilityOutput output)
    {
        std::scoped_lock lock{execution->mutex};
        if (execution->cancellationRequested)
        {
            output = FailureOutput(
                CancelledCode,
                "Worker cancellation was requested");
        }
        execution->output = std::move(output);
        execution->progress.phase = "completed";
        execution->progress.current = 1U;
        execution->progress.total = 1U;
        execution->progress.percent = 100U;
        execution->terminal.store(true, std::memory_order_release);
        const JobLifecycleState state = execution->cancellationRequested
            ? JobLifecycleState::Cancelled
            : execution->output->succeeded
                ? JobLifecycleState::Succeeded
                : JobLifecycleState::Failed;
        (void)HandleRegistry::Instance().SetJobLifecycleState(
            execution->job,
            state);
    }

    static CapabilityOutput TransportFailure(const WorkerRunResult& result)
    {
        const std::string code = result.errorCode.empty()
            ? std::string{InternalCode}
            : result.errorCode;
        const std::string message = result.errorMessage.empty()
            ? "Worker process failed without a diagnostic"
            : result.errorMessage;
        return FailureOutput(code, message);
    }

    static void Run(const std::shared_ptr<JobExecution>& execution) noexcept
    {
        try
        {
            bool cancelledBeforeStart{false};
            {
                std::scoped_lock lock{execution->mutex};
                if (execution->cancellationRequested)
                {
                    execution->workCompleted = true;
                    cancelledBeforeStart = true;
                }
                else
                {
                    execution->progress.phase = "worker_contract";
                    (void)HandleRegistry::Instance().SetJobLifecycleState(
                        execution->job,
                        JobLifecycleState::Running);
                }
            }
            if (cancelledBeforeStart)
            {
                Finalize(
                    execution,
                    FailureOutput(CancelledCode, "Worker job was cancelled before start"));
                return;
            }

            WorkerContractRequirement requirement;
            requirement.requiredCapabilities = {execution->route.workerCapability};
            const WorkerContractResult contract =
                WorkerContractNegotiator{*execution->client}.Negotiate(
                    execution->workerExecutable,
                    requirement);
            if (!contract.compatible)
            {
                {
                    std::scoped_lock lock{execution->mutex};
                    execution->workCompleted = true;
                }
                Finalize(
                    execution,
                    FailureOutput(
                        contract.errorCode.empty()
                            ? InternalCode
                            : std::string_view{contract.errorCode},
                        contract.errorMessage.empty()
                            ? "Worker contract negotiation failed"
                            : std::string_view{contract.errorMessage}));
                return;
            }

            bool cancelledBeforeExecution{false};
            {
                std::scoped_lock lock{execution->mutex};
                if (execution->cancellationRequested)
                {
                    execution->workCompleted = true;
                    cancelledBeforeExecution = true;
                }
                else
                {
                    execution->progress.phase = "worker_execute";
                }
            }
            if (cancelledBeforeExecution)
            {
                Finalize(
                    execution,
                    FailureOutput(CancelledCode, "Worker job was cancelled before execution"));
                return;
            }

            WorkerLaunchOptions options;
            options.executablePath = execution->workerExecutable;
            options.arguments = {
                "--spi-request",
                execution->requestPath.generic_string()};
            options.workingDirectory = execution->workerExecutable.parent_path();
            options.cancellationMarkerPath = execution->cancelPath;
            options.timeout = execution->route.timeout;
            options.cancelGracePeriod = std::chrono::milliseconds{2000};
            options.requireTerminalProgress =
                execution->route.workerCapability == "slice.rgbwsv";
            options.packageArtifacts = execution->packageArtifacts;
            options.progressSink = [weak = std::weak_ptr<JobExecution>{execution}](
                                       const WorkerProgressEvent& event)
            {
                if (const auto current = weak.lock())
                {
                    std::scoped_lock lock{current->mutex};
                    current->progress = event;
                }
            };
            const WorkerRunResult run = execution->client->Run(options);
            bool cancellationRequested{false};
            {
                std::scoped_lock lock{execution->mutex};
                execution->workCompleted = true;
                cancellationRequested = execution->cancellationRequested;
            }

            CapabilityOutput output;
            if (std::filesystem::is_regular_file(execution->resultPath))
            {
                output = ParseResult(*execution);
                if (output.succeeded
                    && run.exitCategory != WorkerExitCategory::Ok)
                {
                    output = FailureOutput(
                        ContractCode,
                        "Worker process and result terminal states disagree");
                }
            }
            else if (cancellationRequested
                || run.stopReason == WorkerStopReason::Cancelled)
            {
                output = FailureOutput(
                    CancelledCode,
                    "Worker cancellation was requested");
            }
            else
            {
                output = TransportFailure(run);
            }
            Finalize(execution, std::move(output));
        }
        catch (const std::exception& error)
        {
            {
                std::scoped_lock lock{execution->mutex};
                execution->workCompleted = true;
            }
            Finalize(
                execution,
                FailureOutput(
                    OutputCode,
                    "Worker job result could not be published",
                    error.what()));
        }
        catch (...)
        {
            {
                std::scoped_lock lock{execution->mutex};
                execution->workCompleted = true;
            }
            Finalize(
                execution,
                FailureOutput(
                    InternalCode,
                    "Worker job failed with an unknown exception"));
        }
    }

    static void JoinAndCleanup(const std::shared_ptr<JobExecution>& execution) noexcept
    {
        try
        {
            {
                std::scoped_lock lock{execution->mutex};
                if (!execution->terminal.load(std::memory_order_acquire)
                    && !execution->workCompleted)
                {
                    execution->cancellationRequested = true;
                    (void)HandleRegistry::Instance().RequestCancel(execution->job);
                }
            }
            (void)worker_detail::WriteCancellationMarker(execution->cancelPath);
            (void)execution->client->RequestCancel();
            if (execution->thread.joinable())
            {
                execution->thread.join();
            }
            std::error_code ignored;
            std::filesystem::remove_all(execution->jobRoot, ignored);
        }
        catch (...)
        {
        }
    }
};

WorkerJobService& WorkerJobService::Instance()
{
    static WorkerJobService service;
    return service;
}

WorkerJobService::WorkerJobService()
    : m_implementation{std::make_unique<Implementation>()}
{
}

WorkerJobService::~WorkerJobService()
{
    std::vector<std::shared_ptr<Implementation::JobExecution>> jobs;
    {
        std::scoped_lock lock{m_implementation->mutex};
        for (const auto& [handle, execution] : m_implementation->jobs)
        {
            (void)handle;
            jobs.push_back(execution);
        }
        m_implementation->jobs.clear();
    }
    for (const auto& execution : jobs)
    {
        Implementation::JoinAndCleanup(execution);
    }
}

WorkerJobSubmission WorkerJobService::Submit(
    pm_job_t* const job,
    pm_module_t* const module,
    CapabilityRoute route,
    const std::uint64_t moduleId,
    const std::uint64_t jobId)
{
    WorkerJobSubmission result;
    if (job == nullptr || module == nullptr || !route.accepted
        || route.carrier != CapabilityCarrier::Worker)
    {
        result.errorCode = "PM-SLICER-INPUT-0002";
        result.errorMessage = "Worker job submission is invalid";
        return result;
    }

    auto execution = std::make_shared<Implementation::JobExecution>();
    execution->job = job;
    execution->module = module;
    execution->route = std::move(route);
    execution->route.jobId = execution->route.jobId.empty()
        ? "pm-job-" + std::to_string(jobId)
        : execution->route.jobId;
    execution->route.correlationId = execution->route.correlationId.empty()
        ? "module-" + std::to_string(moduleId)
            + "-job-" + std::to_string(jobId)
        : execution->route.correlationId;
    execution->jobRoot = MakePrivateJobRoot(moduleId, jobId);
    execution->requestPath = execution->jobRoot / "request.json";
    execution->resultPath = execution->jobRoot / "result.json";
    execution->cancelPath = execution->jobRoot / "cancel.requested";

    try
    {
        std::error_code cleanupError;
        std::filesystem::remove_all(execution->jobRoot, cleanupError);
        execution->workerExecutable = ResolveWorkerExecutable();
        WriteJsonAtomically(
            execution->requestPath,
            Implementation::MakeWorkerRequest(*execution));

        {
            std::scoped_lock lock{m_implementation->mutex};
            for (const auto& [handle, active] : m_implementation->jobs)
            {
                (void)handle;
                if (active->module == module
                    && !active->terminal.load(std::memory_order_acquire))
                {
                    result.errorCode = std::string{ResourceCode};
                    result.errorMessage = "module Worker concurrency limit was reached";
                    result.errorDetail = "maxConcurrentJobs=1";
                    std::filesystem::remove_all(execution->jobRoot, cleanupError);
                    return result;
                }
            }
            m_implementation->jobs.emplace(job, execution);
        }
        try
        {
            execution->thread = std::thread{&Implementation::Run, execution};
        }
        catch (...)
        {
            std::scoped_lock lock{m_implementation->mutex};
            m_implementation->jobs.erase(job);
            throw;
        }
    }
    catch (const std::exception& error)
    {
        std::error_code ignored;
        std::filesystem::remove_all(execution->jobRoot, ignored);
        result.errorCode = std::string{OutputCode};
        result.errorMessage = "Worker job could not be prepared";
        result.errorDetail = error.what();
        return result;
    }

    result.accepted = true;
    return result;
}

bool WorkerJobService::HasJob(pm_job_t* const job) const
{
    return m_implementation->Find(job) != nullptr;
}

std::string WorkerJobService::Poll(pm_job_t* const job) const
{
    const auto execution = m_implementation->Find(job);
    if (!execution)
    {
        return {};
    }
    const auto state = HandleRegistry::Instance().FindJob(job);
    if (!state)
    {
        return {};
    }
    std::scoped_lock lock{execution->mutex};
    return slicer_core::Json::object({
        {"state", LifecycleName(state->LifecycleState())},
        {"phase", execution->progress.phase.empty()
            ? "queued"
            : execution->progress.phase},
        {"current", execution->progress.current},
        {"total", execution->progress.total},
        {"percent", static_cast<int>(execution->progress.percent)},
        {"elapsedMs", execution->progress.elapsedMs},
        {"capability", execution->route.publicCapability}}).dump(0);
}

std::shared_ptr<const CapabilityOutput> WorkerJobService::Result(
    pm_job_t* const job) const
{
    const auto execution = m_implementation->Find(job);
    if (!execution)
    {
        return nullptr;
    }
    std::scoped_lock lock{execution->mutex};
    if (!execution->terminal.load(std::memory_order_acquire)
        || !execution->output.has_value())
    {
        return nullptr;
    }
    return std::make_shared<const CapabilityOutput>(*execution->output);
}

bool WorkerJobService::RequestCancel(pm_job_t* const job) noexcept
{
    try
    {
        const auto execution = m_implementation->Find(job);
        if (!execution)
        {
            return false;
        }
        bool signalWorker{false};
        {
            std::scoped_lock lock{execution->mutex};
            if (execution->terminal.load(std::memory_order_acquire)
                || execution->workCompleted)
            {
                return true;
            }
            execution->cancellationRequested = true;
            signalWorker = true;
            (void)HandleRegistry::Instance().RequestCancel(job);
        }
        if (signalWorker)
        {
            (void)worker_detail::WriteCancellationMarker(execution->cancelPath);
            (void)execution->client->RequestCancel();
        }
        return true;
    }
    catch (...)
    {
        return false;
    }
}

void WorkerJobService::ReleaseJob(pm_job_t* const job) noexcept
{
    try
    {
        std::shared_ptr<Implementation::JobExecution> execution;
        {
            std::scoped_lock lock{m_implementation->mutex};
            const auto entry = m_implementation->jobs.find(job);
            if (entry == m_implementation->jobs.end())
            {
                return;
            }
            execution = entry->second;
            m_implementation->jobs.erase(entry);
        }
        Implementation::JoinAndCleanup(execution);
    }
    catch (...)
    {
    }
}

void WorkerJobService::RemoveModule(pm_module_t* const module) noexcept
{
    try
    {
        std::vector<std::shared_ptr<Implementation::JobExecution>> jobs;
        {
            std::scoped_lock lock{m_implementation->mutex};
            for (auto entry = m_implementation->jobs.begin();
                 entry != m_implementation->jobs.end();)
            {
                if (entry->second->module == module)
                {
                    jobs.push_back(entry->second);
                    entry = m_implementation->jobs.erase(entry);
                }
                else
                {
                    ++entry;
                }
            }
        }
        for (const auto& execution : jobs)
        {
            Implementation::JoinAndCleanup(execution);
        }
    }
    catch (...)
    {
    }
}

}  // namespace slicesoft::module
