#include "slicer_worker/slice/WorkerSliceRequestMaterializer.h"

#include "slicer_core/api/ProfileIdentity.h"
#include "slicer_core/config.h"
#include "slicer_core/config/SlicePipelineConfig.h"
#include "slicer_core/scene/MultiModelScene.h"
#include "slicer_core/scene/SceneEffectiveConfig.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <system_error>
#include <utility>

namespace slicesoft::worker
{
namespace
{

constexpr const char* kCancelledCode{"PM-SLICER-CANCELLED-0070"};
constexpr const char* kContractCode{"PM-SLICER-CONTRACT-0060"};
constexpr const char* kInputCode{"PM-SLICER-INPUT-0001"};
constexpr const char* kLayoutCode{"PM-SLICER-LAYOUT-0022"};
constexpr const char* kOutputCode{"PM-SLICER-OUTPUT-0050"};
constexpr const char* kProfileCode{"PM-SLICER-PROFILE-0030"};
constexpr const char* kProfileMismatchCode{"PM-SLICER-PROFILE-0031"};
constexpr const char* kLegacySamplingStrategy{"legacy_center_sample"};
constexpr const char* kApprovedSamplingCandidate{
    "layer_slab_supersample_2x2_at_least_two_candidate"};

struct MaterializedPaths
{
    std::filesystem::path scenesnapshot;
    std::filesystem::path profile;
    std::filesystem::path sceneconfig;
};

[[noreturn]] void Fail(
    const std::string& code,
    const std::string& message)
{
    throw WorkerSliceRequestMaterializationError(code, message);
}

void CheckCancellation(
    const slicer_core::api::ICancelToken& cancelToken)
{
    if (cancelToken.IsCancelRequested())
    {
        Fail(kCancelledCode, "slice request materialization was cancelled");
    }
}

bool IsLowercaseSha256(const std::string& value)
{
    if (value.size() != 71U || value.rfind("sha256:", 0U) != 0U)
    {
        return false;
    }
    return std::all_of(
        value.begin() + 7,
        value.end(),
        [](const char character)
        {
            return (character >= '0' && character <= '9')
                || (character >= 'a' && character <= 'f');
        });
}

bool IsNormalizedAbsolute(const std::filesystem::path& path)
{
    return path.is_absolute() && path == path.lexically_normal();
}

bool IsRegularFile(const std::filesystem::path& path)
{
    std::error_code error;
    const bool regular = std::filesystem::is_regular_file(path, error);
    return regular && !error;
}

bool IsDirectory(const std::filesystem::path& path)
{
    std::error_code error;
    const bool directory = std::filesystem::is_directory(path, error);
    return directory && !error;
}

void ValidateSceneResourcePaths(const slicer_core::MultiModelScene& scene)
{
    for (const slicer_core::ResourceScope& scope : scene.resourcescopes)
    {
        if (!IsNormalizedAbsolute(scope.rootpath))
        {
            Fail(kInputCode, "scene resource scope root must be normalized and absolute");
        }
        switch (scope.kind)
        {
        case slicer_core::ResourceScopeKind::ObjDirectory:
            if (!IsDirectory(scope.rootpath))
            {
                Fail(kInputCode, "OBJ resource scope root is not a readable directory");
            }
            break;
        case slicer_core::ResourceScopeKind::ThreeMfPackage:
            if (!IsNormalizedAbsolute(scope.packagepath)
                || !IsRegularFile(scope.packagepath))
            {
                Fail(kInputCode, "3MF resource package is not a readable absolute file");
            }
            break;
        case slicer_core::ResourceScopeKind::StlFile:
            if (!IsRegularFile(scope.rootpath))
            {
                Fail(kInputCode, "STL resource scope is not a readable file");
            }
            break;
        }
    }

    for (const slicer_core::ModelSource& model : scene.models)
    {
        if (!IsNormalizedAbsolute(model.sourcepath)
            || !IsRegularFile(model.sourcepath))
        {
            Fail(kInputCode, "scene model source is not a readable normalized absolute file");
        }
    }
}

std::string ReadStringField(
    const slicer_core::Json& object,
    const std::string& field,
    const std::string& code)
{
    try
    {
        if (!object.contains(field) || !object.at(field).is_string())
        {
            Fail(code, field + " must be a string");
        }
        const std::string value = object.at(field).as_string();
        if (value.empty())
        {
            Fail(code, field + " must not be empty");
        }
        return value;
    }
    catch (const WorkerSliceRequestMaterializationError&)
    {
        throw;
    }
    catch (const std::exception&)
    {
        Fail(code, field + " is invalid");
    }
}

void EnsureFreshMaterialization(const MaterializedPaths& paths)
{
    for (const std::filesystem::path& path :
         {paths.scenesnapshot, paths.profile, paths.sceneconfig})
    {
        std::error_code error;
        if (std::filesystem::exists(path, error) || error)
        {
            Fail(kOutputCode, "job directory already contains materialized slice inputs");
        }
    }
}

void CleanupMaterialization(const MaterializedPaths& paths) noexcept
{
    std::error_code error;
    for (const std::filesystem::path& path :
         {paths.scenesnapshot, paths.profile, paths.sceneconfig})
    {
        std::filesystem::remove(path, error);
        error.clear();
        std::filesystem::remove(path.string() + ".tmp", error);
        error.clear();
        std::filesystem::remove(path.string() + ".backup", error);
        error.clear();
    }
}

void WriteJsonAtomically(
    const std::filesystem::path& path,
    const slicer_core::Json& document,
    const slicer_core::api::ICancelToken& cancelToken)
{
    const std::filesystem::path temporary = path.string() + ".tmp";
    std::error_code error;
    std::filesystem::remove(temporary, error);
    error.clear();

    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output)
        {
            Fail(kOutputCode, "failed to open materialized JSON staging file");
        }
        output << document.dump(2);
        output.flush();
        if (!output)
        {
            output.close();
            std::filesystem::remove(temporary, error);
            Fail(kOutputCode, "failed to write materialized JSON staging file");
        }
    }

