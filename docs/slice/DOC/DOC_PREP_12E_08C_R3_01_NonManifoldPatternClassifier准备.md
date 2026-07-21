# DOC_PREP_12E-08C-R3-01 Non-Manifold Pattern Classifier 准备

> 文档状态：EXECUTED / R3-01 COMPLETE
> 日期：2026-07-21
> 前置：12E-08C-R2 COMPLETE

## 1. 原子目标

R3-01 只对最终变换后的 indexed triangle mesh 做确定性 non-manifold edge pattern 分类，并评估“是否存在
唯一、属性可保持的局部 fan split”。本任务不修改网格、不生成 repair operation、不写生产包。

## 2. 稳定模式

每条 incidence 大于 2 的 edge 必须恰有一个 primary pattern：

```text
duplicate_shell_or_exporter_duplicate；
separable_local_edge_fan；
overlapping_component；
mixed_winding_fan；
attribute_conflicting_fan；
unclassified。
```

分类优先级固定为 duplicate、attribute conflict、mixed winding、separable fan、overlapping component、
unclassified，避免相同输入因遍历顺序得到不同 primary pattern。

## 3. 结构证据

classifier 为每条 edge 输出：

```text
排序后的 edge vertex ids；
排序后的 incident output triangle ids 和 source triangle ids；
移除全部 non-manifold edge 后的 residual component ids；
正向/反向 edge use 数量；
duplicate、attribute conflict、mixed winding flags；
uniqueFanSplitFeasible；
primary pattern 与稳定 reason code。
```

residual component 仅通过 incidence=2 的 manifold edge 建立，不借助目标 non-manifold edge。只有全部 residual
fan group 均恰含两个 incident face、两面沿目标 edge 方向相反、属性无冲突且没有 duplicate 时，才允许
`uniqueFanSplitFeasible=true`。

## 4. 属性规则

material name、`has_uv` 或目标 edge 两端的 per-corner UV 任一不一致，分类为
`attribute_conflicting_fan`。本任务不生成 UV、不选择默认材质，也不把材质边界猜测为可拆 fan。

## 5. Aggregate 与 Eligibility

aggregate 记录每种 pattern 数量、non-manifold edge 总数、完整性和唯一 fan split 数量。只有所有
non-manifold edge 都是 `separable_local_edge_fan` 时，现有 eligibility 占位才映射为
`UniquelySeparable`；其他模式一律 `Ambiguous/manual_repair_required`。

闭合/no-non-manifold 输入输出 `status=not_present`、`complete=true`，不得修改 strict/no-op 结果。

## 6. 报告与体积

`mesh_repair_report` 增加 `nonManifoldAnalysis`，保留全部 edge 记录，按 edge key 排序。required real model
当前最大约 10940 条记录，可接受诊断 JSON 体积；production package 仍不包含该报告。

## 7. Fixture Matrix

```text
no non-manifold -> not_present；
重复 incident triangle -> duplicate exporter；
两个 residual fan pair -> separable local edge fan；
多个 residual patch 但不可唯一成对 -> overlapping component；
edge use 正反数量不平衡 -> mixed winding fan；
material/UV 冲突 -> attribute-conflicting fan；
单一 residual patch 且无其他可证模式 -> unclassified；
双运行顺序、计数和 report projection 完全一致。
```

## 8. 真实模型预期

`nai_you_new` 和闭合 Texture2D 3MF 应为 `not_present`；`aishen_fudiao`、`meigui_fudiao` 必须完整分类
全部 non-manifold edge，但允许诚实输出 ambiguous/manual。R3-01 不承诺 repair strict PASS。

## 9. 安全边界

repair 默认关闭；OpenVDB optional/OFF；legacy、Qt、TIFF writer、RGBWSV 协议不变；R3-01A、R3-02 和
12E-08D 不得提前执行。

## 10. 验证计划

```powershell
cmake --build build --config Debug --target mesh_non_manifold_pattern_classifier_unit_tests mesh_repair_contract_unit_tests mesh_repair_preflight
ctest --test-dir build -C Debug -R "mesh_(non_manifold_pattern|repair_(contract|preflight|r3_01))" --output-on-failure
.\scripts\run_12e_08c_r3_01_non_manifold_patterns.ps1 -BuildDir build -Config Debug
```

新增 target/脚本在实现时创建。完成前不得把计划命令记录为通过。

实际执行结果见
`DOC_EXEC_12E_08C_R3_01_NonManifoldPatternClassifier结果.md`。下一允许的原子任务为 R3-01A。
