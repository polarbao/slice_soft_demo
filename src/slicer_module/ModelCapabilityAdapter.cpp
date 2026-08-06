#include "slicer_module/ModelCapabilityAdapter.h"

#include "slicer_core/api/implementation/ModelFacadeImplementation.h"
#include "slicer_core/geometry/SceneModelTriangleMeshAdapter.h"

#include <algorithm>
#include <cctype>

namespace slicesoft::module
{

bool NeverCancelToken::IsCancelRequested() const noexcept
{
    return false;
}

ModelCapabilityAdapter::ModelCapabilityAdapter()
    : m_facade(slicer_core::api::implementation::CreateModelFacade())
{
}

ModelCapabilityAdapter::~ModelCapabilityAdapter() = default;

slicer_core::Json ModelCapabilityAdapter::Execute(
    const std::string& capability,
    const slicer_core::Json& request)
{
    if (capability == "model.import")
    {
        return Import(request);
    }
    if (capability == "model.get_metadata")
    {
        return GetMetadata(request);
    }
    if (capability == "model.release")
    {
        return Release(request);
    }
    if (capability == "geometry.preflight")
    {
        return RunFastPreflight(request);
    }
    return MakeFailure(
        "PM-SLICER-INTERNAL-0099",
        "model capability adapter received an unsupported capability",
        capability);
}

std::shared_ptr<const ImportedModelResource> ModelCapabilityAdapter::Find(
    const slicer_core::api::ModelId modelId) const
{
    std::scoped_lock lock{m_mutex};
    const auto entry = m_resources.find(modelId);
    return entry == m_resources.end() ? nullptr : entry->second;
}

std::shared_ptr<const ImportedModelResource> ModelCapabilityAdapter::FindByPath(
    const std::filesystem::path& path) const
{
    const std::string key = NormalizePath(path);
    std::scoped_lock lock{m_mutex};
    const auto identity = m_pathToModel.find(key);
    if (identity == m_pathToModel.end())
    {
        return nullptr;
    }
    const auto resource = m_resources.find(identity->second);
    return resource == m_resources.end() ? nullptr : resource->second;
}

std::string ModelCapabilityAdapter::NormalizePath(
    const std::filesystem::path& path)
{
    std::string value = std::filesystem::absolute(path)
        .lexically_normal()
        .generic_string();
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](const unsigned char character)
        {
            return static_cast<char>(std::tolower(character));
        });
    return value;
}

slicer_core::Json ModelCapabilityAdapter::MakeMetadata(
    const slicer_core::api::ModelMetadata& metadata)
{
    slicer_core::Json::Array materials;
    materials.reserve(metadata.materials.size());
    for (const slicer_core::api::ModelMaterial& material : metadata.materials)
    {
        materials.emplace_back(slicer_core::Json::object({
            {"name", material.name},
            {"diffuseRgb", MakeNumberArray(material.diffuse_rgb)},
            {"texturePath", material.texture_path.empty()
                ? slicer_core::Json{nullptr}
                : slicer_core::Json{material.texture_path.generic_string()}}}));
    }
    return MakeSuccess({
        {"modelId", std::to_string(metadata.model_id)},
        {"triangleCount", static_cast<std::uint64_t>(metadata.triangle_count)},
        {"vertexCount", static_cast<std::uint64_t>(metadata.vertex_count)},
        {"hasUV", metadata.has_uv},
        {"hasNormals", metadata.has_normals},
        {"materials", slicer_core::Json{std::move(materials)}},
        {"bboxMm", MakeBounds(metadata.local_bounds_mm)},
        {"units", "mm"},
        {"sourceDigest", metadata.source_digest}});
}

slicer_core::Json ModelCapabilityAdapter::Import(
    const slicer_core::Json& request)
{
    slicer_core::api::ModelImportRequest importRequest;
    importRequest.model_path = RequireString(request, "modelPath");
    const slicer_core::Json& options = RequireObject(request, "options");
    importRequest.compute_bbox = RequireBoolean(options, "computeBBox");
    importRequest.extract_materials = RequireBoolean(options, "extractMaterials");

    NeverCancelToken cancelToken;
    const auto result = m_facade->Import(importRequest, cancelToken);
    if (!result.IsOk())
    {
        return MakeFailure(*result.Error());
    }

    try
    {
        slicer_core::ModelLoadConfig config;
        config.input.model_path = result.Value()->source_path;
        auto sceneModel = std::make_shared<const slicer_core::SceneModel>(
            slicer_core::load_model_report(
                config,
                result.Value()->source_path.parent_path()));
        auto resource = std::make_shared<ImportedModelResource>();
        resource->metadata = *result.Value();
        resource->scenemodel = std::move(sceneModel);
        std::scoped_lock lock{m_mutex};
        m_pathToModel[NormalizePath(resource->metadata.source_path)] =
            resource->metadata.model_id;
        m_resources[resource->metadata.model_id] = std::move(resource);
    }
    catch (const std::exception& error)
    {
        (void)m_facade->Release(result.Value()->model_id);
        return MakeFailure(
            "PM-SLICER-INPUT-0002",
            "model scene resource could not be retained",
            error.what());
    }
    return MakeMetadata(*result.Value());
}

