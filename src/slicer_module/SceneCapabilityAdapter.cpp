#include "slicer_module/SceneCapabilityAdapter.h"

#include "slicer_module/ModelCapabilityAdapter.h"
#include "slicer_module/SceneCapabilitySerializationAdapter.h"
#include "slicer_module/SceneLifecycleSupport.h"
#include "slicer_module/SceneViewDataAdapter.h"
#include "slicer_core/api/scene/SceneFacadeService.h"
#include "slicer_core/api/viewdata/SceneViewResources.h"
#include "slicer_core/api/viewdata/TexturedSceneViewDataProvider.h"
#include "slicer_core/scene/MultiModelScene.h"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

namespace slicesoft::module
{

class SceneCapabilityAdapter::Implementation final
{
public:
    explicit Implementation(ModelCapabilityAdapter& models) noexcept
        : m_models(models)
    {
    }

    [[nodiscard]] CapabilityOutput Execute(
        const std::string& capability,
        const slicer_core::Json& request)
    {
        if (capability == "scene.get_viewdata"
            && OptionalString(request, "operation", "query") == "read_blob")
        {
            return m_viewData.ReadBlob(request);
        }
        slicer_core::Json result = MakeFailure(
            "PM-SLICER-INTERNAL-0099",
            "scene capability adapter received an unsupported capability",
            capability);
        if (capability == "scene.apply_operation")
        {
            result = ApplyOperation(request);
        }
        else if (capability == "scene.get_snapshot")
        {
            result = GetSnapshot(request);
        }
        else if (capability == "scene.get_viewdata")
        {
            result = GetViewData(request);
        }
        else if (capability == "geometry.collision")
        {
            result = CheckCollision(request);
        }
        return MakeCapabilityOutput(result);
    }

private:
    struct SceneSession
    {
        slicer_core::api::SceneId id{0U};
        std::string externalid;
        std::shared_ptr<slicer_core::api::SceneFacadeService> facade;
        std::shared_ptr<
            scene_lifecycle::MutableSceneViewModelRepository> repository;
        std::mutex mutex;
        std::uint64_t nextinstanceid{1U};
        std::map<std::string, std::map<std::size_t, std::string>>
            generatedinstanceids;
    };

    struct SceneBuildResult
    {
        std::shared_ptr<SceneSession> session;
        std::optional<slicer_core::api::ApiError> error;
    };

    [[nodiscard]] SceneBuildResult BuildSession(
        const slicer_core::Json& sceneDocument)
    {
        const auto decoded = slicer_core::DeserializeMultiModelScene(sceneDocument);
        if (!decoded.IsValid())
        {
            return {{}, slicer_core::api::ApiError{
                "PM-SLICER-INPUT-0002",
                "scene document is invalid",
                decoded.error->message}};
        }
        slicer_core::api::SceneFacadeSeed seed;
        {
            std::scoped_lock lock{m_mutex};
            seed.scene_id = m_nextSceneId++;
        }
        seed.scene = decoded.scene;
        seed.validation_purpose = slicer_core::SceneValidationPurpose::Draft;
        auto repository = std::make_shared<
            scene_lifecycle::MutableSceneViewModelRepository>();
        for (const slicer_core::ModelSource& source : seed.scene.models)
        {
            const std::filesystem::path path = ResolveSourcePath(seed.scene, source);
            const auto resource = m_models.FindByPath(path);
            if (!resource)
            {
                return {{}, slicer_core::api::ApiError{
                    "PM-SLICER-INPUT-0001",
                    "scene model must be imported before scene capabilities",
                    path.generic_string()}};
            }
            seed.models_by_id[source.modelid] = resource->scenemodel;
            seed.api_model_ids[source.modelid] = resource->metadata.model_id;
            const auto registered = repository->Register(
                resource->metadata.model_id,
                resource->scenemodel);
            if (!registered.IsOk())
            {
                return {{}, *registered.Error()};
            }
            auto registration = scene_lifecycle::BuildModelRegistration(
                *resource);
            if (!registration.IsOk())
            {
                return {{}, *registration.Error()};
            }
            const auto scope = std::find_if(
                seed.scene.resourcescopes.begin(),
                seed.scene.resourcescopes.end(),
                [&source](const slicer_core::ResourceScope& value)
                {
                    return value.resourcescopeid == source.resourcescopeid;
                });
            if (scope == seed.scene.resourcescopes.end())
            {
                return {{}, slicer_core::api::ApiError{
                    "PM-SLICER-INPUT-0002",
                    "scene model resource scope is missing",
                    source.modelid}};
            }
            auto inlineRegistration = *registration.Value();
            inlineRegistration.scene_model_id = source.modelid;
            inlineRegistration.source = source;
            inlineRegistration.scope = *scope;
            seed.registered_models[resource->metadata.model_id] =
                std::move(inlineRegistration);
        }
        auto provider = slicer_core::api::CreateTexturedSceneViewDataProvider(
            repository,
            slicer_core::api::CreateFileSceneViewTextureSource());
        if (!provider.IsOk())
        {
            return {{}, *provider.Error()};
        }
        const slicer_core::api::SceneId sceneId = seed.scene_id;
        const auto facade = slicer_core::api::SceneFacadeService::Create(
            std::move(seed),
            *provider.Value());
        if (!facade.IsOk())
        {
            return {{}, *facade.Error()};
        }
        auto session = std::make_shared<SceneSession>();
        session->id = sceneId;
        session->externalid = decoded.scene.sceneid;
        session->facade = *facade.Value();
        session->repository = std::move(repository);
        return {std::move(session), std::nullopt};
    }

