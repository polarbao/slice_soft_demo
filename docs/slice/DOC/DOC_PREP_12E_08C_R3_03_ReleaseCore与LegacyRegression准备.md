# DOC_PREP_12E-08C-R3-03 Release Core 与 Legacy Regression 准备

> 文档状态：CONSUMED / R3-03 COMPLETE / NON-PRODUCTION
> 日期：2026-07-21
> 前置任务：R3-02 COMPLETE

## 1. 准备结论

R3-03 已具备开始条件，但只能执行“非生产 Release 证据与 legacy 回归”。R3-02 已确认三个 required OBJ
存在完整 self-intersection blocker，因此它们不能进入 global partition/texture/raster/full-closure；闭合 3MF
可作为唯一 geometry-admitted global case。该限制不是测试缺口，而是 strict 安全结论。

R3-03 完成不表示 12E-08D GO。只要三个 required OBJ 未由外部人工修复后取得 strict PASS，R3-04 就必须
输出 NO-GO。

## 2. 固定输入与 Lane

继续使用 R3-02 的四个 required case、effective config、最终姿态和输入 hash，不允许替换模型或调整
tolerance。

```text
release_repair_evidence：Release 复跑完整 pre/repair/post 证据；
global_core：仅 geometry-admitted case 执行 partition/texture/raster/full closure；
legacy_regression：全部既有生产 Profile 走原 legacy 路径；
repair_disabled_invariant：repair OFF 前后生产 TIFF SHA-256 不变；
rip_strict：所有 legacy package 通过现有 RIP Reader 严格校验。
```

三个 OBJ 的 `global_core` 状态固定为 `skipped_due_topology`，不能通过 fallback 或 legacy 结果冒充 global
成功。3MF 即使 global core 通过，本阶段也不得写 global production package。

## 3. 计时边界

Release 报告必须分离：

```text
importTransformMs；
preDiagnosticsEligibilityMs；
repairCoreMs；
attributeValidationPostStrictMs；
partitionMs；
textureTransferMs；
rasterMappingMs；
fullClosureMs；
writeJsonMs；
writeTiffPreviewMs；
peakWorkingSetBytes。
```

核心性能只统计前八项中实际执行的计算阶段；JSON/TIFF/PNG 写盘不得混入 repair/global core 预算。
`skipped_due_topology` 的未执行阶段必须为 `null` 或明确 skipped，不能写 0 冒充执行耗时。

## 4. Legacy 与协议回归

至少验证：

```text
默认 repairEnabled=false；
legacy Profile effective config 不新增强制 repair；
repair disabled baseline/diagnostic package 的逐层 TIFF SHA-256 相同；
manifest schema=p0.rgbwsv.2；
channelOrder=R G B W S V；
bitDepth=8；polarity=black_is_print；
RIP Reader strict PASS；
OpenVDB 默认 OFF。
```

不得更新既有 golden 来掩盖回归。当前 Quick CI 的 `material_process_top2 widthPx=48/226` 差异必须单独
归因并形成显式 baseline 决策；R3-03 不得静默接受当前值。

## 5. 建议输出

```text
output/benchmarks/12e_08c_r3_03_release/release_core_summary.json
schema=slicesoft.mesh_repair_release_evidence.12e_08c_r3_03.1
```

每个 case 至少包含 source/options hash、repair matrix status、global core status、分段计时、peak memory、
legacy/protocol 结果、productionOutputWritten 和 blocker codes。

## 6. 验证计划

任务实施时固化具体脚本，最低入口为：

```powershell
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_12e_08c_r3_03_release_evidence.ps1 -BuildDir build -Config Release
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_material_closure_tests.ps1 -BuildDir build -Config Release -Mode RepairDisabled
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_ci_quick.ps1
```

最后一条若仍被已知 golden 阻断，必须记录失败和具体差异，不得声称全回归通过。

## 7. 完成标准

```text
四个 required case 有 Release repair evidence；
三个 OBJ global core 明确 skipped_due_topology；
3MF global core 的 partition/texture/raster/full closure 有分段证据；
核心计算与写盘耗时分离；
peak working set 可追踪；
legacy repair-disabled TIFF invariant 与 RIP strict 有实际结果；
所有失败和既有 golden blocker 被诚实记录；
productionOutputWritten=false；
R3-04 输入材料完整。
```

## 8. 停止条件

出现以下情况停止，不在 R3-03 扩大范围：

```text
需要绕过 confirmed/coplanar self-intersection；
需要新增通用网格重建或第三方修复库；
需要让 global 失败自动 fallback legacy；
需要写 global production TIFF/package；
需要修改 p0.rgbwsv.2、RGBWSV、uint8 或 black_is_print；
需要静默更新 legacy golden；
需要启用 OpenVDB 默认路径。
```

## 9. 后续关系

R3-03 完成后进入 R3-04 GO/NO-GO。按当前输入证据，R3-04 的预期是 NO-GO；只有三个真实 OBJ 被外部
修复并以同一 required-case 身份重新 strict PASS 后，才能重新评估 12E-08D。
