# DOC_PREP_12E-08C-R3-02 真实模型 Repair Matrix 准备

> 文档状态：EXECUTED / COMPLETE
> 日期：2026-07-21
> 前置任务：R1-01..04、R2-01..04、R3-01、R3-01A COMPLETE

## 1. 准备结论

R3-02 可以开始。R3-01A 已为四个 required case 生成完整、可重复的自相交证据，没有 budget blocked。
三个真实 OBJ 均为 `confirmed_intersection`，闭合 Texture2D 3MF 为 `complete_no_intersection`。因此本任务的
目标是形成诚实的真实模型 repair/no-op/manual/rejected matrix，而不是要求所有模型自动修复成功。

R3-02 完成只表示“真实模型矩阵证据完整”，不表示 required-case production Gate PASS。三个 OBJ 的确认
自相交仍会使 12E-08D 保持 BLOCKED。

## 2. Required Cases

固定使用 R1-04 effective config 与最终姿态：

| caseId | 输入 | R3-01A 基线 |
|---|---|---|
| `nai_you_new` | `model/obj/nai_you_new/MF_nai_you.obj` | confirmed intersection |
| `aishen_fudiao` | `model/obj/aishen_fudiao/MF_aishen_damuzhi_L_tx02.obj` | confirmed + coplanar intersection |
| `meigui_fudiao` | `model/obj/meigui_fudiao/04.obj` | confirmed intersection + non-manifold |
| `three_mf_texture2d_checker` | `samples/models/3mf/texture2d_checker_cube.3mf` | complete no intersection / strict no-op |

不得在任务中替换模型、改变姿态、缩放或放宽 tolerance 来获得更好结果。

## 3. Matrix Lanes

每个 case 至少运行以下显式 lane：

```text
strict_no_repair：repair disabled，complete self-intersection enabled；
conservative_repair：只启用 R2 已实现且默认关闭的 cleanup、weld、winding、simple boundary；
evidence_validation：对可接受候选运行统一 post-strict/attribute/hash validator；
non_manifold_classification：有 non-manifold 时复用 R3-01 只读分类证据。
```

confirmed/coplanar case 的 conservative lane 必须在 mutation 前 fail-fast，或返回未修改原始网格；不得尝试
启发式消除自相交。闭合 3MF 应保持 no-op strict PASS，不得为了“展示 repair”制造 operation。

## 4. 允许的操作集

只允许复用已经实现并有 generated fixture 覆盖的操作：

```text
remove_degenerate_face；
remove_exact_duplicate_face；
weld_vertex（受 tolerance、组件和属性约束）；
flip_triangle_winding（唯一可定向时）；
fill_simple_boundary_loop（平面、凸、预算内、属性唯一且完整相交证据通过时）。
```

禁止：

```text
自动切割/重建 confirmed self-intersection；
批量 non-manifold fan split；
删除带冲突属性的壳；
体素重建替代原始 UV/material provenance；
OpenVDB 自动 fallback；
写 production TIFF/package；
调整 strict 规则或错误码以制造 PASS。
```

R3-01 已证明 `aishen_fudiao` 和 `meigui_fudiao` 不满足全局唯一 fan split，因此 R3-02 只记录其分类和 blocker。

## 5. Matrix 输出字段

总表每个 case/lane 至少包含：

```text
caseId、lane、source/config/options hash；
pre diagnostics 与 complete self-intersection status/count/hash；
non-manifold classification 摘要；
eligibility、repairAttempted、operations 和 operation hash；
candidateAccepted、attribute preservation 和 evidence validation；
post strict complete/pass 与 blocker codes；
geometry/attribute hash 是否保持；
taskEvidenceStatus；
productionGateStatus；
productionOutputWritten=false；
两次运行 stable projection hash。
```

`taskEvidenceStatus` 与 `productionGateStatus` 必须分开：rejected/manual case 仍可完成矩阵证据，但不能计为
production PASS。

## 6. 预期状态

当前证据下的预期不是验收硬编码，而是安全边界：

```text
nai_you_new：rejected_self_intersection / production blocked；
aishen_fudiao：rejected_self_intersection / production blocked；
meigui_fudiao：rejected_self_intersection / production blocked；
Texture2D 3MF：strict_pass_no_repair / task evidence pass / production still non-production-only。
```

若实际结果偏离，必须先解释 evidence 变化，不得直接更新期望。

## 7. 实现计划

R3-02 原子任务内部按以下顺序执行：

```text
1. 先新增 matrix summary schema/golden 和脚本失败断言；
2. 复用 mesh_repair_preflight 组合显式 lane，不新增生产 app；
3. 若现有 CLI 无法表达 matrix options，只新增最小参数或固定 diagnostic preset；
4. 四 case 每 lane 运行两次并比较稳定投影；
5. 汇总 task evidence 与 production Gate，更新报告和上下文。
```

建议输出：

```text
output/benchmarks/12e_08c_r3_02_repair_matrix/repair_matrix_summary.json
```

建议 schema：

```text
slicesoft.mesh_repair_matrix.12e_08c_r3_02.1
```

## 8. 验证入口

目标脚本与 CTest 名称在实现时固化，预期入口：

```powershell
cmake --build build --config Debug --target mesh_repair_preflight mesh_repair_contract_unit_tests mesh_repair_preflight_unit_tests
ctest --test-dir build -C Debug -R "mesh_repair_(contract|preflight|r3_02)" --output-on-failure
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_12e_08c_r3_02_repair_matrix.ps1 -BuildDir build -Config Debug
```

在脚本创建和实际运行前，以上最后一条不得记录为已验证。

## 9. 完成标准

```text
四个 required case、全部规定 lane 有结果；
每个 lane 双运行稳定；
完整自相交证据始终 complete，或诚实记录新的稳定 budget/resource blocker；
confirmed/coplanar 输入没有 repair mutation；
闭合 3MF 保持 no-op strict PASS；
属性、mapping、hash 和 post-strict validator 结果可审计；
taskEvidenceStatus 与 productionGateStatus 分离；
repair 默认关闭；productionOutputWritten=false。
```

## 10. 停止条件

出现以下情况停止，不在 R3-02 内扩大范围：

```text
需要通用自相交重建；
需要非唯一 fan split；
需要第三方网格修复库；
需要修改最终姿态或模型输入；
需要写 global production TIFF；
需要改变 p0.rgbwsv.2 或 legacy 行为；
真实模型完整自相交分析重新进入 budget/resource blocked 且无法在既定预算内复现。
```

## 11. 后续关系

R3-02 完成后才允许判断 R3-03 是否 READY。即使矩阵证据完成，只要 required OBJ 仍是 confirmed
self-intersection，R3-03 可以做非生产 Release 诊断和 legacy regression，但 R3-04 必须输出 NO-GO，
12E-08D 仍不得启动。

## 12. 实际执行摘要

R3-02 已按本准备文档执行。四个 required case 均完成两条 lane、每条两次运行，稳定投影 8/8 通过：

```text
三个真实 OBJ：rejected_self_intersection，repairAttempted=false，operationCount=0；
Texture2D 3MF：strict_pass_no_repair，validator/attribute PASS；
task evidence：4/4 complete；
production Gate：0/4 pass；
productionOutputWritten=false。
```

实现和证据见 `DOC_EXEC_12E_08C_R3_02_真实模型RepairMatrix结果.md`；下一任务准备见
`DOC_PREP_12E_08C_R3_03_ReleaseCore与LegacyRegression准备.md`。
