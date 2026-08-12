# DOC_PREP_16B-01 边界带与接触指标基线实施准备

> 状态：**PREPARATION GATE PASS / IMPLEMENTED**
> 日期：2026-08-12
> 对应任务：`16B-01`

## 1. 准备结论

`16-00` 已为接触姿态诊断给出 GO，且本卡只读取完成自动定向后的几何，不修改顶点、
生产配置、材料、支撑、Package 或 UI。实现归属 `slicer_core/geometry`，满足架构边界。

## 2. 冻结测量合同

| 项目 | 冻结定义 |
|---|---|
| 输入姿态 | 已执行现有 `autoOrient` 并完成 Z=0 归地的 `ModelReport` |
| 长轴 | bbox 的 Y 跨度必须大于 X 跨度，否则显式拒绝 |
| 两侧边界带 | X 最小侧和最大侧各占横向跨度的 `12.5%` |
| 中心带 | 以 X 中心为基准，半宽等于横向跨度的 `12.5%`；仅用于 +Z 约束 |
| 地面 | 定向后 bbox 的 `minZ` |
| 接触 slab | 层高固定 `0.038 mm`；记录前 `1/2` slab、第一 slab 和前两 slab |
| 接触面积 | 三角形按水平 Z 平面裁剪后，其 XY 投影面积之和；这是稳定的只读接触代理，不是二维并集 |
| 候选角 | `atan2(rightMinZ-leftMinZ, transverseSpan)`；正值表示右侧高于左侧 |
| 角度边界 | 只读候选绝对值不超过 `12 deg` |
| +Z 约束 | 中心带最低包络不得低于两侧最低包络 |
| +Y 约束 | +Y 端带宽度不大于 -Y 端带宽度，即甲片尖端朝 +Y |

## 3. 资产与出口

固定资产为 `model/obj/reality` 下 2026-07-29 的 `segment_101..105`，以及标准甲片
`model/obj/nai_you_new/MF_nai_you.obj`。每项记录相对路径、SHA-256、自动定向选择、
bbox、三组接触面积、边界包络、角度和方向约束。

机器可读出口：

```text
schema = slicesoft.stage16.posture_baseline.1
docs/slice/REPORT/assets/posture_baseline.json
output/benchmarks/stage16/posture_baseline.json
```

## 4. 边界

本卡不搜索角度、不应用旋转、不改变现有自动定向结果，不将接触代理提升为生产决策。
`16B-02` 才可生成 diagnostic-only 候选；实际应用仍需经过后续 A/B、用户授权和准入 Gate。