    [[nodiscard]] SceneBuildResult BuildImplicitSession(
        const slicer_core::Json& sceneContext)
    {
        slicer_core::api::SceneId sceneId{0U};
        {
            std::scoped_lock lock{m_mutex};
            sceneId = m_nextSceneId++;
        }
        auto seed = scene_lifecycle::BuildImplicitSceneSeed(
            sceneContext,
            sceneId);
        if (!seed.IsOk())
        {
            return {{}, *seed.Error()};
        }
        auto repository = std::make_shared<
            scene_lifecycle::MutableSceneViewModelRepository>();
        auto provider = slicer_core::api::CreateTexturedSceneViewDataProvider(
            repository,
            slicer_core::api::CreateFileSceneViewTextureSource());
        if (!provider.IsOk())
        {
            return {{}, *provider.Error()};
        }
        const auto facade = slicer_core::api::SceneFacadeService::Create(
            std::move(*seed.Value()),
            *provider.Value());
        if (!facade.IsOk())
        {
            return {{}, *facade.Error()};
        }
        auto session = std::make_shared<SceneSession>();
        session->id = sceneId;
        session->externalid = "scene-" + std::to_string(sceneId);
        session->facade = *facade.Value();
        session->repository = std::move(repository);
        return {std::move(session), std::nullopt};
    }

    [[nodiscard]] static std::filesystem::path ResolveSourcePath(
        const slicer_core::MultiModelScene& scene,
        const slicer_core::ModelSource& source)
    {
        if (source.sourcepath.is_absolute())
        {
            return source.sourcepath;
        }
        for (const slicer_core::ResourceScope& scope : scene.resourcescopes)
        {
            if (scope.resourcescopeid == source.resourcescopeid)
            {
                return scope.rootpath / source.sourcepath;
            }
        }
        return source.sourcepath;
    }

    void StoreSession(
        const std::shared_ptr<SceneSession>& session,
        const std::string& implicitOperationId = {})
    {
        std::scoped_lock lock{m_mutex};
        m_byId[session->id] = session;
        m_byExternalId[session->externalid] = session;
        if (!implicitOperationId.empty())
        {
            m_implicitByOperationId[implicitOperationId] = session;
        }
    }

    [[nodiscard]] std::shared_ptr<SceneSession> FindImplicitSession(
        const std::string& operationId) const
    {
        std::scoped_lock lock{m_mutex};
        const auto entry = m_implicitByOperationId.find(operationId);
        return entry == m_implicitByOperationId.end()
            ? nullptr
            : entry->second;
    }

    [[nodiscard]] std::shared_ptr<SceneSession> FindSession(
        const slicer_core::Json& request) const
    {
        std::scoped_lock lock{m_mutex};
        if (request.contains("sceneHandle"))
        {
            const auto entry = m_byId.find(RequireUnsigned(request, "sceneHandle"));
            return entry == m_byId.end() ? nullptr : entry->second;
        }
        const std::string sceneId = RequireString(request, "sceneId");
        const auto entry = m_byExternalId.find(sceneId);
        return entry == m_byExternalId.end() ? nullptr : entry->second;
    }

