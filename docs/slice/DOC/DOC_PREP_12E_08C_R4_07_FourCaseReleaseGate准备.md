# DOC_PREP_12E-08C-R4-07 Four-case Release Gate 准备

> 文档状态：DEPENDENCY PREPARED / WAIT REQUIRED FAMILY MATRIX 3/3
> 日期：2026-07-22
> 前置任务：R4-06 IMPLEMENTATION COMPLETE / REAL FAMILY MATRIX 0/3 BLOCKED
> 生产边界：只生成诊断、Release 与 legacy 回归证据，不写 global production package

## 1. 任务目标

R4-07 在三个 required family 均有 admitted candidate 后，执行四 case 的 strict/global/Release/legacy
守门验证，取代 R3-03 中三个真实 OBJ 因 topology 而 `skipped_due_topology` 的不完整证据。

本任务不修复模型、不选择候选、不改变生产协议，也不实现 12E-08D adapter。

## 2. 启动 Gate

启动前必须同时满足：

```text
required_aishen_family.requiredFamilyPassCount=1；
required_meigui_family.requiredFamilyPassCount=1；
required_titian_family.requiredFamilyPassCount=1；
三个 intake report 均 admitted=true；
三个 report 的 source/resource/geometry/attribute/audit hash 已冻结；
完整自相交 confirmed=0、coplanar=0、auditComplete=true；
post-strict PASS；
候选文件及外部纹理资源可读取；
工作树中的用户原始模型未被覆盖。
```

当前矩阵为 0/3，因此本文只完成准备，不执行 R4-07。

## 3. 四 Case 身份

| Case | 输入身份 | 选择规则 |
|---|---|---|
| `required_aishen_family` | 爱神 admitted candidate | 读取 R4-06 intake report，不硬编码文件名 |
| `required_meigui_family` | 玫瑰 admitted candidate | 读取 R4-06 intake report，不硬编码文件名 |
| `required_titian_family` | 梯田 admitted candidate | 读取 R4-06 intake report，不硬编码文件名 |
| `texture2d_3mf_control` | `samples/models/3mf/texture2d_checker_cube.3mf` | 已跟踪闭合 Texture2D 3MF 控制组 |

三个 required family 的候选可以是 strict PASS 原始、外部修复或独立重建，但进入矩阵后必须按 source hash
冻结。跨族 clean OBJ 不能替代任何 required case。

## 4. 每 Case 执行链

```text
1. 校验 R4-06 intake identity/hash/admitted；
2. Release fresh full preflight，禁止只复用旧 cache 结论；
3. global partition：minimum/intermediate/allTexture；
4. texture transfer：真实 UV/Texture2D，fallback 与 outsideColored 可解释；
5. classification-to-raster mapping；
6. full material closure：Texture/Fill/Support/Varnish；
7. Release core 分段计时与 peak working set；
8. legacy repair-disabled 切片、TIFF invariant、manifest 和 RIP strict；
9. 重复执行并比较确定性 hash；
10. 汇总 Gate，不写 global production package。
```

global 失败不得回退 legacy；legacy 成功不能替代 global case PASS。

## 5. 不变量

```text
TextureSurface ∩ ModelFill = empty；
TextureSurface ∪ ModelFill = Model；
minimum -> intermediate -> allTexture 单调；
allTexture 时 fill=0、texture=model；
raster overlap/unassigned/outside=0；
full closure expected-domain gap=0；
semantic channel mismatch=0；
support/varnish 不得以 not_evaluated 冒充 PASS；
productionOutputWritten=false。
```

## 6. Release 计时与预算冻结

使用默认 OpenVDB OFF、Release x64、同一台机器和同一构建目录。每 case 先 warm-up 1 次，再测量至少 3 次，
分别记录：

```text
importTransformMs；
preflightFullAuditMs；
partitionMs；
textureTransferMs；
rasterMappingMs；
fullClosureMs；
globalCoreMs；
peakWorkingSetBytes；
JSON/TIFF/PNG write time（单列，不计入 core budget）。
```

R4-07 先输出测量分布，再冻结预算文件；不得只取单次最好值。预算至少记录机器、编译器、构建类型、模型
hash、voxel/width 参数、样本数、median、max 和允许上限。若基线波动无法解释，则 R4-08 保持 NO-GO。

## 7. Legacy 与协议回归

```text
legacy 仍为默认生产模式；
repair 默认关闭；
schema=p0.rgbwsv.2；
channelOrder=R G B W S V；
bitDepth=8；
polarity=black_is_print；
printValue=0；emptyValue=255；
RIP Reader strict PASS；
repair-disabled TIFF invariant PASS；
Quick CI 已知 material_process_top2 baseline 必须得到显式解决，不得静默刷新 golden。
```

## 8. 计划输出

```text
scripts/run_12e_08c_r4_07_four_case_release_gate.ps1
tests/golden/expected/12e_r4_07_release_gate_expectations.json
output/benchmarks/12e_08c_r4_07_release_gate/four_case_summary.json
output/benchmarks/12e_08c_r4_07_release_gate/release_budget.json
docs/slice/DOC/DOC_EXEC_12E_08C_R4_07_FourCaseReleaseGate结果.md
```

汇总 schema 计划为 `slicesoft.r4_four_case_release_gate.12e_08c_r4.1`。生成 benchmark 不提交；expectation
只在输入 hash 与预算经审核后提交。

## 9. 验证命令

```powershell
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_12e_08c_r4_07_four_case_release_gate.ps1 -BuildDir build -Config Release
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_material_closure_tests.ps1 -BuildDir build -Config Release -Mode RepairDisabled
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_ci_quick.ps1
git diff --check
```

上述命令是 R4-07 执行计划，不是当前已通过记录。

## 10. 停止条件

```text
required family 少于 3/3 admitted；
任一候选 hash 与 intake 不一致；
完整审计 incomplete/sampled 或 post-strict 失败；
global 需要 silent fallback；
需要放宽 strict、修改 tolerance 或覆盖原始模型；
需要写 global production TIFF/package；
需要修改 RGBWSV 协议；
Quick CI 差异只能通过无依据更新 golden 解决。
```
