# DEMO_12E-08C 真实模型拓扑修复验证方案

> 文档状态：DEMO / PREPARED
> 日期：2026-07-20
> 对应 PRD/DEV：`PRD_12E_08C_真实模型拓扑修复与严格准入.md`、`DEV_12E_08C_MeshRepairThenStrict设计.md`

## 1. 验证目标

证明修复服务满足“显式、确定、属性保持、修复后重新 strict、失败不写包”。截图或单一问题数量下降不能替代
post-repair strict、attribute、hash 和 12E 全链路证据。

## 2. Generated Fixture Matrix

| Fixture | 预期 |
|---|---|
| closed clean mesh | `strict_pass_no_repair`，hash 不变 |
| degenerate face | 显式移除，post strict PASS，operation 可追溯 |
| exact duplicate same attributes | 去重，attribute PASS |
| duplicate with conflicting material/UV | `manual_repair_required` 或 attribute conflict |
| opposite duplicate ambiguous | blocked，不猜测内外 |
| local winding uniquely orientable | 修正后 strict PASS |
| simple planar boundary loop | 阈值内 fill 后 strict PASS |
| non-planar/oversized boundary | budget/eligibility blocked |
| local separable non-manifold fan | 分解后 strict PASS，组件可解释 |
| ambiguous non-manifold | manual required |
| confirmed self-intersection | fail-fast，repairAttempted=false |
| multi-component input | 不静默 merge；按 policy 明确结果 |

## 3. Attribute Matrix

| 输入 | 必须验证 |
|---|---|
| OBJ/MTL/PNG | material、per-corner UV、texture path/resource 和 triangle provenance |
| 3MF Texture2DGroup | texture group/property index 与 UV 保持 |
| 3MF ColorGroup/BaseMaterial | triangle material property 保持 |
| 新增 hole-fill face | 明确 fallback policy；未知属性不得 production |

纹理验证至少比较 repair 前后未受影响 surface sample 的 RGB 和 source triangle identity。

## 4. 真实模型矩阵

| Case | 首轮诊断 | R2/R3 验证目标 |
|---|---|---|
| `nai_you_new` | boundary=113 | loop 数、周长、平面性、修复资格和 post strict |
| `aishen_fudiao` | boundary=3、nonManifold=59 | issue 来源、局部可分解性、属性保持和 post strict |
| `meigui_fudiao` | nonManifold=10940 | 模式分类；无安全模式时稳定 manual required |
| 3MF Texture2D fixture | closed | no-op strict，纹理属性与 hash 稳定 |

每个模型记录输入和配置 SHA-256、最终姿态、pre/post diagnostics、operation hash、结果状态、核心时间和峰值内存。
三个真实 OBJ 的 R1-04 baseline 已出现 sampled self-intersection evidence；R3-01A 必须对 required cases
输出完整候选枚举或稳定 budget blocked，sampled 不能作为 post-strict PASS。

## 5. Repeatability

相同二进制、输入和配置连续运行至少三次，要求：

```text
status 相同；
operation list/hash 相同；
post geometry/attribute hash 相同；
post topology counts 相同；
partition/texture/raster counts 相同。
```

性能允许波动，但必须报告样本数、median/p95 或等价统计，不能以单次 Debug 结果冻结预算。

## 6. Negative Gates

必须覆盖：

```text
repair disabled；
unsupported mode；
self-intersection；
attribute conflict；
budget exceeded；
post strict still blocked；
hash nondeterministic；
OpenVDB unavailable 不影响默认 OFF repair diagnostics；
任一失败 productionOutputWritten=false。
```

## 7. 12E End-to-End Gate

只有 repaired strict PASS case 继续执行：

```text
global partition overlap=0/unassigned=0；
texture transfer outsideColored=0；
raster mapping invariants PASS；
full material closure PASS；
RIP strict PASS（仅未来 08D candidate）；
legacy Profile regression PASS。
```

R1/R2 修复专项本身不写 production package。R3 可生成 diagnostic evidence，不得绕过 08D admission。

## 8. Release 性能

独立记录：加载、适配、pre diagnostics、repair、post diagnostics、partition、texture transfer、raster mapping、
full closure 和写盘。准入预算只使用核心阶段，不混入 TIFF/PNG/JSON 保存时间。

## 9. 计划验证命令

具体 target 创建后再固化实际路径，预期验证类别为：

```powershell
cmake --build build --config Debug --target <mesh-repair-unit-target>
ctest --test-dir build -C Debug -R "mesh_repair|production_admission" --output-on-failure
cmake --build build --config Release --target <mesh-repair-real-model-target>
.\scripts\run_12e_08c_repair_evidence.ps1 -BuildDir build -Config Release
.\scripts\run_12e_08c_release_evidence.ps1 -BuildDir build -Config Release
```

以上是计划命令，文件/target 未创建前不得记录为已运行。

### 9.1 R2-01 已固化命令

```powershell
cmake --build build --config Debug --target mesh_repair_preflight mesh_repair_cleanup_unit_tests
ctest --test-dir build -C Debug -R "mesh_repair_(cleanup|preflight)" --output-on-failure
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_12e_08c_r2_cleanup_evidence.ps1 -BuildDir build -Config Debug
```

证据脚本缺少 R1 effective config 时自动生成 baseline。四个 case 各执行两次，只写诊断 JSON，断言默认
OpenVDB OFF、operation 范围、source mapping、post diagnostics 和 stable projection，不写 TIFF/package。

### 9.2 R2-02 已固化命令

```powershell
cmake --build build --config Debug --target mesh_repair_preflight mesh_repair_cleanup_unit_tests mesh_repair_contract_unit_tests
ctest --test-dir build -C Debug -R "mesh_repair_(r2_02|cleanup|contract|preflight)" --output-on-failure
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_12e_08c_r2_02_topology_evidence.ps1 -BuildDir build -Config Debug
```

generated fixture 覆盖 safe weld、跨组件近邻、退化阻断、唯一 winding 和 non-orientable 歧义。真实模型
逐 case 双运行，组件数、vertex mapping 和 stable projection 一致；本任务不要求真实 OBJ 获得 strict PASS。

## 10. 退出标准

```text
R1：Schema、hash、eligibility、generated diagnostics 可复现；
R2：保守修复和属性保护 fixture 通过；
R3：所有真实模型得到可审计结果，Release matrix 可复现；
08D：只有 required cases 全部 production gate PASS 后才能开始。
```
