# DOC_PREP_12E-08C-R2-04 Post-Repair Strict 与 Attribute Guard 准备

> 文档状态：PREPARED / R2-04 READY
> 日期：2026-07-20
> 前置：12E-08C-R2-03 COMPLETE

## 1. 原子目标

R2-04 不再新增 repair operation。它把 R2-01/02/03 已有候选统一送入独立 validator，证明“修复成功”必须
同时满足完整 post-strict、来源覆盖、属性保持、operation/hash 确定性和 non-production safety。

## 2. 必须固化的验证顺序

```text
ValidateOperationSequence；
ValidateSourceTriangleMappings；
ValidateVertexMappings；
ValidateGeneratedTriangleMappings；
ValidateMaterialAndUvPreservation；
RunCompletePostTopologyAndRobustness；
ComputeCanonicalPostHashes；
Classify repaired_strict_pass | manual_repair_required | repair_failed。
```

任何步骤失败都不得保留 `repaired_strict_pass`。validator 只读 candidate 和 evidence，不执行二次隐式修复。

## 3. Source/Vertex Mapping Gate

```text
每个原 source triangle 恰有一条 sourceMappings；
retained output index 唯一、有效并指向相同 material/UV；
removed disposition 必须有对应 operation；
每个原 source vertex 恰好被一个 vertexMappings source 集合覆盖；
output vertex index 连续、唯一且有效；
跨组件来源不得进入同一 weld mapping。
```

adapter-filtered degenerate 继续允许无 output index，但必须有稳定 source id 和 cleanup operation。

## 4. Generated Face Gate

```text
每个新增 output triangle 恰有一条 generatedTriangleMappings；
generated output 不得出现在 retained source output 中；
三项 generatingBoundaryVertexIndices 必须等于实际 triangle；
attributePolicy 必须属于显式白名单；
inherit_uniform_material_no_uv 要求 hasUv=false 且 materialName 与实际 attributes 一致；
newTriangles 与 mapping 数量一致；
unknown/fallback/material/uv conflict 必须为 0 才可 PASS。
```

R2-04 不新增纹理生成策略。带 UV boundary 继续 manual，不能通过伪造默认 UV 获得 PASS。

## 5. Post-Strict Gate

`postRepair.strictPass=true` 必须来自 repair 后重新运行的 strict diagnostics，并额外要求：

```text
self_intersection_check_sampled=false；
boundary/non-manifold/duplicate/opposite/winding/degenerate/confirmed intersection 全为 0；
component ownership 与 operation 解释一致；
attribute validator pass=true；
operationId 从 1 连续递增；
operation hash、post geometry hash、post attribute hash 双运行一致。
```

R2-04 仍固定 `productionAllowed=false` 和 `productionOutputWritten=false`。post strict PASS 只是 R3/08D 输入证据。

## 6. Negative Fixture Matrix

| Fixture | 必须结果 |
|---|---|
| cleanup/weld/winding/simple hole 正常候选 | validator PASS；若几何闭合则 repaired strict PASS |
| source mapping 缺失/重复 | `blocked_source_mapping` |
| vertex source 覆盖缺失/重复 | `blocked_vertex_mapping` |
| generated output 重复或越界 | `blocked_generated_mapping` |
| generated material/UV 与 policy 不符 | `blocked_generated_attribute` |
| operation id 跳号/重复 | `blocked_operation_sequence` |
| post diagnostics sampled | `blocked_incomplete_post_strict` |
| post strict 仍有 blocker | manual/failed，绝不 production |
| 重复运行 hash 不同 | `E_12E_REPAIR_HASH_NONDETERMINISTIC` |

## 7. 真实模型预期

R2-04 对 required real models 只验证证据完整性，不承诺修复：

```text
nai_you_new/aishen_fudiao：因 incomplete self-intersection evidence 保持 manual；
meigui_fudiao：因 non-manifold/opposite duplicate 保持 manual；
Texture2D 3MF：no-op strict PASS，所有 mapping/hash 稳定。
```

R2-04 不替代 R3-01 non-manifold classifier 或 R3-01A 完整自相交证据。

## 8. 实施边界

优先新增纯 core `MeshRepairEvidenceValidator.*`，由 `MeshRepairService` 在所有 operation set 完成后调用。
报告只扩展验证状态/issue，不改 schema id；若现有字段无法无歧义表达，再通过向后兼容字段扩展处理。

禁止：Qt、TIFF、pipeline router、OpenVDB 默认化、legacy 接入、生产 admission、通用 remeshing。

## 9. 计划验证

```powershell
cmake --build build --config Debug --target mesh_repair_cleanup_unit_tests mesh_repair_contract_unit_tests
ctest --test-dir build -C Debug -R "mesh_repair_(r2_04|r2_03|cleanup|contract|preflight)" --output-on-failure
.\scripts\run_12e_08c_r2_04_post_strict_evidence.ps1 -BuildDir build -Config Debug
```

脚本和新 target 在 R2-04 实现时创建；当前不得把计划命令写成已运行。

## 10. 准备结论

R2-04 输入、验证顺序、错误边界、negative matrix、真实模型预期和 non-production safety 已明确，状态 READY。
R3 和 12E-08D 继续阻断，必须等待 R2-04 实际实现和验证。
