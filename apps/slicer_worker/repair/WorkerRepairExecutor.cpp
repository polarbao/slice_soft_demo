#include "slicer_worker/repair/WorkerRepairExecutor.h"

#include "slicer_core/api/ProfileIdentity.h"
#include "slicer_core/engine/ProductionRepairFacadeFactory.h"

#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace slicesoft::worker
{
namespace
{

constexpr const char* kCancelledCode{"PM-SLICER-CANCELLED-0070"};
constexpr const char* kInputCode{"PM-SLICER-INPUT-0001"};
constexpr const char* kInternalCode{"PM-SLICER-INTERNAL-0099"};
constexpr const char* kOutputCode{"PM-SLICER-OUTPUT-0050"};

class WorkerRepairInputError final : public std::runtime_error
{
public:
    WorkerRepairInputError(std::string code, const std::string& message)
        : std::runtime_error(message),
          m_code(std::move(code))
    {
    }

    /** @brief 返回稳定错误码。@return PM-SLICER 错误码。 */
    [[nodiscard]] const std::string& Code() const noexcept
    {
        return m_code;
    }

private:
    std::string m_code;
};

[[noreturn]] void Fail(const std::string& code, const std::string& message)
{
    throw WorkerRepairInputError(code, message);
}

const slicer_core::Json& RequireObject(
    const slicer_core::Json& object,
    const std::string& key)
{
    if (!object.is_object() || !object.contains(key)
        || !object.at(key).is_object())
    {
        Fail(kInputCode, "repair object field is missing or invalid: " + key);
    }
    return object.at(key);
}

std::string RequireString(
    const slicer_core::Json& object,
    const std::string& key)
{
    if (!object.is_object() || !object.contains(key)
        || !object.at(key).is_string()
        || object.at(key).as_string().empty())
    {
        Fail(kInputCode, "repair string field is missing or invalid: " + key);
    }
    return object.at(key).as_string();
}

bool RequireBool(
    const slicer_core::Json& object,
    const std::string& key)
{
    if (!object.is_object() || !object.contains(key)
        || !object.at(key).is_bool())
    {
        Fail(kInputCode, "repair boolean field is missing or invalid: " + key);
    }
    return object.at(key).as_bool();
}

bool IsWithin(
    const std::filesystem::path& child,
    const std::filesystem::path& parent)
{
    const std::filesystem::path normalizedChild =
        std::filesystem::absolute(child).lexically_normal();
    const std::filesystem::path normalizedParent =
        std::filesystem::absolute(parent).lexically_normal();
    auto childIterator = normalizedChild.begin();
    for (auto parentIterator = normalizedParent.begin();
         parentIterator != normalizedParent.end();
         ++parentIterator, ++childIterator)
    {
        if (childIterator == normalizedChild.end()
            || *childIterator != *parentIterator)
        {
            return false;
        }
    }
    return true;
}

void WriteJson(
    const std::filesystem::path& path,
    const slicer_core::Json& document)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output)
    {
        Fail(kOutputCode, "repair job file could not be created");
    }
    output << document.dump(2) << '\n';
    output.flush();
    if (!output)
    {
        Fail(kOutputCode, "repair job file could not be written completely");
    }
}

void RemovePaths(const std::vector<std::filesystem::path>& paths) noexcept
{
    std::error_code ignored;
    for (auto iterator = paths.rbegin(); iterator != paths.rend(); ++iterator)
    {
        std::filesystem::remove_all(*iterator, ignored);
    }
}

struct MaterializedRepair
{
    slicer_core::api::RepairRequest request;
    std::filesystem::path profilePath;
};

