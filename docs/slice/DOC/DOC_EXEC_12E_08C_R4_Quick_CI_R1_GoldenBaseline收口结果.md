# DOC_EXEC_12E-08C-R4 Quick-CI-R1 Golden Baseline 收口结果

> 文档状态：COMPLETE / PASS
> 完成时间：2026-07-23
> 基线提交：`ed9fafc`
> 任务性质：回归 Fixture 与真实用户 Profile 解耦

## 1. 结果摘要

Quick-CI-R1 已完成。`material_process_top2` 不再把真实爱神模型 Profile 当作固定尺寸 Golden Fixture，
而是使用独立的小型确定性 Fixture。真实用户 Profile、场景语义、切片核心、TIFF writer 和 RGBWSV
协议均未修改。

最终状态：

```text
Golden fixture identity: PASS
Deterministic output: PASS
Golden run #1: PASS
Golden run #2: PASS
Quick CI: PASS
Real user Profile unchanged: PASS
Production protocol unchanged: PASS
```

## 2. 根因与修复

历史 Golden 最初基于：

```text
samples/models/textured/fixtures/policy_textured_small.obj
grid=48 x 24 x 25
modelPixels=22,560
supportPixels=5,640
```

后续同名配置被调整为 UI 使用的真实爱神模型，但 Golden 期望仍保留小 Fixture 的固定结果，导致
Quick CI 把 `226 x 425 x 573` 的真实模型输出与 `48 x 24 x 25` 进行比较。

本次采用职责解耦，而不是刷新 Golden 掩盖漂移：

```text
真实用户 Profile：
  samples/configs/material_process/nail_rgb_white_varnish_top2.json
  保持指向 model/obj/aishen_fudiao/MF_aishen_damuzhi_L_tx02.obj

确定性 Golden Fixture：
  samples/configs/golden/material_process_top2_fixture.json
  指向 samples/models/textured/fixtures/policy_textured_small.obj
  独立输出 output/GoldenMaterialProcessTop2
```

`tests/golden/expected/r2_golden_summaries.json` 只切换 Fixture 配置与输出目录，固定期望值没有改变。

## 3. 修改范围

```text
新增：
  samples/configs/golden/material_process_top2_fixture.json

修改：
  tests/golden/expected/r2_golden_summaries.json

未修改：
  samples/configs/material_process/nail_rgb_white_varnish_top2.json
  samples/scenarios/slicer_scenarios.json
  src/slicer_core
  TIFF writer
  RIP reader
```

## 4. 验证结果

### 4.1 构建

```powershell
cmake --build build --config Debug --target slicer_cli
```

结果：PASS。

### 4.2 Fixture 直接切片

```powershell
.\build\Debug\slicer_cli.exe --config samples\configs\golden\material_process_top2_fixture.json
```

结果：

```text
packageDir=output/GoldenMaterialProcessTop2
grid=48 x 24 x 25
modelPixels=22,560
supportPixels=5,640
```

### 4.3 Golden 稳定性

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_golden_tests.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_golden_tests.ps1
```

结果：连续两次 PASS；`material_process_top2` 两次均得到相同尺寸和像素统计。

### 4.4 完整 Quick CI

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_ci_quick.ps1
```

结果：PASS，进程耗时约 `366.6 s`。覆盖 Debug 全量构建、support shape、quick regression、
Golden、Stage 10 输出合同、Qt self-test 和真实 overlay fixture；末尾输出 `CI quick complete.`。

## 5. 协议与产品边界

```text
manifest schema=p0.rgbwsv.2：未修改；
channel order=R G B W S V：未修改；
bitDepth=8：未修改；
polarity=black_is_print：未修改；
legacy 默认生产路径：未修改；
global_surface_shell：仍为 diagnostic-only；
productionAdmission：仍为 not_evaluated。
```

## 6. 后续准入

Quick-CI-R1 技术 Gate 已闭环，R4-08-R2 的执行依赖现已满足。下一步可以基于当前证据执行
R4-08-R2 GO/NO-GO 刷新；若全部技术 Gate 继续通过但用户尚未独立授权 12E-08D，决策必须是
`CONDITIONAL_TECHNICAL_PASS`，不能写成 `GO`。
