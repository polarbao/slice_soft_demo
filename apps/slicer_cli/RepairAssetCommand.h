#pragma once

#include <cstdint>
#include <filesystem>

namespace slicer_cli
{

/** @brief `--repair-asset` 的输入。 */
struct RepairAssetRequest
{
    std::filesystem::path inputModelPath;
    std::filesystem::path outputObjPath;
    /// @brief 允许在删除退化面后补闭合边界环；默认关闭，须显式开启。
    bool allowBoundaryFill{false};
    /// @brief 单个边界环允许的最大边数；仅 allowBoundaryFill 时生效。
    std::uint64_t maxBoundaryLoopEdges{0U};
    /// @brief 单个孔洞允许的最大面积；仅 allowBoundaryFill 时生效。
    double maxHoleAreaMm2{0.0};
    /// @brief 边界环允许的最大直径。
    double maxBoundaryLoopDiameterMm{0.0};
    /// @brief 边界环允许的最大周长。
    double maxBoundaryLoopPerimeterMm{0.0};
    /// @brief 边界环允许的最大平面度误差。
    double maxBoundaryPlanarityErrorMm{0.0};
    /// @brief 允许被填补影响的面占比上限，必须在 (0, 1]。
    double maxAffectedFaceRatio{0.0};
    /// @brief 退化面判定阈值（面积平方，mm^4）；<=0 表示用适配器默认值。
    double degenerateAreaEpsilonMm2{0.0};
};

/**
 * @brief 执行确定性资产修复并写出 OBJ/MTL。
 *
 * 仅做保守清理：删除显式退化面与同属性同绕序的精确重复面；
 * 显式开启时才追加有限的边界环填补。不改变 model occupancy，
 * 不做顶点焊接、不翻转绕序——那些会改变几何语义，须由单独授权的卡承担。
 *
 * @param request 输入模型、输出路径与边界填补限额。
 * @return 进程退出码；0 表示修复完成且证据已打印。
 */
[[nodiscard]] int RunRepairAsset(const RepairAssetRequest& request);

}  // namespace slicer_cli
