# DOC_PREP_16B-02 ContactLevelingAnalyzer 实施准备

> 状态：**PREPARATION GATE PASS / IMPLEMENTED**
> 日期：2026-08-12
> 对应任务：`16B-02`

## 1. 准备结论

16B-01 已冻结两侧边界带、首半 slab 接触面积代理、方向约束和角度边界。本卡在这些只读
指标上增加确定性有界搜索，不修改源模型、实例变换、生产配置或自动定向默认。代码归属
`slicer_core/geometry`，无 Qt、材料、支撑和输出依赖。

## 2. 搜索合同

```text
轴：只绕甲片长轴 +Y 作 X/Z 平面滚转；
粗搜索：[-12, +12] deg，固定 0.5 deg；
精化：粗最优附近 +/-0.5 deg，固定 0.1 deg；
地面：每个临时候选独立平移到 minZ=0；
主目标：最大化首半 physical slab 的 XY 投影接触面积代理；
平局：先最小化两侧下包络绝对差，再最小化绝对角，最后选择数值较小角；
预算：高度增量 <=0.5 mm，X footprint 增量 <=0.5 mm；
方向：保持 16B-01 +Z 与尖端 +Y 约束；
输出：diagnostic_only，不返回已旋转顶点。
```

## 3. 稳定回退

| 条件 | `rejectionReason` |
|---|---|
| 基线不可测 | `baseline_<ContactPostureMetrics reason>` |
| 无候选满足方向/角度/高度/占地 | `no_candidate_satisfies_constraints` |

非法策略参数抛出 `invalid_argument`，调用方不得静默改用其他搜索范围。

## 4. 验证资产

沿用 16B-01 的 Reality 101..105 与标准 `nai_you` 六资产，输出：

```text
schema = slicesoft.stage16.contact_leveling_diagnostic.1
docs/slice/REPORT/assets/contact_leveling_diagnostic.json
output/benchmarks/stage16/contact_leveling_diagnostic.json
```

本卡只验证候选可生成、约束和确定性；接触/支撑/准入 A/B 属于 16B-03。
