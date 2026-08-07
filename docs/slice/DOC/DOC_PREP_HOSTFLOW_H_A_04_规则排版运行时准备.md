# DOC_PREP HOSTFLOW H-A-04 规则排版运行时准备

> 状态：**PREPARATION GATE PASS / IMPLEMENTATION AUTHORIZED**  
> 日期：2026-08-07  
> 任务：H-A-04 `applyGridLayout`

## Goal

通过既有 `scene.apply_operation` 暴露 11×2 行优先规则排版，使打印宿主不复制排版算法，且
保持与主干 `SceneLayoutPanel` 一致的间距、隐藏占位、锁定和碰撞语义。

## Reuse Audit

| 现有模块 | 可复用事实 | 本卡接线 |
|---|---|---|
| `GridLayoutPolicy` | 无 Qt、稳定行优先、1..11 列、1..2 行、最多 22 实例 | Facade 候选态调用 |
| `SceneFacade` | operationId replay、revision、候选态原子提交 | 新操作沿用同一事务 |
| `SceneCapabilityAdapter` | 已有 handle/inline/implicit 三条场景路径 | 只增加 layout DTO 解析 |
| `SceneCollisionService` | 统一碰撞和越界真值 | 排版提交后由 authority 重新计算 |
| `SceneLayoutPanel` | 使用相同 `GridLayoutPolicy` | 作为行为对照，不修改主干 UI |

## Scope

修改：

```text
contracts/slicer_capability_dtos.*
src/slicer_core/api/SceneDtos.h
src/slicer_core/api/scene/SceneFacadeOperation.cpp
src/slicer_core/api/scene/SceneFacadeLayout.cpp（新）
src/slicer_module/SceneCapabilityAdapter.cpp
src/slicer_module/SceneLifecycleSupport.*
tests/hostflow/SceneLayoutAdapterTests.cpp（新）
tests/contracts/scene_facade_14b03/Main.cpp
tests/unit/grid_layout_policy/Main.cpp（修正已漂移的 10 mm 默认值断言）
CMakeLists.txt
```

不修改：

```text
contracts/print_module_spi.h
apps/slicer_debug_ui/**
p0.rgbwsv.2 / TIFF writer / RIP reader
能力数量与导出数量
```

## Runtime Rules

1. `applyGridLayout` 是单操作批次，禁止与 add/remove/transform 混用。
2. layout 字段完整必填，范围和常量不合法时返回稳定输入/配置错误。
3. Facade 在 authority 副本上计算全部 placement，失败不增加 revision。
4. 成功时只增加一次 scene revision；变换 revision 仅对实际改变实例增加。
5. locked overlap、容量溢出、空场景和非法 bounds 均 fail-closed。
6. operation fingerprint 包含全部 layout 参数；同 id 改参数必须失败。

## Test Matrix

正向：1、11、12、22 个实例；10 mm 列/行间距；隐藏占位；锁定无冲突；exact replay；
二次排版不累积旧 derived offset。

负向：0 实例、23 实例、容量不足、列数 0/12、行数 0/3、负间距、错误常量、与其他 operation
混批、携带 instanceId、锁定冲突、同 operationId 改间距。

## Gate Conclusion

合同、代码所有权、复用策略、错误边界和测试入口均明确；用户已授权受控修订。
H-A-04 可以进入开发，完成后 H-A-03 才具备完整前置。

