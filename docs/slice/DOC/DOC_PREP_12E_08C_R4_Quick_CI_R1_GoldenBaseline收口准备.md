# DOC_PREP_12E-08C-R4 Quick-CI-R1 Golden Baseline 收口准备

> 文档状态：READY FOR DEVELOPMENT
> 准备完成时间：2026-07-23
> 前置任务：R4-07-R2 COMPLETE / candidate budget FROZEN PASS
> 任务性质：回归 fixture 与用户 Profile 解耦；不修改生产切片语义

## 1. 当前问题

当前 `scripts/run_ci_quick.ps1` 在 `scripts/run_golden_tests.ps1` 的 `material_process_top2` 用例失败：

```text
expected widthPx=48
actual widthPx=226
```

2026-07-23 当前工作树实际复现：

```text
grid=226 x 425 x 573
modelPixels=7,055,867
supportPixels=20,915,992
Debug totalMs=48,343.879
```

失败不是 R4-07-R2 引入的算法回退，而是 golden 用例身份发生了历史漂移。

## 2. 根因证据

### 2.1 Golden 初始身份

提交 `a8159e5` 首次引入 `r2_golden_summaries.json` 时，
`material_process_top2` 使用小型确定性 fixture：

```text
samples/models/textured/fixtures/policy_textured_small.obj
expected grid=48 x 24 x 25
expected modelPixels=22,560
expected supportPixels=5,640
```

### 2.2 用户 Profile 替换

提交 `5ec2bdf` 为 UI 默认甲片样例，把同一配置文件的模型改成真实甲片，并增加缩放、支撑要求和 preview；
提交 `4f3f4bd` 又把路径调整为当前真实爱神模型：

```text
model/obj/aishen_fudiao/MF_aishen_damuzhi_L_tx02.obj
```

但 `tests/golden/expected/r2_golden_summaries.json` 仍保留小 fixture 的 48 x 24 x 25 期望。结果是：

```text
同一个 JSON 同时承担“用户真实 Profile”和“快速确定性 Golden Fixture”；
Profile 合理变化会无条件破坏 Quick CI；
Quick CI 每次先切 573 层真实模型，运行时间也被放大。
```

## 3. 处理决策

Quick-CI-R1 不直接把 golden 更新为 `226 x 425 x 573`，也不把真实用户 Profile 改回小模型。正确做法是解耦：

```text
用户 Profile：
  保留 samples/configs/material_process/nail_rgb_white_varnish_top2.json；
  继续面向真实甲片和 UI 场景；
  不进入固定尺寸的 quick golden。

Golden Fixture：
  新增 samples/configs/golden/material_process_top2_fixture.json；
  使用 policy_textured_small.obj；
  使用独立 output/GoldenMaterialProcessTop2；
  r2_golden_summaries.json 只指向该 fixture。
```

这样既保留真实模型 UI 行为，也恢复可重复、快速、可审计的 Golden Gate。

## 4. 实施文件

计划修改：

```text
samples/configs/golden/material_process_top2_fixture.json
tests/golden/expected/r2_golden_summaries.json
docs/slice/DOC/DOC_EXEC_12E_08C_R4_Quick_CI_R1_GoldenBaseline收口结果.md
docs/codex_task/current/TASKS_12E_08C_R4_模型导入预检与修复资产准入任务清单.md
```

不修改：

```text
samples/configs/material_process/nail_rgb_white_varnish_top2.json
samples/scenarios/slicer_scenarios.json 中真实用户场景语义
slicer_core
TIFF writer
RGBWSV 协议
```

## 5. 验收标准

```text
1. Golden fixture 所有资源均为 Git tracked；
2. fixture 两次运行的 grid/modelPixels/supportPixels 完全一致；
3. material_process_top2 golden 恢复 48 x 24 x 25，或在当前代码下出现差异时先归因再审查期望；
4. 真实 nail_rgb_white_varnish_top2 Profile 仍指向真实爱神模型；
5. scripts/run_golden_tests.ps1 PASS；
6. scripts/run_ci_quick.ps1 PASS；
7. 不通过改成真实模型当前输出值来掩盖 fixture 身份漂移；
8. 不修改 p0.rgbwsv.2、通道顺序、位深和极性。
```

## 6. 停止条件

```text
小 fixture 在当前代码下不能稳定复现；
需要修改 slicer_core 才能让旧期望通过；
真实 Profile 行为必须被回退；
需要修改生产 TIFF 或材料优先级；
Quick CI 出现新的独立失败。
```

遇到上述情况必须停止，将新失败作为独立问题归因，不得批量刷新 golden。

## 7. 验证命令

```powershell
cmake --build build --config Debug --target slicer_cli
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_golden_tests.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_golden_tests.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_ci_quick.ps1
git diff --check
git status --short
```

## 8. 准备结论

根因、文件边界、fixture 身份、验收标准、停止条件和验证命令已明确。Quick-CI-R1 可进入独立开发；
在其 PASS 前，R4-08-R2 只能保持“结构准备完成、执行依赖未满足”。