    CheckCancellation(cancelToken);
    std::filesystem::rename(temporary, path, error);
    if (error)
    {
        std::filesystem::remove(temporary, error);
        Fail(kOutputCode, "failed to publish materialized JSON atomically");
    }
}

std::string UtcNow()
{
    const std::time_t now = std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now());
    std::tm utc{};
#if defined(_WIN32)
    gmtime_s(&utc, &now);
#else
    gmtime_r(&now, &utc);
#endif
    std::ostringstream output;
    output << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return output.str();
}

}  // namespace

WorkerSliceRequestMaterializationError::WorkerSliceRequestMaterializationError(
    std::string code,
    const std::string& message)
    : std::runtime_error(message),
      m_code(std::move(code))
{
}

const std::string& WorkerSliceRequestMaterializationError::Code() const noexcept
{
    return m_code;
}

WorkerSliceMaterialization::WorkerSliceMaterialization(
    std::filesystem::path sceneSnapshotPath,
    std::filesystem::path profilePath,
    std::filesystem::path sceneConfigPath,
    std::filesystem::path packageDirectory,
    std::string sceneHash,
    std::string profileHash,
    std::string profileVersion,
    const std::uint64_t sceneRevision,
    std::string targetMode,
    const int dpiX,
    const int dpiY,
    const bool productionAdmissionCommitted)
    : m_sceneSnapshotPath(std::move(sceneSnapshotPath)),
      m_profilePath(std::move(profilePath)),
      m_sceneConfigPath(std::move(sceneConfigPath)),
      m_packageDirectory(std::move(packageDirectory)),
      m_sceneHash(std::move(sceneHash)),
      m_profileHash(std::move(profileHash)),
      m_profileVersion(std::move(profileVersion)),
      m_sceneRevision(sceneRevision),
      m_targetMode(std::move(targetMode)),
      m_dpiX(dpiX),
      m_dpiY(dpiY),
      m_productionAdmissionCommitted(productionAdmissionCommitted)
{
}

const std::filesystem::path& WorkerSliceMaterialization::SceneSnapshotPath() const noexcept
{
    return m_sceneSnapshotPath;
}

const std::filesystem::path& WorkerSliceMaterialization::ProfilePath() const noexcept
{
    return m_profilePath;
}

const std::filesystem::path& WorkerSliceMaterialization::SceneConfigPath() const noexcept
{
    return m_sceneConfigPath;
}

const std::filesystem::path& WorkerSliceMaterialization::PackageDirectory() const noexcept
{
    return m_packageDirectory;
}

