#include "slicer_core/engine/ProductionRepairFacadeFactory.h"

#include "slicer_core/api/ProfileIdentity.h"
#include "slicer_core/config.h"
#include "slicer_core/diagnostics/MeshRepairReport.h"
#include "slicer_core/geometry/SceneModelTriangleMeshAdapter.h"
#include "slicer_core/geometry/repair/DeterministicObjAssetWriter.h"
#include "slicer_core/geometry/repair/MeshRepairHash.h"
#include "slicer_core/geometry/repair/MeshRepairService.h"
#include "slicer_core/geometry/repair/StrictRepairAssetRecheck.h"
#include "slicer_core/model.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <exception>
#include <fstream>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace slicer_core::engine
{
namespace
{

using Clock = std::chrono::steady_clock;

constexpr const char* kCancelledCode{"PM-SLICER-CANCELLED-0070"};
constexpr const char* kInputCode{"PM-SLICER-INPUT-0001"};
constexpr const char* kInternalCode{"PM-SLICER-INTERNAL-0099"};
constexpr const char* kOutputCode{"PM-SLICER-OUTPUT-0050"};
constexpr const char* kTopologyBlockedCode{"PM-SLICER-TOPOLOGY-0010"};
constexpr const char* kTopologyPostCode{"PM-SLICER-TOPOLOGY-0011"};

api::ApiError MakeError(
    std::string code,
    std::string message,
    std::string detail = {})
{
    api::ApiError error;
    error.code = std::move(code);
    error.message = std::move(message);
    error.detail = std::move(detail);
    return error;
}

Json ReadJson(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        throw std::runtime_error("failed to open effective Profile");
    }
    return Json::parse(input);
}

std::string ReadBytes(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        throw std::runtime_error(
            "failed to read asset bytes: " + path.generic_string());
    }
    return std::string(
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>());
}

std::string AssetDigest(
    const std::filesystem::path& objPath,
    const std::filesystem::path& mtlPath,
    const std::vector<std::filesystem::path>& texturePaths)
{
    std::string payload{"slicesoft.repair.asset.bundle.1\n"};
    const auto append = [&](const std::filesystem::path& path)
    {
        if (path.empty())
        {
            return;
        }
        const std::string bytes = ReadBytes(path);
        payload += path.filename().generic_string();
        payload += '\n';
        payload += std::to_string(bytes.size());
        payload += '\n';
        payload += bytes;
        payload += '\n';
    };
    append(objPath);
    append(mtlPath);
    std::vector<std::filesystem::path> sortedTextures = texturePaths;
    std::sort(sortedTextures.begin(), sortedTextures.end());
    for (const std::filesystem::path& path : sortedTextures)
    {
        append(path);
    }
    return "sha256:" + ComputeMeshRepairSha256(payload);
}

std::string SourceDigest(const std::filesystem::path& path)
{
    return "sha256:" + ComputeMeshRepairSha256(ReadBytes(path));
}

