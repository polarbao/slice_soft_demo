# DOC_EXEC_12E-08D-04 显式 Profile 与 Release Matrix 结果

> 状态：COMPLETE
> 日期：2026-07-23
> 范围：受限 Global Surface Shell production Profile、真实模型 Release package、RIP/no-fallback Gate

## 1. 结论

12E-08D-04 已完成。项目首次具备一个明确、可审计、fail-closed 的
`global_surface_shell_restricted_candidate` 生产 Profile，并通过两个独立真实 OBJ 模型族的 Release
写包与 RIP Reader 校验。

本结论必须分成两层：

```text
受限 Profile production admission：GO；
普通 Global 全功能等价：NO-GO。
```

受限 Profile 只允许纹理 RGB 与白墨 Model Fill，明确关闭支撑、表面光油、外侧光油和材料闭环修复。
它不会在不支持的工艺配置下写包，也不会静默回退 legacy。

## 2. 显式 Profile 合同

唯一已准入目标：

```text
materialProcessProfile.target =
  global_surface_shell_restricted_candidate
```

必要条件：

```text
slicePipeline.mode = global_surface_shell，且必须显式声明；
texture.applyMode = global_surface_shell；
texture.surfaceShell.geometryMode = global_3d_distance；
texture.surfaceShell.surfaceScope = all_closed_surfaces；
modelFill.enabled = true；
modelFill.material = white；
modelFill.scope = complement_of_global_texture_shell；
modelFill.value = 0；
support.enabled = false；
surfaceVarnish.enabled = false；
outerVarnish.enabled = false，thicknessMm = 0；
materialClosure.repair.enabled = false；
legacy materialPolicy/materialRoleMapping 不得混入；
纹理壳层宽度不得低于 two-cell minimum。
```

任何条件不满足均返回 `E_12E_PIPELINE_GLOBAL_NOT_ADMITTED`，并且不生成 production package。

## 3. 实现链路

```text
RunSlicePipeline
  -> ModelPreflightPipelineGate(global_surface_shell)
  -> EvaluateGlobalSurfaceShellProductionProfile
  -> RunTextureFillPartitionReleaseBenchmark
  -> production-resolution raster mapping
  -> full material closure evidence
  -> GlobalSurfaceShellProductionLayerAdapter
  -> shared RGBWSV package writer
  -> staging RIP strict validation
  -> atomic publish
```

本阶段未新增 TIFF encoder。Legacy 与 Global 继续使用 08D-03 的共享 writer。

CLI timing 中的 `engine` 已改为使用实际 `SliceRunResult.effective_pipeline_mode`，因此 Global 正向 case
稳定输出：

```text
SLICE_TIMING engine=global_surface_shell
```

## 4. Release 真实模型矩阵

固定配置：

```text
buildType = Release；
dpi = 600 x 600；
layerThicknessMm = 0.2；
storageMode = stripped；
preview = PNG / interval 5；
OpenVDB = OFF；
Model Fill = W=0；
S/V = disabled。
```

结果：

| Case | 模型族 | Grid | RGB 打印像素 | W 打印像素 | S/V | Slice | Output | Total | RIP |
|---|---|---:|---:|---:|---:|---:|---:|---:|---|
| xiao_ma | xiao_ma_wu_yu_new | 298 x 563 x 30 | 428431 / 428431 / 428506 | 5028 | 0 / 0 | 804.935 ms | 1228.240 ms | 2097.113 ms | PASS |
| yecan | yecan | 293 x 719 x 31 | 412820 / 442249 / 442462 | 87546 | 0 / 0 | 820.395 ms | 1416.068 ms | 2306.583 ms | PASS |

机器可读结果：

```text
output/benchmarks/12e_08d_04_global_production/
  global_production_matrix_summary.json
```

输出目录属于验证产物，不提交 Git。

## 5. 协议与负向门禁

两个正向 package 均满足：

```text
schema = p0.rgbwsv.2；
channelOrder = R G B W S V；
bitDepth = 8；
polarity = black_is_print；
printValue = 0；
emptyValue = 255；
requestedPipelineMode = effectivePipelineMode = global_surface_shell；
productionAcceptance = admitted；
productionOutputWritten = true；
fallbackApplied = false；
TIFF layer list 完整；
RIP Reader strict PASS。
```

负向 `support.enabled=true`：

```text
返回 E_12E_PIPELINE_GLOBAL_NOT_ADMITTED；
productionOutputWritten = false；
目标 package 不存在；
未执行 legacy fallback。
```

Legacy Repair Disabled TIFF/RIP 回归通过。

## 6. GO/NO-GO

### 6.1 GO

以下组合可以写正式 RGBWSV package：

```text
显式 global_surface_shell；
strict preflight PASS；
受限候选 Profile；
纹理 RGB；
白墨 Model Fill；
无支撑；
无光油；
0.2 mm Release 候选层厚。
```

### 6.2 NO-GO

以下能力仍不能冒充已完成：

```text
Global 支撑生成与 S 通道工艺等价；
Global 表面/外侧光油生成；
0.01 mm 最终层厚真实模型 Release 矩阵；
爱神/玫瑰/梯田复杂自相交模型的 strict production admission；
普通 UI 中不受约束的 Global 全功能入口。
```

因此 12E-09B 可以只暴露“受限 Global Profile”并锁定不支持项；不能把它描述为与 legacy
全部材料能力等价。

## 7. 验证入口

```powershell
cmake --build build --config Release --target slicer_cli rip_reader_test global_surface_shell_production_pipeline_unit_tests rgbwsv_production_package_writer_unit_tests slice_pipeline_router_unit_tests
.\scripts\run_12e_08d_04_global_production_matrix.ps1 -SkipBuild
```

脚本同时执行定向单测、两个真实模型 production package、RIP strict、负向 no-package 和 legacy
Repair Disabled 回归。

## 8. 最终验证记录

2026-07-23 本机验证：

```text
Debug full build：PASS；
Debug CTest：49/49 PASS；
Release 08D-04 定向 CTest：3/3 PASS；
Release xiao_ma/yecan production matrix：2/2 PASS；
RIP Reader strict：2/2 PASS；
support-enabled no-package：PASS；
Legacy Repair Disabled TIFF SHA-256 invariant：PASS；
scripts/run_ci_quick.ps1：PASS。
```