    [[nodiscard]] slicer_core::Json ApplyOperation(
        const slicer_core::Json& request)
    {
        const std::string operationId = RequireString(request, "operationId");
        const auto& operations = RequireArray(request, "operations");
        const bool hasHandle = request.contains("sceneHandle");
        const bool hasContext = request.contains("sceneContext");
        const bool hasScene = request.contains("scene");
        const slicer_core::Json* sceneDocument = nullptr;
        if (hasScene)
        {
            sceneDocument = &RequireObject(request, "scene");
        }
        const bool nonEmptyInlineScene = sceneDocument != nullptr
            && sceneDocument->size() > 0U;
        const bool hasAddInstance = std::any_of(
            operations.begin(),
            operations.end(),
            [](const slicer_core::Json& operation)
            {
                return operation.is_object()
                    && operation.contains("type")
                    && operation.at("type").is_string()
                    && operation.at("type").as_string() == "addInstance";
            });

        if (hasContext && (hasHandle || nonEmptyInlineScene))
        {
            return MakeFailure(
                "PM-SLICER-INPUT-0002",
                "sceneContext is only valid for implicit scene creation");
        }

        std::shared_ptr<SceneSession> session;
        bool implicitRequest{false};
        bool newImplicitSession{false};
        if (nonEmptyInlineScene)
        {
            const SceneBuildResult built = BuildSession(*sceneDocument);
            if (built.error)
            {
                return MakeFailure(*built.error);
            }
            session = built.session;
            StoreSession(session);
        }
        else if (hasHandle)
        {
            session = FindSession(request);
        }
        else
        {
            session = FindImplicitSession(operationId);
            if (session)
            {
                implicitRequest = true;
                if (!hasContext)
                {
                    return MakeFailure(
                        "PM-SLICER-INPUT-0002",
                        "implicit scene replay requires sceneContext");
                }
            }
            else if (hasAddInstance)
            {
                implicitRequest = true;
                if (!hasContext)
                {
                    return MakeFailure(
                        "PM-SLICER-INPUT-0002",
                        "implicit scene creation requires sceneContext");
                }
                if (RequireUnsigned(request, "currentSceneRevision") != 0U
                    || RequireUnsigned(request, "expectedSceneRevision") != 0U)
                {
                    return MakeFailure(
                        "PM-SLICER-PROFILE-0031",
                        "implicit scene creation requires revision zero");
                }
                const SceneBuildResult built = BuildImplicitSession(
                    RequireObject(request, "sceneContext"));
                if (built.error)
                {
                    return MakeFailure(*built.error);
                }
                session = built.session;
                newImplicitSession = true;
            }
            else
            {
                session = FindSession(request);
            }
        }
        if (!session)
        {
            return MakeFailure(
                "PM-SLICER-INPUT-0001",
                "scene handle was not found");
        }
        slicer_core::api::SceneOperationRequest operationRequest;
        operationRequest.scene_id = session->id;
        operationRequest.operation_id = operationId;
        operationRequest.current_scene_revision = RequireUnsigned(
            request,
            "currentSceneRevision");
        operationRequest.expected_scene_revision = RequireUnsigned(
            request,
            "expectedSceneRevision");
        if (hasContext)
        {
            operationRequest.scene_context_identity =
                RequireObject(request, "sceneContext").dump(0);
        }

        for (std::size_t index = 0U; index < operations.size(); ++index)
        {
            const auto parsed = ParseOperation(
                operations[index],
                session,
                operationId,
                index);
            if (!parsed.IsOk())
            {
                return MakeFailure(*parsed.Error());
            }
            operationRequest.operations.push_back(*parsed.Value());
        }
        NeverCancelToken cancelToken;
        const auto result = session->facade->ApplyOperation(
            operationRequest,
            cancelToken);
        if (!result.IsOk())
        {
            return MakeFailure(*result.Error());
        }
        if (newImplicitSession)
        {
            StoreSession(session, operationId);
        }

        slicer_core::Json response =
            SceneCapabilitySerializationAdapter::SerializeCommit(
                *result.Value());
        if (implicitRequest || !hasHandle)
        {
            slicer_core::Json::Object fields = response.as_object();
            fields.emplace("sceneHandle", session->id);
            response = slicer_core::Json{std::move(fields)};
        }
        return response;
    }