const std::string& WorkerSliceMaterialization::SceneHash() const noexcept
{
    return m_sceneHash;
}

const std::string& WorkerSliceMaterialization::ProfileHash() const noexcept
{
    return m_profileHash;
}

const std::string& WorkerSliceMaterialization::ProfileVersion() const noexcept
{
    return m_profileVersion;
}

std::uint64_t WorkerSliceMaterialization::SceneRevision() const noexcept
{
    return m_sceneRevision;
}

const std::string& WorkerSliceMaterialization::TargetMode() const noexcept
{
    return m_targetMode;
}

int WorkerSliceMaterialization::DpiX() const noexcept
{
    return m_dpiX;
}

int WorkerSliceMaterialization::DpiY() const noexcept
{
    return m_dpiY;
}

bool WorkerSliceMaterialization::ProductionAdmissionCommitted() const noexcept
{
    return m_productionAdmissionCommitted;
}

std::string WorkerSliceRequestMaterializer::ComputeProfileHash(
    const slicer_core::Json& profile)
{
    return slicer_core::api::ComputeProfileDocumentHash(profile);
}

WorkerSliceMaterialization WorkerSliceRequestMaterializer::Materialize(
    const WorkerRequestEnvelope& request,
    const slicer_core::api::ICancelToken& cancelToken)
{
    if (request.Identity().Capability() != "slice.rgbwsv"
        || !request.HasScene()
        || !request.HasProfile()
        || !request.HasOutput()
        || !request.SceneHash().has_value())
    {
        Fail(kContractCode, "materializer requires a complete slice.rgbwsv request");
    }
    CheckCancellation(cancelToken);

    const MaterializedPaths paths{
        request.Identity().JobDirectory() / "scene.snapshot.json",
        request.Identity().JobDirectory() / "profile.effective.json",
        request.Identity().JobDirectory() / "scene_config.effective.json"};
    EnsureFreshMaterialization(paths);

    bool ownsMaterialization{true};
    try
    {
        const slicer_core::MultiModelSceneDecodeResult decoded =
            slicer_core::DeserializeMultiModelScene(request.Scene());
        if (!decoded.IsValid())
        {
            Fail(kLayoutCode, "scene document cannot be decoded");
        }
        const slicer_core::SceneValidationResult validation =
            slicer_core::ValidateMultiModelScene(
                decoded.scene,
                slicer_core::SceneValidationPurpose::Production);
        if (!validation.IsValid())
        {
            Fail(kLayoutCode, validation.errors.front().message);
        }
        ValidateSceneResourcePaths(decoded.scene);

        const std::string sceneHash =
            slicer_core::ComputeMultiModelSceneHash(decoded.scene);
        const std::string externalSceneHash = "sha256:" + sceneHash;
        if (!IsLowercaseSha256(*request.SceneHash())
            || *request.SceneHash() != externalSceneHash)
        {
            Fail(
                kLayoutCode,
                "request sceneHash does not match the committed scene: requested="
                    + *request.SceneHash() + ", actual=" + externalSceneHash);
        }

        const std::string profileVersion = ReadStringField(
            request.Profile(), "profileVersion", kProfileCode);
        const std::string profileHash = ReadStringField(
            request.Profile(), "profileHash", kProfileCode);
        if (!IsLowercaseSha256(profileHash)
            || profileHash != ComputeProfileHash(request.Profile()))
        {
            Fail(kProfileCode, "request profileHash does not match the Profile document");
        }

        const std::string outputContract = ReadStringField(
            request.Output(), "contract", kContractCode);
        if (outputContract != "p0.rgbwsv.2")
        {
            Fail(kContractCode, "slice output contract must be p0.rgbwsv.2");
        }
        const std::filesystem::path packageDirectory =
            std::filesystem::path(ReadStringField(
                request.Output(), "packageDir", kOutputCode));
        if (!IsNormalizedAbsolute(packageDirectory))
        {
            Fail(kOutputCode, "packageDir must be normalized and absolute");
        }
        std::error_code packageError;
        const bool packageExists =
            std::filesystem::exists(packageDirectory, packageError);
        if (packageError)
        {
            Fail(kOutputCode, "packageDir resolves to an invalid output target");
        }
        if (packageExists
            && !std::filesystem::is_directory(
                packageDirectory, packageError))
        {
            Fail(kOutputCode, "packageDir resolves to an invalid output target");
        }

        WriteJsonAtomically(
            paths.scenesnapshot,
            slicer_core::SerializeMultiModelScene(decoded.scene),
            cancelToken);
        WriteJsonAtomically(paths.profile, request.Profile(), cancelToken);

        slicer_core::SliceConfig profile;
        try
        {
            profile = slicer_core::load_slice_config(paths.profile);
        }
        catch (const std::exception& error)
        {
            Fail(kProfileCode, std::string("Profile validation failed: ") + error.what());
        }
        if (profile.geometry_sampling.strategy != kLegacySamplingStrategy
            && profile.geometry_sampling.strategy
                != kApprovedSamplingCandidate)
        {
            Fail(
                kProfileCode,
                "geometrySampling.strategy is not approved for production integration");
        }
        if (!profile.material_process_profile.enabled
            || profile.material_process_profile.name
                != decoded.scene.resolvedprofileid)
        {
            Fail(kProfileMismatchCode, "scene and Profile material identities do not match");
        }
        if (!IsNormalizedAbsolute(profile.output.package_dir)
            || profile.output.package_dir.lexically_normal()
                != packageDirectory)
        {
            Fail(kProfileMismatchCode, "Profile and request package directories do not match");
        }

        CheckCancellation(cancelToken);
        slicer_core::SceneEffectiveConfigRequest effectiveRequest;
        effectiveRequest.scene = decoded.scene;
        effectiveRequest.sourcescenepath = paths.scenesnapshot;
        effectiveRequest.generatedconfigpath = paths.sceneconfig;
        effectiveRequest.sourceprofileid = decoded.scene.resolvedprofileid;
        effectiveRequest.sourceprofileconfigpath = paths.profile;
        effectiveRequest.outputpackagedir = packageDirectory;
        effectiveRequest.generatedatutc = UtcNow();
        effectiveRequest.dpix = profile.output.dpi_x;
        effectiveRequest.dpiy = profile.output.dpi_y;
        effectiveRequest.layerheightmm = profile.output.layer_thickness_mm;
        effectiveRequest.slicepipelinemode =
            slicer_core::SlicePipelineModeName(profile.slice_pipeline.mode);
        effectiveRequest.geometrysamplingstrategy =
            profile.geometry_sampling.strategy;
        effectiveRequest.production = true;
        effectiveRequest.cancelled = cancelToken.IsCancelRequested();
        const slicer_core::SceneEffectiveConfigResult effective =
            slicer_core::WriteSceneEffectiveConfig(effectiveRequest);
        if (!effective.IsValid())
        {
            if (effective.error.has_value()
                && effective.error->code
                    == slicer_core::SceneValidationErrorCode::EffectiveConfigCancelled)
            {
                Fail(kCancelledCode, effective.error->message);
            }
            Fail(kOutputCode, "failed to publish scene effective config");
        }
        CheckCancellation(cancelToken);

        ownsMaterialization = false;
        return WorkerSliceMaterialization(
            paths.scenesnapshot,
            paths.profile,
            paths.sceneconfig,
            packageDirectory,
            sceneHash,
            profileHash,
            profileVersion,
            decoded.scene.scenerevision,
            slicer_core::SlicePipelineModeName(profile.slice_pipeline.mode),
            profile.output.dpi_x,
            profile.output.dpi_y,
            std::all_of(
                decoded.scene.instances.begin(),
                decoded.scene.instances.end(),
                [](const slicer_core::SceneModelInstance& item)
                {
                    return !item.instance.visible
                        || item.admissionstatus
                            == slicer_core::SceneInstanceAdmissionStatus::Admitted;
                }));
    }
    catch (const WorkerSliceRequestMaterializationError&)
    {
        if (ownsMaterialization)
        {
            CleanupMaterialization(paths);
        }
        throw;
    }
    catch (const std::exception& error)
    {
        if (ownsMaterialization)
        {
            CleanupMaterialization(paths);
        }
        throw WorkerSliceRequestMaterializationError(
            "PM-SLICER-INTERNAL-0099",
            std::string("unexpected slice request materialization failure: ")
                + error.what());
    }
}

}  // namespace slicesoft::worker
