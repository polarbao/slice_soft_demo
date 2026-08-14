#include "slicer_core/api/implementation/ModelFacadeImplementation.h"

#include "slicer_core/model.h"
#include "slicer_core/scene/SceneResourceIdentity.h"
#include "slicer_core/system/Sha256.h"

#include <bit>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

// 文件职责：把既有模型加载与快速预检能力封装为无 Qt Facade；
// 边界：句柄、取消和错误必须在 Facade 内收敛，异常不得跨 API 边界。
namespace slicer_core::api::implementation
{
namespace
{

constexpr const char* kInputError{"PM-SLICER-INPUT-0001"};
constexpr const char* kModelParseError{"PM-SLICER-INPUT-0002"};
constexpr const char* kResourceError{"PM-SLICER-RESOURCE-0040"};
constexpr const char* kCancelledError{"PM-SLICER-CANCELLED-0070"};
constexpr const char* kInternalError{"PM-SLICER-INTERNAL-0099"};

ApiError MakeError(
    const char* code,
    std::string message,
    std::string detail = {})
{
    return ApiError{code, std::move(message), std::move(detail)};
}

std::string ReadFileBytes(const std::filesystem::path& path)
{
    std::ifstream input{path, std::ios::binary};
    if (!input)
    {
        throw std::runtime_error(
            "failed to read model source: " + path.generic_string());
    }
    return {
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()};
}

void AppendUint64(std::string& payload, const std::uint64_t value)
{
    for (int shift = 56; shift >= 0; shift -= 8)
    {
        payload.push_back(static_cast<char>((value >> shift) & 0xffU));
    }
}

void AppendDouble(std::string& payload, const double value)
{
    AppendUint64(payload, std::bit_cast<std::uint64_t>(value));
}

void AppendPoint(std::string& payload, const Vec3& point)
{
    AppendDouble(payload, point.x);
    AppendDouble(payload, point.y);
    AppendDouble(payload, point.z);
}

std::string ComputeMeshIdentity(const ModelReport& report)
{
    std::string payload;
    payload.reserve(report.triangles.size() * 72U + 128U);
    payload.append("slicesoft.mesh.v1");
    payload.push_back('\0');
    payload.append(report.format);
    payload.push_back('\0');
    AppendUint64(
        payload,
        static_cast<std::uint64_t>(report.triangles.size()));
    for (const Triangle& triangle : report.triangles)
    {
        AppendPoint(payload, triangle.a);
        AppendPoint(payload, triangle.b);
        AppendPoint(payload, triangle.c);
    }
    return ComputeSha256(payload);
}

Bounds3d MakeBounds(const BoundingBox& bounds)
{
    Bounds3d result;
    result.min_mm = {bounds.min.x, bounds.min.y, bounds.min.z};
    result.max_mm = {bounds.max.x, bounds.max.y, bounds.max.z};
    return result;
}

ModelMaterial MakeMaterial(const MaterialInfo& material)
{
    constexpr double kByteMaximum{255.0};
    ModelMaterial result;
    result.name = material.name;
    result.diffuse_rgb = {
        static_cast<double>(material.diffuse_rgb.at(0U)) / kByteMaximum,
        static_cast<double>(material.diffuse_rgb.at(1U)) / kByteMaximum,
        static_cast<double>(material.diffuse_rgb.at(2U)) / kByteMaximum};
    if (material.has_texture)
    {
        result.texture_path = material.diffuse_texture_path;
    }
    return result;
}

ModelMetadata MakeMetadata(
    const ModelId modelId,
    const ModelReport& report,
    const ModelImportRequest& request,
    std::string sourceDigest)
{
    ModelMetadata metadata;
    metadata.model_id = modelId;
    metadata.source_path = report.model_path;
    metadata.format = report.format;
    metadata.vertex_count = report.vertex_count;
    metadata.triangle_count = report.triangle_count;
    metadata.has_uv = report.faces_with_uv > 0U;
    metadata.has_normals = report.has_normals;
    metadata.local_bounds_mm = MakeBounds(report.bbox_mm);
    metadata.source_digest = std::move(sourceDigest);
    metadata.mesh_identity = ComputeMeshIdentity(report);
    metadata.appearance_identity = ComputeSceneResourceHash(report);

    for (const MaterialInfo& material : report.material_infos)
    {
        metadata.has_texture = metadata.has_texture || material.has_texture;
        if (request.extract_materials)
        {
            metadata.materials.push_back(MakeMaterial(material));
        }
    }
    return metadata;
}

class ModelFacadeService final : public ModelFacade
{
public:
    ApiResult<ModelMetadata> Import(
        const ModelImportRequest& request,
        const ICancelToken& cancelToken) noexcept override
    {
        try
        {
            if (cancelToken.IsCancelRequested())
            {
                return ApiResult<ModelMetadata>::Failure(MakeError(
                    kCancelledError,
                    "model import was cancelled"));
            }
            if (request.model_path.empty())
            {
                return ApiResult<ModelMetadata>::Failure(MakeError(
                    kInputError,
                    "model path is empty"));
            }

            const std::filesystem::path sourcePath =
                std::filesystem::absolute(request.model_path)
                    .lexically_normal();
            if (!std::filesystem::is_regular_file(sourcePath))
            {
                return ApiResult<ModelMetadata>::Failure(MakeError(
                    kInputError,
                    "model file was not found",
                    sourcePath.generic_string()));
            }

            ModelLoadConfig config;
            config.input.model_path = sourcePath;
            const ModelReport report = load_model_report(
                config,
                sourcePath.parent_path());
            if (cancelToken.IsCancelRequested())
            {
                return ApiResult<ModelMetadata>::Failure(MakeError(
                    kCancelledError,
                    "model import was cancelled"));
            }

            const std::string sourceDigest =
                ComputeSha256(ReadFileBytes(sourcePath));
            if (cancelToken.IsCancelRequested())
            {
                return ApiResult<ModelMetadata>::Failure(MakeError(
                    kCancelledError,
                    "model import was cancelled"));
            }

            ModelId modelId{0U};
            {
                std::scoped_lock lock{m_mutex};
                if (m_nextModelId == 0U)
                {
                    return ApiResult<ModelMetadata>::Failure(MakeError(
                        kResourceError,
                        "model handle space is exhausted"));
                }
                modelId = m_nextModelId++;
            }

            ModelMetadata metadata = MakeMetadata(
                modelId,
                report,
                request,
                sourceDigest);
            if (cancelToken.IsCancelRequested())
            {
                return ApiResult<ModelMetadata>::Failure(MakeError(
                    kCancelledError,
                    "model import was cancelled"));
            }
            {
                std::scoped_lock lock{m_mutex};
                m_models.emplace(modelId, metadata);
            }
            return ApiResult<ModelMetadata>::Success(std::move(metadata));
        }
        catch (const std::bad_alloc& error)
        {
            return ApiResult<ModelMetadata>::Failure(MakeError(
                kResourceError,
                "model import exhausted available memory",
                error.what()));
        }
        catch (const std::filesystem::filesystem_error& error)
        {
            return ApiResult<ModelMetadata>::Failure(MakeError(
                kInputError,
                "model source could not be accessed",
                error.what()));
        }
        catch (const std::runtime_error& error)
        {
            return ApiResult<ModelMetadata>::Failure(MakeError(
                kModelParseError,
                "model import failed",
                error.what()));
        }
        catch (const std::exception& error)
        {
            return ApiResult<ModelMetadata>::Failure(MakeError(
                kInternalError,
                "unexpected model import failure",
                error.what()));
        }
        catch (...)
        {
            return ApiResult<ModelMetadata>::Failure(MakeError(
                kInternalError,
                "unknown model import failure"));
        }
    }