    [[nodiscard]] slicer_core::api::ApiResult<
        slicer_core::api::SceneOperation> ParseOperation(
        const slicer_core::Json& operation,
        const std::shared_ptr<SceneSession>& session,
        const std::string& operationId,
        const std::size_t operationIndex)
    {
        if (!operation.is_object())
        {
            throw CapabilityRequestError("operations must contain objects");
        }
        slicer_core::api::SceneOperation result;
        const std::string type = RequireString(operation, "type");
        if (type == "addInstance")
        {
            if (operation.contains("instanceId"))
            {
                throw CapabilityRequestError(
                    "addInstance must not contain instanceId");
            }
            result.type = slicer_core::api::SceneOperationType::AddInstance;
            result.model_id = ParseModelId(RequireString(operation, "modelId"));
            result.instance_id = ResolveAddInstanceId(
                session,
                operation,
                operationId,
                operationIndex);
            if (operation.contains("initialTransform"))
            {
                result.initial_transform = ParseInitialTransform(
                    RequireObject(operation, "initialTransform"));
            }
            const auto resource = m_models.Find(result.model_id);
            if (!resource)
            {
                return slicer_core::api::ApiResult<
                    slicer_core::api::SceneOperation>::Failure({
                    "PM-SLICER-INPUT-0001",
                    "addInstance references an unknown or released modelId",
                    std::to_string(result.model_id)});
            }
            const auto registration =
                scene_lifecycle::BuildModelRegistration(*resource);
            if (!registration.IsOk())
            {
                return slicer_core::api::ApiResult<
                    slicer_core::api::SceneOperation>::Failure(
                    *registration.Error());
            }
            const auto registered = session->facade->RegisterModel(
                *registration.Value());
            if (!registered.IsOk())
            {
                return slicer_core::api::ApiResult<
                    slicer_core::api::SceneOperation>::Failure(
                    *registered.Error());
            }
            const auto viewRegistered = session->repository->Register(
                resource->metadata.model_id,
                resource->scenemodel);
            if (!viewRegistered.IsOk())
            {
                return slicer_core::api::ApiResult<
                    slicer_core::api::SceneOperation>::Failure(
                    *viewRegistered.Error());
            }
        }
        else if (type == "applyGridLayout")
        {
            for (const char* const forbiddenField : {
                     "instanceId",
                     "modelId",
                     "assignInstanceId",
                     "initialTransform"})
            {
                if (operation.contains(forbiddenField))
                {
                    throw CapabilityRequestError(
                        "applyGridLayout contains a forbidden instance field");
                }
            }
            result.type =
                slicer_core::api::SceneOperationType::ApplyGridLayout;
            result.layout = ParseGridLayout(
                RequireObject(operation, "layout"));
        }
        else
        {
            result.instance_id = RequireString(operation, "instanceId");
        }

        if (type == "removeInstance")
        {
            result.type = slicer_core::api::SceneOperationType::RemoveInstance;
        }
        else if (type == "translate")
        {
            result.type = slicer_core::api::SceneOperationType::Translate;
            const auto delta = ReadNumber3(RequireField(operation, "deltaMm"));
            result.value_x = delta[0];
            result.value_y = delta[1];
            result.value_z = delta[2];
        }
        else if (type == "rotateZ")
        {
            result.type = slicer_core::api::SceneOperationType::RotateZ;
            result.value_z = RequireNumber(operation, "degrees");
        }
        else if (type == "uniformScale")
        {
            result.type = slicer_core::api::SceneOperationType::UniformScale;
            result.value_x = RequireNumber(operation, "factor");
        }
        else if (type == "mirror")
        {
            const std::string axis = RequireString(operation, "axis");
            result.type = axis == "x"
                ? slicer_core::api::SceneOperationType::MirrorX
                : slicer_core::api::SceneOperationType::MirrorY;
            if (axis != "x" && axis != "y")
            {
                throw CapabilityRequestError("mirror axis must be x or y");
            }
        }
        else if (type != "addInstance" && type != "applyGridLayout")
        {
            throw CapabilityRequestError("scene operation type is invalid");
        }
        return slicer_core::api::ApiResult<
            slicer_core::api::SceneOperation>::Success(std::move(result));
    }

    [[nodiscard]] static std::string ResolveAddInstanceId(
        const std::shared_ptr<SceneSession>& session,
        const slicer_core::Json& operation,
        const std::string& operationId,
        const std::size_t operationIndex)
    {
        if (operation.contains("assignInstanceId"))
        {
            return RequireString(operation, "assignInstanceId");
        }
        std::scoped_lock lock{session->mutex};
        auto& generated = session->generatedinstanceids[operationId];
        const auto existing = generated.find(operationIndex);
        if (existing != generated.end())
        {
            return existing->second;
        }
        const std::string instanceId =
            "instance-" + std::to_string(session->nextinstanceid++);
        generated.emplace(operationIndex, instanceId);
        return instanceId;
    }

