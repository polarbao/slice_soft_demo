#pragma once

#include "slicer_core/api/ApiResult.h"
#include "slicer_core/api/CommonDtos.h"
#include "slicer_core/scene/SceneModel.h"
#include "slicer_core/texture_image.h"

#include <filesystem>
#include <map>
#include <memory>

namespace slicer_core::api
{

/**
 * @brief 纹理 ViewData 使用的只读模型资源边界。
 */
class ISceneViewModelRepository
{
public:
    /** @brief 通过接口销毁仓库。 */
    virtual ~ISceneViewModelRepository() = default;

    /**
     * @brief 解析一个不可变的已导入模型。
     * @param modelId 场景快照中的数值 API 模型标识。
     * @return 共享模型资源或稳定的 PM-SLICER 错误。
     */
    [[nodiscard]] virtual ApiResult<std::shared_ptr<const SceneModel>> GetModel(
        ModelId modelId) const noexcept = 0;
};

/**
 * @brief 失败即拒绝的 ViewData 所使用的只读纹理解码器边界。
 */
class ISceneViewTextureSource
{
public:
    /** @brief 通过接口销毁纹理源。 */
    virtual ~ISceneViewTextureSource() = default;

    /**
     * @brief 将一个已声明纹理加载为直通 Alpha RGBA8。
     * @param path 已导入模型保留的纹理路径。
     * @return 已解码像素或稳定的缺失/解码错误。
     */
    [[nodiscard]] virtual ApiResult<TextureImage> Load(
        const std::filesystem::path& path) const noexcept = 0;
};

/**
 * @brief 创建由映射支撑的不可变模型仓库。
 * @param models 以数值 API 标识为键的模型。
 * @return 就绪仓库或稳定的 PM-SLICER 错误。
 */
[[nodiscard]] ApiResult<std::shared_ptr<const ISceneViewModelRepository>>
CreateSceneViewModelRepository(
    std::map<ModelId, std::shared_ptr<const SceneModel>> models) noexcept;

/**
 * @brief 创建生产宿主使用的文件系统/WIC 纹理源。
 * @return 就绪的只读纹理源。
 */
[[nodiscard]] std::shared_ptr<const ISceneViewTextureSource>
CreateFileSceneViewTextureSource();

}  // namespace slicer_core::api