    ApiResult<ModelMetadata> GetMetadata(
        const ModelId modelId) const noexcept override
    {
        try
        {
            std::scoped_lock lock{m_mutex};
            const auto found = m_models.find(modelId);
            if (found == m_models.end())
            {
                return ApiResult<ModelMetadata>::Failure(MakeError(
                    kInputError,
                    "model handle is not active",
                    std::to_string(modelId)));
            }
            return ApiResult<ModelMetadata>::Success(found->second);
        }
        catch (const std::exception& error)
        {
            return ApiResult<ModelMetadata>::Failure(MakeError(
                kInternalError,
                "failed to read model metadata",
                error.what()));
        }
        catch (...)
        {
            return ApiResult<ModelMetadata>::Failure(MakeError(
                kInternalError,
                "unknown model metadata failure"));
        }
    }

    ApiResult<void> Release(const ModelId modelId) noexcept override
    {
        try
        {
            std::scoped_lock lock{m_mutex};
            if (m_models.erase(modelId) == 0U)
            {
                return ApiResult<void>::Failure(MakeError(
                    kInputError,
                    "model handle is not active",
                    std::to_string(modelId)));
            }
            return ApiResult<void>::Success();
        }
        catch (const std::exception& error)
        {
            return ApiResult<void>::Failure(MakeError(
                kInternalError,
                "failed to release model handle",
                error.what()));
        }
        catch (...)
        {
            return ApiResult<void>::Failure(MakeError(
                kInternalError,
                "unknown model release failure"));
        }
    }

private:
    mutable std::mutex m_mutex;
    ModelId m_nextModelId{1U};
    std::unordered_map<ModelId, ModelMetadata> m_models;
};

}  // namespace

std::unique_ptr<ModelFacade> CreateModelFacade()
{
    return std::make_unique<ModelFacadeService>();
}

}  // namespace slicer_core::api::implementation