    [[nodiscard]] static slicer_core::ModelTransform ParseInitialTransform(
        const slicer_core::Json& value)
    {
        slicer_core::ModelTransform transform;
        if (value.contains("translateXMm"))
        {
            transform.translatexmm = RequireNumber(value, "translateXMm");
        }
        if (value.contains("translateYMm"))
        {
            transform.translateymm = RequireNumber(value, "translateYMm");
        }
        if (value.contains("rotateZDeg"))
        {
            transform.rotatezdeg = RequireNumber(value, "rotateZDeg");
        }
        if (value.contains("uniformScale"))
        {
            transform.uniformscale = RequireNumber(value, "uniformScale");
        }
        if (value.contains("mirrorX"))
        {
            transform.mirrorx = RequireBoolean(value, "mirrorX");
        }
        if (value.contains("mirrorY"))
        {
            transform.mirrory = RequireBoolean(value, "mirrorY");
        }
        return transform;
    }

    [[nodiscard]] static slicer_core::SceneLayout ParseGridLayout(
        const slicer_core::Json& value)
    {
        slicer_core::SceneLayout layout;
        layout.policy = RequireString(value, "policy");
        layout.maxcolumns = RequireInteger(value, "maxColumns");
        layout.maxrows = RequireInteger(value, "maxRows");
        layout.columngapmm = RequireNumber(value, "columnGapMm");
        layout.rowgapmm = RequireNumber(value, "rowGapMm");
        layout.spacingmode = RequireString(value, "spacingMode");
        layout.order = RequireString(value, "order");
        return layout;
    }

    [[nodiscard]] slicer_core::Json GetSnapshot(
        const slicer_core::Json& request) const
    {
        const auto session = FindSession(request);
        if (!session)
        {
            return MakeFailure("PM-SLICER-INPUT-0001", "scene was not found");
        }
        const auto result = session->facade->GetSnapshot(session->id);
        return result.IsOk()
            ? SceneCapabilitySerializationAdapter::SerializeSnapshot(*result.Value())
            : MakeFailure(*result.Error());
    }

    [[nodiscard]] slicer_core::Json GetViewData(
        const slicer_core::Json& request)
    {
        if (OptionalString(request, "operation", "query") != "query")
        {
            throw CapabilityRequestError("scene.get_viewdata operation is invalid");
        }
        const auto session = FindSession(request);
        if (!session)
        {
            return MakeFailure("PM-SLICER-INPUT-0001", "scene was not found");
        }
        slicer_core::api::SceneViewDataRequest viewRequest;
        viewRequest.scene_id = session->id;
        viewRequest.expected_scene_revision = RequireUnsigned(
            request,
            "expectedSceneRevision");
        viewRequest.view_mode = ParseViewMode(RequireString(request, "viewMode"));
        viewRequest.lod = ParseLod(RequireString(request, "lod"));
        viewRequest.mesh_transform = ParseMeshTransform(
            RequireString(request, "meshTransform"));
        viewRequest.mesh_attribute_format = ParseMeshAttributeFormat(
            OptionalString(request, "meshAttributeFormat", "float32"));
        viewRequest.max_bytes = RequireUnsigned(request, "maxBytes");
        if (RequireString(request, "texturePolicy") != "require_if_present")
        {
            throw CapabilityRequestError("texturePolicy must be require_if_present");
        }
        viewRequest.content.clear();
        for (const auto& item : RequireArray(request, "content"))
        {
            if (!item.is_string())
            {
                throw CapabilityRequestError("content must contain strings");
            }
            viewRequest.content.push_back(ParseContent(item.as_string()));
        }
        if (request.contains("instanceIds"))
        {
            for (const auto& item : RequireArray(request, "instanceIds"))
            {
                if (!item.is_string())
                {
                    throw CapabilityRequestError("instanceIds must contain strings");
                }
                viewRequest.instance_ids.push_back(item.as_string());
            }
        }
        NeverCancelToken cancelToken;
        const auto result = session->facade->GetViewData(viewRequest, cancelToken);
        return result.IsOk()
            ? m_viewData.Serialize(*result.Value())
            : MakeFailure(*result.Error());
    }

