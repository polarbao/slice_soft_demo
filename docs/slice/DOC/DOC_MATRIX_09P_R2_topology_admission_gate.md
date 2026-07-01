# DOC_MATRIX_09P_R2_topology_admission_gate

> 文档版本：v0.1
> 文档状态：Formal Gate Matrix / Stage 09P-R2-3
> 生成日期：2026-07-01
> 适用范围：experimental OpenVDB surface-shell pipeline 的 production admission hardening

---

## 1. 目的

本文件固定 09P-R2 的 topology admission gate 矩阵。

该矩阵只用于 experimental OpenVDB 路线的准入判断和 report 展示，不代表 legacy `slicer_cli` production path 被替换，也不允许 experimental path 写真实 OBJ/3MF production RGBWSV TIFF。

---

## 2. 稳定 Issue Code

09P-R2-3 必须覆盖以下 code：

| Code | 生产含义 |
|---|---|
| `MESH_BOUNDARY_EDGES` | Mesh 存在开放边界，不能视为严格闭合实体 |
| `MESH_SELF_INTERSECTION_CONFIRMED` | 已确认自相交，必须 fail fast |
| `MESH_NON_MANIFOLD_EDGES` | Mesh 存在非流形边，不能进入 strict production admission |
| `MESH_DUPLICATE_FACES` | Mesh 存在重复面，可能影响 SDF 或壳层归属 |
| `MESH_OPPOSITE_DUPLICATE_FACES` | Mesh 存在反向重复面，可能影响法线和内外判断 |
| `MESH_LOCAL_WINDING_INCONSISTENCY` | Mesh 局部绕序不一致，可能影响表面壳层和纹理转移 |
| `OPENVDB_UNAVAILABLE` | 当前构建或运行环境不可用 OpenVDB |
| `OPENVDB_LEVEL_SET_FAILED` | OpenVDB level set 生成失败 |

---

## 3. strict_closed Gate Matrix

| Code | status | productionAllowed | nonProduction | 说明 |
|---|---|---:|---:|---|
| `MESH_SELF_INTERSECTION_CONFIRMED` | `fail_fast` | false | false | 自相交确认后直接拒绝，不继续生成 non-production 输出 |
| `MESH_BOUNDARY_EDGES` | `non_production_only` | false | true | 保留 experimental diagnostic 输出 |
| `MESH_NON_MANIFOLD_EDGES` | `non_production_only` | false | true | 保留 experimental diagnostic 输出 |
| `MESH_DUPLICATE_FACES` | `non_production_only` | false | true | 保留 experimental diagnostic 输出 |
| `MESH_OPPOSITE_DUPLICATE_FACES` | `non_production_only` | false | true | 保留 experimental diagnostic 输出 |
| `MESH_LOCAL_WINDING_INCONSISTENCY` | `non_production_only` | false | true | 保留 experimental diagnostic 输出 |
| `OPENVDB_UNAVAILABLE` | `non_production_only` | false | true | 不影响 legacy path，但 OpenVDB experimental path 不可生产 |
| `OPENVDB_LEVEL_SET_FAILED` | `non_production_only` | false | true | 保留失败诊断，不写 production package |

当没有 blocker 时，`strict_closed` 可以返回 `production_allowed`；但在 09P-R2 experimental CLI 中仍必须受 `writeProductionRgbwsv=false` 和 `productionPackageWritten=false` 约束。

---

## 4. warn_and_attempt Matrix

| 输入 issue | status | productionAllowed | nonProduction | 说明 |
|---|---|---:|---:|---|
| 任意 09P-R2-3 gate code | `non_production_only` | false | true | 允许继续 experimental report / preview / benchmark，不允许 production package |

`warn_and_attempt` 永远不能被视为 production-safe。

---

## 5. diagnostic_only Matrix

| 输入 issue | status | productionAllowed | nonProduction | 说明 |
|---|---|---:|---:|---|
| 任意 09P-R2-3 gate code | `diagnostic_only` | false | true | 只输出诊断信息，不进入生产候选 |

---

## 6. repair_then_strict Placeholder Matrix

| 输入 issue | status | productionAllowed | nonProduction | 说明 |
|---|---|---:|---:|---|
| 任意 09P-R2-3 gate code | `non_production_only` | false | true | 09P-R2 不实现 mesh repair；repair 完成并重新 strict 诊断前不得 productionAllowed |

后续若实现 repair，必须补充 repair report、修复前后 hash、重新诊断结果，并且只能在重新通过 `strict_closed` 后考虑生产候选。

---

## 7. 验证入口

单测目标：

```powershell
cmake --build build --config Debug --target production_admission_policy_unit_tests
.\build\Debug\production_admission_policy_unit_tests.exe
```

完整回归：

```powershell
ctest --test-dir build -C Debug --output-on-failure
```

对应测试文件：

```text
tests/unit/production_admission_policy/main.cpp
```

