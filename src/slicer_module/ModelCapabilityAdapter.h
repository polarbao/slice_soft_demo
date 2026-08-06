#pragma once

#include "slicer_module/CapabilityJsonAdapter.h"
#include "slicer_core/api/ModelFacade.h"
#include "slicer_core/model.h"
#include "slicer_core/scene/SceneModel.h"

#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <string>

namespace slicesoft::module
{

/** @brief Cancellation token used by synchronous light operations. */
class NeverCancelToken final : public slicer_core::api::ICancelToken
{
public:
    /** @brief Reports that synchronous work remains active. @return Always false. */
    [[nodiscard]] bool IsCancelRequested() const noexcept override;
};

/** @brief Imported model resources retained for scene and ViewData facades. */
struct ImportedModelResource
{
    slicer_core::api::ModelMetadata metadata;
    std::shared_ptr<const slicer_core::SceneModel> scenemodel;
};

/** @brief Wires model and fast-preflight capability DTOs to base services. */
class ModelCapabilityAdapter final
{
public:
    /** @brief Creates an adapter with the production model facade. */
    ModelCapabilityAdapter();

    /** @brief Destroys the retained facade and model resources. */
    ~ModelCapabilityAdapter();

    /** @brief Executes a model capability. @param capability Frozen ID. @param request DTO object. @return Result envelope. */
    [[nodiscard]] slicer_core::Json Execute(
        const std::string& capability,
        const slicer_core::Json& request);

    /** @brief Finds one retained model. @param modelId Numeric API identity. @return Shared resource or nullptr. */
    [[nodiscard]] std::shared_ptr<const ImportedModelResource> Find(
        slicer_core::api::ModelId modelId) const;

    /** @brief Resolves an imported model by source path. @param path Source path. @return Shared resource or nullptr. */
    [[nodiscard]] std::shared_ptr<const ImportedModelResource> FindByPath(
        const std::filesystem::path& path) const;

private:
    [[nodiscard]] static std::string NormalizePath(
        const std::filesystem::path& path);
    [[nodiscard]] static slicer_core::Json MakeMetadata(
        const slicer_core::api::ModelMetadata& metadata);
    [[nodiscard]] slicer_core::Json Import(const slicer_core::Json& request);
    [[nodiscard]] slicer_core::Json GetMetadata(
        const slicer_core::Json& request) const;
    [[nodiscard]] slicer_core::Json Release(const slicer_core::Json& request);
    [[nodiscard]] std::shared_ptr<const slicer_core::SceneModel>
        ResolvePreflightModel(const slicer_core::Json& request) const;
    [[nodiscard]] slicer_core::Json RunFastPreflight(
        const slicer_core::Json& request) const;
    [[nodiscard]] static slicer_core::Json MakeIssue(
        const std::string& code,
        const std::string& severity,
        std::size_t count,
        const std::string& detail);

    std::unique_ptr<slicer_core::api::ModelFacade> m_facade;
    mutable std::mutex m_mutex;
    std::map<slicer_core::api::ModelId, std::shared_ptr<ImportedModelResource>>
        m_resources;
    std::map<std::string, slicer_core::api::ModelId> m_pathToModel;
};

}  // namespace slicesoft::module
