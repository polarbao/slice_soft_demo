# DOC_EXEC_12E-08C-R3-01 Non-Manifold Pattern Classifier 结果

> 文档状态：COMPLETE / NON-PRODUCTION
> 日期：2026-07-21
> 前置：12E-08C-R2 COMPLETE

## 1. 完成范围

新增只读的 `MeshNonManifoldPatternClassifier`，不执行 mesh mutation、fan split、修复操作或生产输出。
分类器建立 canonical edge incidence map，并用仅经 incidence=2 manifold edge 相连的 residual
triangle component 证据分析每个 non-manifold edge。

固定主分类和优先级为：

```text
duplicate_shell_or_exporter_duplicate；
attribute_conflicting_fan；
mixed_winding_fan；
separable_local_edge_fan；
overlapping_component；
unclassified。
```

edge evidence 包含边端点、incident output/source triangle、residual component、方向计数、冲突标记、
reason code 与 unique fan split 可行性。只有每个 residual fan group 都恰好包含两个相反方向
edge use，且无重复几何和属性冲突时，才允许 `uniqueFanSplitFeasible=true`。

## 2. 契约与入口

```text
MeshRepairOptions.classifyNonManifoldPatterns=false；
MeshRepairResult.nonManifoldAnalysis；
report.nonManifoldAnalysis；
mesh_repair_preflight --classify-r3-01；
scripts/run_12e_08c_r3_01_non_manifold_patterns.ps1。
```

新选项已纳入 canonical options hash。报告 schema id 仍为 `slicesoft.mesh_repair.12e_08c.1`，
golden 固定默认 `not_evaluated` 结构。显式开启分类器时，preflight 仅把全部唯一可分 fan 映射为
`UniquelySeparable`，其他 non-manifold 证据仍为 `Ambiguous`。

## 3. 单元与负向覆盖

```text
无 non-manifold edge；
exporter duplicate；
separable local fan；
overlapping component；
mixed winding；
material conflict；
UV conflict；
unclassified single residual patch；
attribute count mismatch 稳定错误码；
边顺序和 source evidence 重复性。
```

## 4. 真实模型证据

| Case | Non-Manifold Edges | 主要模式 | Unique Fan Split | Repeatability |
|---|---:|---|---|---|
| `nai_you_new` | 0 | `not_present` | false | PASS |
| `aishen_fudiao` | 59 | duplicate exporter=2，attribute conflict=57 | false | PASS |
| `meigui_fudiao` | 10940 | duplicate exporter=10935，attribute conflict=5 | false | PASS |
| Texture2D 3MF | 0 | `not_present` | false | PASS |

四个 required case 各运行两次，稳定投影一致；修复未执行，生产输出未写入。两个具有
non-manifold edge 的真实模型均不满足全局唯一 fan split，因此不允许无模式批量修复。

证据摘要：
`output/benchmarks/12e_08c_r3_01_non_manifold/non_manifold_summary.json`。

## 5. 验证记录

已执行：

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\run_12e_08c_r3_01_non_manifold_patterns.ps1 -BuildDir build -Config Debug
```

实际结果：

```text
Debug 全量构建：PASS；
Debug CTest：34/34 PASS；
Qt --self-test：startup、experimental-report-summary PASS；
真实模型证据：4/4 case、4/4 repeatability PASS；
classifier repairAttempted=false，productionOutputWritten=false。
```

`run_ci_quick.ps1` 本任务未重复执行。R2-04 已记录其在既有 `material_process_top2`
widthPx golden 基线处失败（expected=48，actual=226）；该已知阻断与本任务只读 mesh classifier
无关，本任务没有修改 legacy 切片尺寸、Profile 或 TIFF writer。

## 6. 安全边界与下一任务

```text
repair 默认关闭；
classifier 默认关闭；
OpenVDB optional/OFF；
legacy、Qt、TIFF writer 未接入；
p0.rgbwsv.2 / R G B W S V / uint8 / black_is_print 不变；
12E-08D 继续 BLOCKED。
```

下一允许的原子任务为 R3-01A 完整自相交证据。R3-02 必须等待 R3-01A 完成后才能开始。
