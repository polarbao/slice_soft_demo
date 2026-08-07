# DOC_DECISION 14F-R3 HOSTFLOW 规则排版合同受控修订

## Status

**ACCEPTED / USER AUTHORIZED**

> 日期：2026-08-07  
> 关联任务：HOSTFLOW H-A-04  
> 上游：H-A-01、H-A-02，DTO v1.6  
> 授权记录：用户授权继续 H-X/H-A 后续开发，并授予 H-A 合同修订权限。

## Context

打印宿主需要一次完成最多 11×2 个实例的规则排版。切片核心已经有无 Qt 的
`GridLayoutPolicy`，但冻结 ABI 只有实例增删和单实例变换。若让打印宿主自行计算每个实例的
落位，会复制切片侧排版算法，并使主 UI 与打印宿主的间距、锁定和碰撞语义发生漂移。

## Decision

将能力 DTO 从 v1.6 受控提升到 v1.7，在现有 `scene.apply_operation.operations[].type` 中增加
`applyGridLayout`，不新增能力和导出。该操作携带完整 `layout`：

```text
policy=grid
maxColumns=1..11
maxRows=1..2
columnGapMm>=0
rowGapMm>=0
spacingMode=edge_clearance
order=row_major
```

冻结语义：

1. 操作作用于场景稳定实例顺序，最多 22 个实例。
2. 隐藏实例占位，锁定实例保持原位；锁定冲突 fail-closed。
3. `applyGridLayout` 必须是请求内唯一 operation，避免变换中间态成为排版基准。
4. 成功响应继续返回 canonical transforms、碰撞、越界、revision 和 scene hash。
5. 独立能力 id `scene.layout` 继续禁止；必须复用 `scene.apply_operation`。
6. SPI v1、11 个导出、15 项能力、三车道和生产 TIFF 协议均不改变。

## Consequences

- 宿主无需 include `slicer_core` 或复制 `GridLayoutPolicy`。
- 规则排版与主干 `SceneLayoutPanel` 使用同一核心策略。
- H-A-03 可在纯 C/Qt 宿主闭环中验证导入、排版、切片和校验。
- DTO minor 使用新枚举和条件字段；旧操作请求与响应保持兼容。

## Validation

```powershell
python tests/contracts/ValidateCapabilityDtos.py
python tests/contracts/ValidateThreeLaneContract.py
cmake --build build-slicesoft/main --config Debug --target hostflow_ha04_grid_layout_tests scene_facade_14b03_unit_tests grid_layout_policy_unit_tests
ctest --test-dir build-slicesoft/main -C Debug -R "hostflow_ha04|scene_facade_14b03|grid_layout_policy" --output-on-failure
cmake --build build-slicesoft/main --config Release --target hostflow_ha04_grid_layout_tests scene_facade_14b03_unit_tests grid_layout_policy_unit_tests
ctest --test-dir build-slicesoft/main -C Release -R "hostflow_ha04|scene_facade_14b03|grid_layout_policy" --output-on-failure
```

