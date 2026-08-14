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

/** @brief 同步轻量操作使用的取消令牌。 */
class NeverCancelToken final : public slicer_core::api::ICancelToken
{
public:
    /** @brief 报告同步工作仍在继续。 @return 始终返回 false。 */
    [[nodiscard]] bool IsCancelRequested() const noexcept override;
};

/** @brief 为场景和 ViewData Facade 保留的已导入模型资源。 */
struct ImportedModelResource
{
    slicer_core::api::ModelMetadata metadata;
    std::shared_ptr<const slicer_core::SceneModel> scenemodel;
};

/** @brief 将模型及快速预检能力 DTO 接入基础服务。 */
class ModelCapabilityAdapter final
{
public:
    /** @brief 使用生产 ModelFacade 创建适配器。 */
    ModelCapabilityAdapter();

    /** @brief 销毁所持 ModelFacade 和模型资源。 */
    ~ModelCapabilityAdapter();

    /** @brief 执行模型能力。 @param capability 冻结 ID。 @param request DTO 对象。 @return 结果对象。 */
    [[nodiscard]] slicer_core::Json Execute(
        const std::string& capability,
        const slicer_core::Json& request);

    /** @brief 查找一个保留模型。 @param modelId 数值 API 标识。 @return 共享资源或 nullptr。 */
    [[nodiscard]] std::shared_ptr<const ImportedModelResource> Find(
        slicer_core::api::ModelId modelId) const;

    /** @brief 按源路径解析已导入模型。 @param path 源路径。 @return 共享资源或 nullptr。 */
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