MaterializedRepair Materialize(const WorkerRequestEnvelope& envelope)
{
    if (envelope.Identity().Capability() != "geometry.repair"
        || !envelope.HasInput() || !envelope.HasProfile())
    {
        Fail(kInputCode, "geometry.repair requires input and profile objects");
    }

    const slicer_core::Json& input = envelope.Input();
    const std::filesystem::path jobRoot =
        envelope.Identity().RequestPath().parent_path();
    const std::filesystem::path modelPath =
        std::filesystem::path(RequireString(input, "modelPath"));
    const std::filesystem::path outputPath =
        std::filesystem::path(RequireString(input, "outputPath"));
    const slicer_core::Json& resourceScope =
        RequireObject(input, "sourceResourceScope");
    const std::filesystem::path resourceRoot =
        std::filesystem::path(RequireString(resourceScope, "rootPath"));
    if (!modelPath.is_absolute() || !outputPath.is_absolute()
        || !resourceRoot.is_absolute()
        || outputPath.parent_path() != jobRoot / "repair"
        || !IsWithin(modelPath, resourceRoot))
    {
        Fail(kInputCode, "repair paths do not satisfy job/resource ownership");
    }

    const std::string modelFormat = RequireString(input, "modelFormat");
    const std::string outputFormat =
        RequireString(input, "repairOutputFormat");
    const std::string policy = RequireString(input, "policy");
    const bool requireStrictPass = RequireBool(input, "requireStrictPass");
    if (modelFormat != "obj" || outputFormat != "obj"
        || policy != "conservative" || !requireStrictPass)
    {
        Fail(kInputCode, "repair policy or format is unsupported");
    }

    const std::string profileHash = RequireString(input, "profileHash");
    if (slicer_core::api::ComputeProfileDocumentHash(envelope.Profile())
        != profileHash)
    {
        Fail("PM-SLICER-PROFILE-0031", "repair Profile hash is stale");
    }

    std::filesystem::create_directories(jobRoot / "repair");
    const std::filesystem::path profilePath =
        jobRoot / "repair.profile.generated.json";
    WriteJson(profilePath, envelope.Profile());

    slicer_core::api::RepairRequest request;
    request.job_id = envelope.Identity().JobId();
    request.correlation_id = envelope.Identity().CorrelationId();
    if (input.contains("modelId") && input.at("modelId").is_string())
    {
        request.model_id = input.at("modelId").as_string();
    }
    request.source_model_path = modelPath.lexically_normal();
    request.repaired_model_path = outputPath.lexically_normal();
    request.profile_config_path = profilePath;
    request.source_resource_root = resourceRoot.lexically_normal();
    request.job_root_path = jobRoot;
    request.profile_hash = profileHash;
    request.model_format = modelFormat;
    request.repair_output_format = outputFormat;
    request.policy = policy;
    request.require_strict_pass = requireStrictPass;
    return {std::move(request), profilePath};
}

std::filesystem::path EvidencePath(const std::filesystem::path& outputPath)
{
    return std::filesystem::path(outputPath.generic_string() + ".evidence.json");
}

void PublishRepairBundle(
    const slicer_core::api::RepairResult& result,
    const std::filesystem::path& outputPath,
    const slicer_core::Json& evidence,
    std::vector<std::filesystem::path>* published)
{
    const std::filesystem::path stagingDirectory =
        result.repaired_model_path.parent_path();
    const std::filesystem::path evidencePath = EvidencePath(outputPath);
    const std::filesystem::path temporaryEvidence =
        std::filesystem::path(evidencePath.generic_string() + ".tmp");
    if (!std::filesystem::is_directory(stagingDirectory)
        || result.repaired_model_path.filename() != outputPath.filename()
        || std::filesystem::exists(outputPath))
    {
        Fail(kOutputCode, "repair staging output cannot be published safely");
    }

    try
    {
        for (const std::filesystem::directory_entry& entry
             : std::filesystem::directory_iterator(stagingDirectory))
        {
            if (!entry.is_regular_file())
            {
                Fail(kOutputCode, "repair staging contains a non-file entry");
            }
            const std::filesystem::path destination =
                outputPath.parent_path() / entry.path().filename();
            if (std::filesystem::exists(destination))
            {
                Fail(kOutputCode, "repair output resource already exists");
            }
            std::filesystem::rename(entry.path(), destination);
            published->push_back(destination);
        }
        std::error_code cleanupError;
        std::filesystem::remove(stagingDirectory, cleanupError);
        if (cleanupError || !std::filesystem::is_regular_file(outputPath))
        {
            Fail(kOutputCode, "repair staging directory could not be finalized");
        }

        WriteJson(temporaryEvidence, evidence);
        std::filesystem::rename(temporaryEvidence, evidencePath);
        published->push_back(evidencePath);
    }
    catch (const WorkerRepairInputError&)
    {
        RemovePaths(*published);
        RemovePaths({stagingDirectory, temporaryEvidence, evidencePath});
        throw;
    }
    catch (const std::filesystem::filesystem_error& error)
    {
        RemovePaths(*published);
        RemovePaths({stagingDirectory, temporaryEvidence, evidencePath});
        Fail(kOutputCode, std::string("repair publication failed: ") + error.what());
    }
}

