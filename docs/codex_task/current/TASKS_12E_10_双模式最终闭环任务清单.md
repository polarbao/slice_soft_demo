# TASKS 12E-10 双模式最终闭环任务清单

> 状态：10A/10B/10C/10D COMPLETE / STAGE 12E COMPLETE
> 日期：2026-08-03
> 规则：每次只执行用户明确授权的一个原子任务

## 1. 12E-10A 同层 Preview 最终一致性

状态：`COMPLETE / 2026-08-03`

目标：

```text
使用生产 TIFF 真源和 09A 诊断语义；
验证 Texture Surface / Model Fill / Partition / W/S/V 同层；
验证 layerIndex/zMm、raster、DPI 和像素统计；
不生成第二套生产预览文件。
```

完成 Gate：正向、缺证据、stale、跨层和非等方 DPI 测试通过。

完成证据：

```text
REPORT_12E_10A_同层Preview最终一致性当前状态.md；
Debug/Release 三项 core preview CTest PASS；
Debug/Release diagnostic-semantic-preview 与 material-closure-diagnostics smoke PASS；
精确闭环报告、生产 TIFF 和 09A 语义按 layerIndex/zMm 绑定。
```

## 2. 12E-10B 真实 OBJ/3MF 双模式矩阵

状态：`COMPLETE / 2026-08-03`

准备证据：`DOC_PREP_12E_10B_真实模型双模式矩阵准备.md`。

目标：

```text
xiao_ma、yecan 正向；
texture2d_checker_cube.3mf 格式控制；
aishen/meigui/titian BLOCKED_EXPECTED；
Legacy/Global 分开记录；
minimum/intermediate/allTexture 分开记录；
package/TIFF/report/RIP/no-fallback 证据完整。
```

完成 Gate：固定 required 行无遗漏，矩阵 schema 校验通过。

完成证据：

```text
scripts/run_12e_10b_final_closure_matrix.ps1；
REPORT_12E_10B_真实OBJ_3MF双模式矩阵当前状态.md；
xiao_ma/yecan Legacy/Global minimum/intermediate/all_texture 12/12 PASS；
Texture2D checker 3MF Legacy/Global 2/2 PASS；
aishen/meigui/titian 3/3 BLOCKED_EXPECTED；
RIP strict 14/14 PASS，fallback 0。
```

## 3. 12E-10C Release 性能与内存结论

状态：`COMPLETE / PASS / 2026-08-03`

目标：

```text
同参考机、同模型、同宽度、同 DPI、同层厚和同输出策略；
记录 core/compose/TIFF/preview-report/total；
记录 peak working set；
至少 3 次并使用中位数；
明确 Legacy 默认和 Global 候选结论。
```

完成 Gate：口径一致、原始摘要可追溯，不把 I/O 差异冒充核心引擎差异。

完成证据：

```text
scripts/run_12e_10c_release_performance.ps1；
REPORT_12E_10C_Release性能与内存当前状态.md；
36/36 计量样本与 RIP strict PASS，fallback 0；
Global/Legacy core 1.826x..2.562x；
Global/Legacy total 2.244x..3.161x；
Global/Legacy peak memory 3.079x..4.304x；
Legacy 默认、Global 显式候选结论保持不变。
```

## 4. 12E-10D 文档与阶段封口

状态：`COMPLETE / 2026-08-03`

目标：

```text
生成 REPORT_12E；
更新用户手册、README、总览、AGENTS 和上下文；
列出完成项、阻断、性能、协议、真实模型和后续路线；
把 12E 标为 COMPLETE 或记录真实 NO-GO。
```

完成 Gate：文档状态与实际验证一致，所有链接存在，`git diff --check` 通过。

完成证据：

```text
DOC_PREP_12E_10D_阶段封口准备.md；
REPORT_12E_全局纹理壳层与模型填充当前状态.md；
SLICE_12E_双模式纹理壳层与模型填充验收说明.md；
README、索引、任务看板、AGENTS 和项目上下文已同步。
```

## 5. 任务验证规则

每个任务开始和结束：

```powershell
git branch --show-current
git status --short
git diff --check
```

C++/Qt、Release、RIP 和脚本验证按 PRD/DEV/DEMO 与当前 `build-and-test.md` 执行。