slicer_core::Json ModelCapabilityAdapter::GetMetadata(
    const slicer_core::Json& request) const
{
    const auto result = m_facade->GetMetadata(
        ParseModelId(RequireString(request, "modelId")));
    return result.IsOk()
        ? MakeMetadata(*result.Value())
        : MakeFailure(*result.Error());
}

slicer_core::Json ModelCapabilityAdapter::Release(
    const slicer_core::Json& request)
{
    const auto modelId = ParseModelId(RequireString(request, "modelId"));
    const auto result = m_facade->Release(modelId);
    if (!result.IsOk())
    {
        return MakeFailure(*result.Error());
    }
    std::scoped_lock lock{m_mutex};
    const auto resource = m_resources.find(modelId);
    if (resource != m_resources.end())
    {
        m_pathToModel.erase(NormalizePath(resource->second->metadata.source_path));
        m_resources.erase(resource);
    }
    return MakeSuccess({
        {"modelId", std::to_string(modelId)},
        {"released", true}});
}

std::shared_ptr<const slicer_core::SceneModel>
ModelCapabilityAdapter::ResolvePreflightModel(
    const slicer_core::Json& request) const
{
    if (request.contains("modelId"))
    {
        const auto resource = Find(ParseModelId(RequireString(request, "modelId")));
        if (!resource)
        {
            throw CapabilityRequestError("modelId is not imported");
        }
        return resource->scenemodel;
    }
    const std::filesystem::path path = RequireString(request, "modelPath");
    slicer_core::ModelLoadConfig config;
    config.input.model_path = path;
    return std::make_shared<const slicer_core::SceneModel>(
        slicer_core::load_model_report(config, path.parent_path()));
}

slicer_core::Json ModelCapabilityAdapter::RunFastPreflight(
    const slicer_core::Json& request) const
{
    if (RequireString(request, "mode") != "fast")
    {
        throw CapabilityRequestError(
            "only geometry.preflight mode=fast is available synchronously");
    }
    try
    {
        const auto model = ResolvePreflightModel(request);
        const slicer_core::AdaptedTriangleMesh mesh =
            slicer_core::AdaptSceneModelToTriangleMesh(*model);
        const auto& topology = mesh.topology;
        slicer_core::Json::Array issues;
        std::string admission{"passed"};
        if (topology.non_manifold_edges > 0U)
        {
            admission = "manual_repair_required";
            issues.emplace_back(MakeIssue(
                "PM-SLICER-TOPOLOGY-0011",
                "error",
                topology.non_manifold_edges,
                "non-manifold edges detected"));
        }
        if (topology.boundary_edges > 0U)
        {
            issues.emplace_back(MakeIssue(
                "PM-SLICER-TOPOLOGY-0010",
                "warning",
                topology.boundary_edges,
                "boundary edges detected"));
        }
        if (topology.accepted_triangles == 0U)
        {
            admission = "blocked";
        }
        const slicer_core::api::Bounds3d bounds{
            {model->bbox_mm.min.x, model->bbox_mm.min.y, model->bbox_mm.min.z},
            {model->bbox_mm.max.x, model->bbox_mm.max.y, model->bbox_mm.max.z}};
        return MakeSuccess({
            {"admission", admission},
            {"issues", slicer_core::Json{std::move(issues)}},
            {"topology", slicer_core::Json::object({
                {"boundaryEdges", static_cast<std::uint64_t>(topology.boundary_edges)},
                {"nonManifoldEdges", static_cast<std::uint64_t>(topology.non_manifold_edges)},
                {"selfIntersectionPairs", static_cast<std::uint64_t>(0U)},
                {"isClosed", topology.boundary_edges == 0U
                    && topology.non_manifold_edges == 0U}})},
            {"bboxMm", MakeBounds(bounds)},
            {"outOfBounds", false}});
    }
    catch (const CapabilityRequestError&)
    {
        throw;
    }
    catch (const std::exception& error)
    {
        return MakeFailure(
            "PM-SLICER-INPUT-0002",
            "fast model preflight failed",
            error.what());
    }
}

slicer_core::Json ModelCapabilityAdapter::MakeIssue(
    const std::string& code,
    const std::string& severity,
    const std::size_t count,
    const std::string& detail)
{
    return slicer_core::Json::object({
        {"code", code},
        {"severity", severity},
        {"count", static_cast<std::uint64_t>(count)},
        {"detail", detail}});
}

}  // namespace slicesoft::module
