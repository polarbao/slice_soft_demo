# DOC_PREP_12E-08C-R4-07 Four-case Release Gate 准备

> 文档状态：DEVELOPMENT GATE PASS / FINAL REQUIRED-FAMILY GATE PREPARED
> 日期：2026-07-22
> 前置任务：R4-06 IMPLEMENTATION COMPLETE；development intake 2/2；required family 0/3
> 生产边界：只生成 diagnostic、Release 开发测量与 legacy 回归证据，不写 global production package

## 1. 任务目标

R4-07 分为两个互不混淆的 Gate：

```text
Development Gate：使用 model 目录中通过 R4-06 intake 的资产，完成代码、四 case diagnostic、开发性能测量和 legacy 回归；
Final Required-family Gate：爱神、玫瑰、梯田各有一个 admitted 候选后，完成真实族 Release 验收和生产预算冻结。
```

本任务不修复模型、不改变生产协议，也不实现 12E-08D adapter。

## 2. 启动 Gate

### 2.1 Development Gate

至少一个 `development_model_pool` 候选必须满足：

```text
位于 model 目录；
R4-06 intake admitted=true；
完整审计 complete、strict PASS；
confirmedIntersectionPairs=0、coplanarOverlapPairs=0；
资源、几何、属性和 audit hash 可重复；
productionOutputWritten=false。
```

当前实际证据：

| Candidate | 模型 | Intake |
|---|---|---|
| `development_xiao_ma_damuzhi` | `model/obj/xiao_ma_wu_yu_new/MF_Xiao_ma_Damuzhi_ty02.obj` | ADMITTED |
| `development_yecan_3` | `model/obj/yecan/3.obj` | ADMITTED |

因此 Development Gate 已通过，R4-07 开发和 diagnostic 四 case 已执行。

### 2.2 Final Required-family Gate

最终验收前必须同时满足：

```text
required_aishen_family.requiredFamilyPassCount=1；
required_meigui_family.requiredFamilyPassCount=1；
required_titian_family.requiredFamilyPassCount=1；
三个 intake report 均 admitted=true；
source/resource/geometry/attribute/audit hash 已冻结；
完整自相交 confirmed=0、coplanar=0、auditComplete=true；
post-strict PASS；
候选文件及纹理资源可读取。
```

当前 required family matrix 为 `0/3`。它不再阻止开发，但继续阻止最终验收、生产预算冻结、R4-08 和
12E-08D。

## 3. 四 Case 身份

### 3.1 已执行的 Development Matrix

| Case | 输入 | 宽度场景 |
|---|---|---|
| `development_xiao_ma_minimum` | xiao_ma admitted candidate | minimum |
| `development_xiao_ma_all_texture` | xiao_ma admitted candidate | allTexture |
| `development_yecan_intermediate` | yecan admitted candidate | intermediate |
| `texture2d_3mf_control` | `samples/models/3mf/texture2d_checker_cube.3mf` | Texture2D 控制组 |

### 3.2 待执行的 Final Matrix

| Case | 输入身份 | 选择规则 |
|---|---|---|
| `required_aishen_family` | 爱神 admitted candidate | 读取 R4-06 intake report，不硬编码文件名 |
| `required_meigui_family` | 玫瑰 admitted candidate | 读取 R4-06 intake report，不硬编码文件名 |
| `required_titian_family` | 梯田 admitted candidate | 读取 R4-06 intake report，不硬编码文件名 |
| `texture2d_3mf_control` | 闭合 Texture2D 3MF | 已跟踪控制组 |

跨族 clean OBJ 可以用于 Development Matrix，但不能替代任何 required family case。

## 4. 每 Case 执行链

```text
1. 校验 R4-06 intake identity/hash/admitted；
2. Release fresh full preflight，禁止只复用旧 cache 结论；
3. global partition：minimum/intermediate/allTexture；
4. texture transfer：真实 UV/Texture2D；
5. classification-to-raster mapping；
6. full material closure：Texture/Fill/Support/Varnish；
7. warm-up 1 次并测量 3 次 core time 与 peak working set；
8. legacy repair-disabled TIFF invariant、manifest 和 RIP strict；
9. 汇总 Gate，不写 global production package。
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
productionOutputWritten=false。
```

## 6. Release 计时与预算边界

Development Matrix 使用默认 OpenVDB OFF、Release x64、同一构建目录；每 case warm-up 1 次、测量 3 次。
`globalCoreMs` 包含 partition、texture transfer、raster 和 full closure，不包含 JSON/TIFF/PNG 写盘。

当前测量只作为开发基线，不得冻结为生产预算。生产预算必须在 required family 3/3 后，记录机器、编译器、
构建类型、模型 hash、参数、样本数、median、max 和允许上限，并通过最终真实族矩阵复核。

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
repair-disabled TIFF invariant PASS。
```

## 8. 已实现输出

```text
scripts/run_12e_08c_r4_07_development_gate.ps1
tests/golden/expected/12e_r4_07_development_gate_expectations.json
output/benchmarks/12e_08c_r4_07_development_gate/four_case_development_summary.json
docs/slice/DOC/DOC_EXEC_12E_08C_R4_07_DevelopmentGate结果.md
```

Development summary schema 为 `slicesoft.r4_four_case_development_gate.12e_08c_r4.1`。benchmark 位于忽略
目录，不纳入源代码提交。

## 9. 验证命令

```powershell
cmake --build build --config Release --target repaired_asset_intake repaired_asset_intake_unit_tests
ctest --test-dir build -C Release -R "^repaired_asset_intake_unit_tests$" --output-on-failure
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_12e_08c_r4_06_repaired_asset_intake.ps1 -BuildDir build -Config Release -SkipBuild
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_12e_08c_r4_07_development_gate.ps1 -BuildDir build -Config Release
git diff --check
```

实际结果：R4-06 development intake `2/2 admitted`，R4-07 development four-case `4/4 PASS`，legacy
TIFF/RIP PASS。

## 10. 停止条件

Development Gate 停止条件：

```text
没有 development_model_pool admitted candidate；
任一开发候选 hash 与 intake 不一致；
完整审计 incomplete/sampled 或 post-strict 失败；
global 需要 silent fallback；
需要写 global production TIFF/package或修改 RGBWSV 协议。
```

Final Gate 停止条件：

```text
required family 少于 3/3 admitted；
真实族 four-case 任一失败；
生产预算未冻结；
legacy TIFF/RIP/Quick CI 未闭环；
用户尚未明确授权 12E-08D。
```
