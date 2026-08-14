#pragma once

#include "slicer_core/geometry/GeometryOccupancyPolicy.h"

#include <cstdint>
#include <span>
#include <vector>

namespace slicer_core
{

/** @brief 层占用请求使用的几何场准入信息。 */
enum class GeometryOccupancyInputKind
{
    SingleIntervalHeightfield,
    GeneralMesh
};

/** @brief 以毫米垂直占用范围表示的一列 XY 采样。 */
struct GeometryOccupancyColumn
{
    bool occupied{false};
    double minimumZMm{0.0};
    double maximumZMm{0.0};
};

/** @brief 用于生成层占用掩码的纯几何输入。 */
struct LayerOccupancyRequest
{
    std::span<const GeometryOccupancyColumn> columns;
    std::span<const GeometryOccupancyColumn> coverageSubsampleColumns;
    int layerCount{0};
    double layerThicknessMm{0.0};
    GeometryOccupancyInputKind inputKind{GeometryOccupancyInputKind::SingleIntervalHeightfield};
    GeometryOccupancyPolicy policy{MakeLegacyGeometryOccupancyPolicy()};
};

/**
 * @brief 先按层、再按 XY 列索引生成的二值占用掩码。
 *
 * 成员名保留惯用 camelCase，因为此 DTO 是公共核心合同，
 * 而非项目自定义、采用全小写成员名的测试夹具结构。
 */
struct LayerOccupancyResult
{
    std::vector<std::vector<std::uint8_t>> masks;
    std::vector<int> firstOccupiedLayers;
    std::vector<int> lastOccupiedLayers;
};

/**
 * @brief 使用显式 STL-only 策略生成几何占用掩码。
 * @param request 列场、可选固定 2x2 覆盖场、输出层几何及策略。
 * @return 层掩码和逐列占用范围。
 * @throws std::invalid_argument 请求无效或选择不受支持的候选策略时抛出。
 */
LayerOccupancyResult BuildLayerOccupancy(const LayerOccupancyRequest& request);

/**
 * @brief 在不调用候选算法的前提下验证几何占用策略。
 * @param policy 待验证策略。
 * @throws std::invalid_argument 当前提供者不支持该策略时抛出。
 */
void ValidateLayerOccupancyPolicy(const GeometryOccupancyPolicy& policy);

}  // namespace slicer_core