bool IsSafeIdentityToken(const std::string& value)
{
    return !value.empty() && std::all_of(
        value.begin(), value.end(),
        [](const unsigned char character)
        {
            return std::isalnum(character) != 0
                || character == '-' || character == '_';
        });
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

void ValidateRequest(const api::RepairRequest& request)
{
    if (!IsSafeIdentityToken(request.job_id)
        || request.correlation_id.empty())
    {
        throw std::invalid_argument("repair request identity is invalid");
    }
    if (request.model_format != "obj"
        || request.repair_output_format != "obj"
        || request.policy != "conservative"
        || !request.require_strict_pass)
    {
        throw std::invalid_argument("repair request policy/format is unsupported");
    }
    if (!std::filesystem::is_regular_file(request.source_model_path)
        || request.source_model_path.extension() != ".obj"
        || !std::filesystem::is_regular_file(request.profile_config_path))
    {
        throw std::invalid_argument("repair source/Profile input is invalid");
    }
    if (request.repaired_model_path.extension() != ".obj"
        || request.repaired_model_path == request.source_model_path
        || request.source_resource_root.empty()
        || request.job_root_path.empty()
        || !IsWithin(request.source_model_path, request.source_resource_root)
        || !IsWithin(request.repaired_model_path, request.job_root_path))
    {
        throw std::invalid_argument("repair path ownership is invalid");
    }
    if (std::filesystem::exists(request.repaired_model_path))
    {
        throw std::invalid_argument("repair output already exists");
    }
    if (request.profile_hash.empty()
        || api::ComputeProfileDocumentHash(ReadJson(request.profile_config_path))
            != request.profile_hash)
    {
        throw std::invalid_argument("effective Profile identity is stale");
    }
}

Json TopologyJson(const MeshRepairDiagnosticsSummary& topology)
{
    return Json::object({
        {"available", topology.available},
        {"strictPass", topology.strictPass},
        {"boundaryEdges", static_cast<double>(topology.boundaryEdges)},
        {"nonManifoldEdges", static_cast<double>(topology.nonManifoldEdges)},
        {"duplicateFaces", static_cast<double>(topology.duplicateFaces)},
        {"oppositeDuplicateFaces", static_cast<double>(topology.oppositeDuplicateFaces)},
        {"localWindingIssues", static_cast<double>(topology.localWindingIssues)},
        {"degenerateTriangles", static_cast<double>(topology.degenerateTriangles)},
        {"connectedComponents", static_cast<double>(topology.connectedComponents)},
        {"selfIntersectionPairs", static_cast<double>(
            topology.confirmedSelfIntersectionPairs)}});
}

void RemoveOutputBundle(
    const std::filesystem::path& objPath,
    const std::filesystem::path& mtlPath,
    const std::vector<std::filesystem::path>& texturePaths,
    const std::filesystem::path& stagingDirectory)
{
    std::error_code ignored;
    std::filesystem::remove(objPath, ignored);
    if (!mtlPath.empty())
    {
        std::filesystem::remove(mtlPath, ignored);
    }
    for (const std::filesystem::path& texturePath : texturePaths)
    {
        std::filesystem::remove(texturePath, ignored);
    }
    if (!stagingDirectory.empty())
    {
        std::filesystem::remove_all(stagingDirectory, ignored);
    }
}

class CancelledError final : public std::runtime_error
{
public:
    CancelledError()
        : std::runtime_error("repair operation was cancelled")
    {
    }
};

void ThrowIfCancelled(const api::ICancelToken& cancelToken)
{
    if (cancelToken.IsCancelRequested())
    {
        throw CancelledError();
    }
}

class TopologyBlockedError final : public std::runtime_error
{
public:
    TopologyBlockedError(std::string code, std::string message)
        : std::runtime_error(std::move(message)), m_code(std::move(code))
    {
    }

    const std::string& Code() const noexcept
    {
        return m_code;
    }

private:
    std::string m_code;
};

class ProductionRepairFacade final : public api::RepairFacade
{
public:
    api::ApiResult<api::RepairResult> Run(
        const api::RepairRequest& request,
        const api::ICancelToken& cancelToken) noexcept override
    {
        const Clock::time_point start = Clock::now();
        std::filesystem::path writtenObj;
        std::filesystem::path writtenMtl;
        std::vector<std::filesystem::path> writtenTextures;
        std::filesystem::path stagingDirectory;
        try
        {
            ValidateRequest(request);
            ThrowIfCancelled(cancelToken);

            SliceConfig profile = load_slice_config(request.profile_config_path);
            profile.input.model_path = request.source_model_path;
            profile.input.format = "obj";
            validate_slice_config(profile);
            const ModelReport sourceModel = load_model_report(
                profile, request.profile_config_path.parent_path());
            for (const MaterialInfo& material : sourceModel.material_infos)
            {
                if (material.has_texture && !material.texture_exists)
                {
                    throw std::invalid_argument(
                        "repair source texture resource is missing");
                }
            }
            const AdaptedTriangleMesh sourceMesh =
                AdaptSceneModelToTriangleMesh(sourceModel);
            ThrowIfCancelled(cancelToken);

            MeshRepairCleanupRequest cleanupRequest;
            cleanupRequest.mesh = &sourceMesh;
            cleanupRequest.input.sourcePath =
                request.source_model_path.generic_string();
            cleanupRequest.input.inputFormat = "obj";
            cleanupRequest.options.enabled = true;
            cleanupRequest.options.mode = "repair_then_strict";
            cleanupRequest.options.validatePostRepairEvidence = true;
            cleanupRequest.options.analyzeCompleteSelfIntersections = true;
            cleanupRequest.sourceHash = SourceDigest(request.source_model_path);
            MeshRepairCleanupResult cleanup =
                ExecuteMeshRepairCleanup(cleanupRequest);
            if (cleanup.evidence.status != MeshRepairStatus::StrictPassNoRepair
                && cleanup.evidence.status != MeshRepairStatus::RepairedStrictPass)
            {
                throw TopologyBlockedError(
                    kTopologyBlockedCode,
                    "mesh is not eligible for conservative repair");
            }
            ThrowIfCancelled(cancelToken);

            stagingDirectory =
                request.repaired_model_path.parent_path()
                / ("." + request.job_id + ".repair.staging");
            if (std::filesystem::exists(stagingDirectory))
            {
                throw std::runtime_error("repair staging directory already exists");
            }
            std::filesystem::create_directories(stagingDirectory);
            DeterministicObjAssetWriteRequest writeRequest;
            writeRequest.mesh = &cleanup.candidate;
            writeRequest.outputObjPath =
                stagingDirectory / request.repaired_model_path.filename();
            writeRequest.cancellationRequested = [&cancelToken]()
            {
                return cancelToken.IsCancelRequested();
            };
            DeterministicObjAssetWriteResult written;
            try
            {
                written = WriteDeterministicObjAsset(writeRequest);
            }
            catch (const std::exception&)
            {
                if (cancelToken.IsCancelRequested())
                {
                    throw CancelledError();
                }
                throw std::filesystem::filesystem_error(
                    "repair asset staging failed",
                    writeRequest.outputObjPath,
                    std::make_error_code(std::errc::io_error));
            }
            writtenObj = written.objPath;
            writtenMtl = written.mtlPath;
            writtenTextures = written.texturePaths;

            StrictRepairAssetRecheckRequest recheckRequest;
            recheckRequest.stagedObjPath = written.objPath;
            recheckRequest.profileConfigPath = request.profile_config_path;
            recheckRequest.profileHash = request.profile_hash;
            recheckRequest.expectedMesh = &cleanup.candidate;
            recheckRequest.cancellationRequested = [&cancelToken]()
            {
                return cancelToken.IsCancelRequested();
            };
            StrictRepairAssetRecheckResult recheck;
            try
            {
                recheck = RecheckStrictRepairAsset(recheckRequest);
            }
            catch (const std::exception&)
            {
                if (cancelToken.IsCancelRequested())
                {
                    throw CancelledError();
                }
                throw TopologyBlockedError(
                    kTopologyPostCode,
                    "repaired asset reimport/strict recheck failed");
            }
            if (!recheck.strictComplete || !recheck.strictPass
                || !recheck.attributesPreserved)
            {
                throw TopologyBlockedError(
                    kTopologyPostCode,
                    "repaired asset did not pass complete strict recheck");
            }
            ThrowIfCancelled(cancelToken);

            api::RepairResult result;
            result.repaired_model_path = written.objPath;
            result.source_hash = SourceDigest(request.source_model_path);
            result.repaired_hash = AssetDigest(
                written.objPath, written.mtlPath, written.texturePaths);
            result.preflight_before = TopologyJson(cleanup.evidence.preRepair);
            result.preflight_after = TopologyJson(recheck.diagnostics);
            Json::Object evidence =
                BuildMeshRepairReport(cleanup.evidence).as_object();
            evidence.emplace("asset", Json::object({
                {"assetWritten", true},
                {"assetReimported", recheck.assetReimported},
                {"strictComplete", recheck.strictComplete},
                {"strictPass", recheck.strictPass},
                {"attributesPreserved", recheck.attributesPreserved},
                {"outputFormat", "obj"},
                {"publicationState", "job_staging"}}));
            result.evidence = Json(std::move(evidence));
            result.elapsed_ms = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    Clock::now() - start).count());
            return api::ApiResult<api::RepairResult>::Success(
                std::move(result));
        }
        catch (const CancelledError& error)
        {
            RemoveOutputBundle(
                writtenObj, writtenMtl, writtenTextures, stagingDirectory);
            return api::ApiResult<api::RepairResult>::Failure(
                MakeError(kCancelledCode, error.what()));
        }
        catch (const TopologyBlockedError& error)
        {
            RemoveOutputBundle(
                writtenObj, writtenMtl, writtenTextures, stagingDirectory);
            return api::ApiResult<api::RepairResult>::Failure(
                MakeError(error.Code(), error.what()));
        }
        catch (const std::invalid_argument& error)
        {
            RemoveOutputBundle(
                writtenObj, writtenMtl, writtenTextures, stagingDirectory);
            return api::ApiResult<api::RepairResult>::Failure(
                MakeError(kInputCode, "repair request is invalid", error.what()));
        }
        catch (const std::filesystem::filesystem_error& error)
        {
            RemoveOutputBundle(
                writtenObj, writtenMtl, writtenTextures, stagingDirectory);
            return api::ApiResult<api::RepairResult>::Failure(
                MakeError(kOutputCode, "repair asset write failed", error.what()));
        }
        catch (const std::bad_alloc&)
        {
            RemoveOutputBundle(
                writtenObj, writtenMtl, writtenTextures, stagingDirectory);
            return api::ApiResult<api::RepairResult>::Failure(
                MakeError("PM-SLICER-RESOURCE-0040", "repair ran out of memory"));
        }
        catch (const std::exception& error)
        {
            RemoveOutputBundle(
                writtenObj, writtenMtl, writtenTextures, stagingDirectory);
            return api::ApiResult<api::RepairResult>::Failure(
                MakeError(kInternalCode, "unexpected repair failure", error.what()));
        }
        catch (...)
        {
            RemoveOutputBundle(
                writtenObj, writtenMtl, writtenTextures, stagingDirectory);
            return api::ApiResult<api::RepairResult>::Failure(
                MakeError(kInternalCode, "unknown repair failure"));
        }
    }
};

}  // namespace

std::unique_ptr<api::RepairFacade> CreateProductionRepairFacade()
{
    return std::make_unique<ProductionRepairFacade>();
}

}  // namespace slicer_core::engine
