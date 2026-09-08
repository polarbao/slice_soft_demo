#pragma once

#include "slicer_core/output/rgbwsv/RgbwsvPackage.h"
#include "slicer_core/pipeline/SceneRasterTypes.h"

#include <cstddef>
#include <cstdint>

namespace slicer_core
{

/** @brief RGBWSV 通道数与两个特殊通道的下标（合成与校验共用）。 */
inline constexpr std::size_t kSceneChannelCount{6U};
inline constexpr std::size_t kSceneSupportChannel{4U};
inline constexpr std::size_t kSceneVarnishChannel{5U};

/**
 * @brief 逐像素闭合判定的三个谓词。
 *
 * MF-14a 从 `SceneLayerComposer.cpp` 下沉。它们是**整条合成路径最热的代码**：
 * `ValidateLayer` 的逐像素循环对每个像素都要走一遍，生产口径实测该循环
 * 约 18.3 s / 143 层（约 128 ms/层、737 万像素/层），是 compose 窗口里
 * 最大的一块。下沉后它们可被独立对拍，也让那条循环的开销一目了然。
 *
 * ⚠ **全部使用 `[]` 而非 `.at()`，这是有前提的。** 调用方
 * （`ValidateLayer`）在进入逐像素循环之前已经校验过全部尺寸：
 * `layer.output.channels.size() == pixelCount * kSceneChannelCount`，
 * 且四个 ownership 掩码各走过 `IsBinaryMask(mask, pixelCount)`（其中含
 * size 判定）。故 `pixelIndex < pixelCount` 时所有下标必然在界内。
 * 原先用 `.at()`，每像素约 20 次边界检查，143 层 x 737 万像素合计约
 * **210 亿次** —— 纯开销。
 *
 * **删改调用方那些尺寸校验之前，必须先回来看这段。**
 */

/** @brief 通道字节在交错 RGBWSV 缓冲里的下标。 */
[[nodiscard]] inline std::size_t SceneChannelIndex(
    const std::size_t pixelIndex,
    const std::size_t channel) noexcept
{
    return pixelIndex * kSceneChannelCount + channel;
}

/** @brief 该像素的六个通道是否全为 empty_value。 */
[[nodiscard]] bool IsEmptySourcePixel(
    const SceneInstanceRasterLayer& layer,
    std::size_t pixelIndex,
    std::uint8_t emptyValue);

/** @brief 按 model > outerVarnish > support 的优先级解出像素归属。 */
[[nodiscard]] SceneRasterOwnership ResolveOwnership(
    const SceneInstanceRasterLayer& layer,
    std::size_t pixelIndex);

/** @brief 该像素的通道字节是否与其材料归属自闭合。 */
[[nodiscard]] bool SourcePixelHasClosure(
    const SceneInstanceRasterLayer& layer,
    std::size_t pixelIndex,
    SceneRasterOwnership ownership,
    const RgbwsvProtocol& protocol);

}  // namespace slicer_core