slicer_core::Json BuildWorkerEvidence(
    const slicer_core::api::RepairResult& result)
{
    if (!result.evidence.is_object() || !result.evidence.contains("asset")
        || !result.evidence.at("asset").is_object())
    {
        Fail(kInternalCode, "repair facade omitted required asset evidence");
    }
    const slicer_core::Json& asset = result.evidence.at("asset");
    return slicer_core::Json::object({
        {"assetWritten", asset.at("assetWritten")},
        {"assetReimported", asset.at("assetReimported")},
        {"strictComplete", asset.at("strictComplete")},
        {"strictPass", asset.at("strictPass")},
        {"attributesPreserved", asset.at("attributesPreserved")},
        {"publicationState", "job_owned"},
        {"repairReport", result.evidence},
    });
}

}  // namespace

WorkerRepairExecutor::WorkerRepairExecutor(
    std::unique_ptr<slicer_core::api::RepairFacade> facade)
    : m_facade(std::move(facade))
{
    if (!m_facade)
    {
        throw std::invalid_argument("WorkerRepairExecutor requires a facade");
    }
}

WorkerCapabilityExecutionResult WorkerRepairExecutor::Execute(
    const WorkerRequestEnvelope& request,
    const slicer_core::api::ICancelToken& cancelToken)
{
    std::optional<MaterializedRepair> materialized;
    std::vector<std::filesystem::path> published;
    try
    {
        materialized = Materialize(request);
        const auto response = m_facade->Run(materialized->request, cancelToken);
        std::error_code ignored;
        std::filesystem::remove(materialized->profilePath, ignored);
        if (!response.IsOk())
        {
            const slicer_core::api::ApiError* error = response.Error();
            const std::string code = error == nullptr
                ? kInternalCode : error->code;
            const std::string message = error == nullptr
                ? "repair facade returned no result" : error->message;
            const std::optional<std::string> detail =
                error == nullptr || error->detail.empty()
                ? std::nullopt
                : std::optional<std::string>{error->detail};
            const std::optional<WorkerResultCleanup> cleanup =
                code == kCancelledCode
                ? std::optional<WorkerResultCleanup>{
                    WorkerResultCleanup{true, false}}
                : std::nullopt;
            return WorkerCapabilityExecutionResult::Failure(
                code, message, detail, cleanup);
        }
        if (response.Value() == nullptr)
        {
            return WorkerCapabilityExecutionResult::Failure(
                kInternalCode, "repair facade returned no output");
        }

        const slicer_core::api::RepairResult& value = *response.Value();
        const slicer_core::Json evidence = BuildWorkerEvidence(value);
        PublishRepairBundle(
            value, materialized->request.repaired_model_path,
            evidence, &published);
        return WorkerCapabilityExecutionResult::Success(
            slicer_core::Json::object({
                {"outputPath", materialized->request.repaired_model_path.generic_string()},
                {"sourceDigest", value.source_hash},
                {"outputDigest", value.repaired_hash},
                {"preflightBefore", value.preflight_before},
                {"preflightAfter", value.preflight_after},
                {"evidence", evidence},
                {"elapsedMs", static_cast<double>(value.elapsed_ms)},
            }));
    }
    catch (const WorkerRepairInputError& error)
    {
        RemovePaths(published);
        if (materialized.has_value())
        {
            std::error_code ignored;
            std::filesystem::remove(materialized->profilePath, ignored);
        }
        return WorkerCapabilityExecutionResult::Failure(
            error.Code(), error.what());
    }
    catch (const std::exception& error)
    {
        RemovePaths(published);
        if (materialized.has_value())
        {
            std::error_code ignored;
            std::filesystem::remove(materialized->profilePath, ignored);
        }
        return WorkerCapabilityExecutionResult::Failure(
            kInternalCode,
            "unexpected repair executor failure",
            std::string(error.what()));
    }
}

std::unique_ptr<IWorkerCapabilityExecutor>
CreateProductionWorkerRepairExecutor()
{
    return std::make_unique<WorkerRepairExecutor>(
        slicer_core::engine::CreateProductionRepairFacade());
}

}  // namespace slicesoft::worker
