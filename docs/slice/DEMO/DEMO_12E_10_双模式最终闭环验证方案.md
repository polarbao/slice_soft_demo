# DEMO 12E-10 双模式最终闭环验证方案

> 文档版本：v1.1
> 文档状态：FORMAL / 10A COMPLETE / 10B READY
> 日期：2026-08-03

## 1. 验证目标

验证 Stage 12E 的诊断、生产、协议、真实模型和性能证据能在固定矩阵中一致闭环，并保持 Legacy
默认、Global 显式 opt-in、strict fail-closed 和 no-fallback。

## 2. 10A Preview

```text
同一 package 的首/中/末层；
minimum/intermediate/allTexture；
RGB、W、S、V、RGB+S+W+V；
Texture Surface、Model Fill、Partition；
六通道像素探针；
635/600 与 600/600 物理比例；
缺诊断证据、stale、尺寸不匹配和跨层负向。
```

10A 已实际覆盖：Debug/Release core CTest、同层语义 UI smoke、材料闭环报告 UI smoke、缺失/candidate/
跨层/zMm stale/scene stale/非等方 DPI。详细命令与结果见
`REPORT_12E_10A_同层Preview最终一致性当前状态.md`。

## 3. 10B 真实模型矩阵

| 模型族 | Legacy | Global | 预期 |
|---|---|---|---|
| xiao_ma | minimum/intermediate/allTexture | 同三点 | required PASS |
| yecan | minimum/intermediate/allTexture | 同三点 | required PASS |
| texture2d_checker_cube.3mf | 格式控制 | 已准入能力内执行 | PASS 或明确不适用 |
| aishen/meigui/titian | strict preflight | strict preflight | BLOCKED_EXPECTED |

每个生产成功 case 必须有 TIFF layer list、manifest、report、RIP strict、通道统计和 no-fallback。

## 4. 10C Release

```text
固定 referenceMachine；
Release runtime；
至少重复 3 次并记录中位数；
core/compose/TIFF/preview-report/total 分段；
peak working set；
Legacy 与 Global 使用相同模型、width、DPI、层厚和输出策略；
结果只形成当前参考机基线，不宣称跨设备 SLA。
```

## 5. 负向

```text
confirmed self-intersection；
Global Profile 不准入；
RIP schema/bitDepth/polarity 不匹配；
manifest layer 缺失；
diagnostic identity stale；
preview layerIndex 不存在；
runner 中断或输出目录不可写；
禁止 silent fallback。
```

## 6. 计划命令

```powershell
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
.\scripts\run_12e_10_final_closure.ps1 -BuildDir build -Config Release
.\runtime\slicesoft\Release\rip_reader_test.exe --package <package> --summary
.\runtime\slicesoft\Release\slicer_debug_ui.exe --self-test
.\scripts\run_ci_quick.ps1
git diff --check
```

实际执行时必须记录完整参数、referenceMachine、输出目录、重复次数和真实结果。

## 7. 通过标准

```text
required 正向行 PASS；
blocked 行按预期 fail-closed；
Texture/Fill overlap=0、unassigned=0；
同层 Preview 与统计一致；
RIP strict 无协议错误；
Legacy 默认不变；
Global 无 silent fallback；
最终 REPORT 明确剩余风险和下一阶段。
```