    [[nodiscard]] slicer_core::Json CheckCollision(
        const slicer_core::Json& request)
    {
        const SceneBuildResult built = BuildSession(RequireObject(request, "scene"));
        if (built.error)
        {
            return MakeFailure(*built.error);
        }
        const auto snapshot = built.session->facade->GetSnapshot(built.session->id);
        if (!snapshot.IsOk())
        {
            return MakeFailure(*snapshot.Error());
        }
        if (snapshot.Value()->scene_revision != RequireUnsigned(
                request,
                "expectedSceneRevision"))
        {
            return MakeFailure(
                "PM-SLICER-LAYOUT-0022",
                "scene revision is stale");
        }
        NeverCancelToken cancelToken;
        const auto result = built.session->facade->CheckCollision(
            *snapshot.Value(),
            cancelToken);
        return result.IsOk()
            ? SceneCapabilitySerializationAdapter::SerializeCollision(
                snapshot.Value()->scene_revision,
                *result.Value())
            : MakeFailure(*result.Error());
    }

    [[nodiscard]] static slicer_core::api::ViewMode ParseViewMode(
        const std::string& value)
    {
        if (value == "top")
        {
            return slicer_core::api::ViewMode::Top;
        }
        if (value == "three_d")
        {
            return slicer_core::api::ViewMode::ThreeD;
        }
        throw CapabilityRequestError("viewMode is invalid");
    }

    [[nodiscard]] static slicer_core::api::ViewLod ParseLod(
        const std::string& value)
    {
        const std::map<std::string, slicer_core::api::ViewLod> values{
            {"auto", slicer_core::api::ViewLod::Auto},
            {"lod0", slicer_core::api::ViewLod::Lod0},
            {"lod1", slicer_core::api::ViewLod::Lod1},
            {"lod2", slicer_core::api::ViewLod::Lod2},
            {"outline_only", slicer_core::api::ViewLod::OutlineOnly}};
        const auto entry = values.find(value);
        if (entry == values.end())
        {
            throw CapabilityRequestError("lod is invalid");
        }
        return entry->second;
    }

    [[nodiscard]] static slicer_core::api::MeshTransform ParseMeshTransform(
        const std::string& value)
    {
        if (value == "local")
        {
            return slicer_core::api::MeshTransform::Local;
        }
        if (value == "world")
        {
            return slicer_core::api::MeshTransform::World;
        }
        throw CapabilityRequestError("meshTransform is invalid");
    }

    [[nodiscard]] static slicer_core::api::MeshAttributeFormat
    ParseMeshAttributeFormat(const std::string& value)
    {
        if (value == "float32")
        {
            return slicer_core::api::MeshAttributeFormat::Float32;
        }
        if (value == "float16")
        {
            return slicer_core::api::MeshAttributeFormat::Float16;
        }
        throw CapabilityRequestError("meshAttributeFormat is invalid");
    }

    [[nodiscard]] static slicer_core::api::ViewContent ParseContent(
        const std::string& value)
    {
        const std::map<std::string, slicer_core::api::ViewContent> values{
            {"bbox", slicer_core::api::ViewContent::Bbox},
            {"outline", slicer_core::api::ViewContent::Outline},
            {"surface_preview", slicer_core::api::ViewContent::SurfacePreview},
            {"mesh", slicer_core::api::ViewContent::Mesh},
            {"appearance", slicer_core::api::ViewContent::Appearance}};
        const auto entry = values.find(value);
        if (entry == values.end())
        {
            throw CapabilityRequestError("content item is invalid");
        }
        return entry->second;
    }

    ModelCapabilityAdapter& m_models;
    SceneViewDataAdapter m_viewData;
    mutable std::mutex m_mutex;
    slicer_core::api::SceneId m_nextSceneId{1U};
    std::map<slicer_core::api::SceneId, std::shared_ptr<SceneSession>> m_byId;
    std::map<std::string, std::shared_ptr<SceneSession>> m_byExternalId;
    std::map<std::string, std::shared_ptr<SceneSession>>
        m_implicitByOperationId;
};

SceneCapabilityAdapter::SceneCapabilityAdapter(ModelCapabilityAdapter& models)
    : m_implementation(std::make_unique<Implementation>(models))
{
}

SceneCapabilityAdapter::~SceneCapabilityAdapter() = default;

CapabilityOutput SceneCapabilityAdapter::Execute(
    const std::string& capability,
    const slicer_core::Json& request)
{
    return m_implementation->Execute(capability, request);
}

}  // namespace slicesoft::module
