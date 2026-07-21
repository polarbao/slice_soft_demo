# DEV_12E-08C MeshRepairThenStrict 设计

> 文档版本：v0.1
> 文档状态：DEV / R1、R2 IMPLEMENTED
> 日期：2026-07-20
> 对应 PRD：`PRD_12E_08C_真实模型拓扑修复与严格准入.md`

## 1. 技术目标

新增与切片算法解耦的、显式且可审计的 Mesh Repair 服务。服务只接受最终变换后的核心 SceneModel/mesh DTO，
输出修复候选、属性映射、诊断和 admission evidence，不写 TIFF、不依赖 Qt，也不在内部决定生产发布。

## 2. 架构位置

```text
SceneModel + final transform
  -> SceneModelTriangleMeshAdapter
  -> PreRepairTopologyDiagnostics
  -> MeshRepairEligibilityPolicy
  -> MeshRepairService (explicit only)
  -> PostRepairTopologyDiagnostics
  -> MeshAttributePreservationValidator
  -> EvaluateProductionAdmission(strict_closed)
  -> 12E partition/transfer/raster/closure
  -> 12E-08D production policy (later)
```

依赖方向：

```text
geometry/repair -> scene DTO + geometry diagnostics
diagnostics/report -> repair result DTO
pipeline -> repair facade + partition services
apps -> public facade
```

禁止：

```text
geometry/repair -> Qt/TIFF/report writer
report writer -> repair decision
repair service -> production package publish
OpenVDB internal type -> public repair API
```

R2-01 已实现隔离 candidate、退化面/同属性同向 exact duplicate cleanup 和 `sourceMappings[]`。R2-02 已实现
受约束 vertex weld、唯一 local winding 传播、组件守门与 `vertexMappings[]`。opposite duplicate、属性冲突和
confirmed self-intersection 均不会被这些 operation set 绕过；复杂 boundary 和 non-manifold 仍受后续原子 Gate 阻断。

R2-03 已进一步实现简单、平面、严格凸、显式预算内的 boundary fill，只允许
`inherit_uniform_material_no_uv`，并通过 `generatedTriangleMappings[]` 将新面与原 source mapping 分离。
复杂边界、sampled intersection evidence、UV/材质冲突继续 manual；non-manifold 仍不在 R2 范围内修复。

R2-04 已实现独立、只读的 `MeshRepairEvidenceValidator`。validator 不复用 repair 阶段的“成功”判断，而是按
固定顺序重新验证 operation sequence、source/vertex/generated provenance、material/UV、完整 post-strict、
canonical hash 与 non-production safety；任一 Gate 失败都丢弃 candidate，禁止保留
`repaired_strict_pass`。该 validator 默认关闭，仅由显式 R2-04 诊断入口启用。

## 3. 建议目录

```text
src/slicer_core/geometry/repair/
  MeshRepairTypes.*
  MeshRepairEligibilityPolicy.*
  MeshRepairService.*
  MeshRepairHash.*
  MeshAttributePreservationValidator.*

src/slicer_core/diagnostics/
  MeshRepairReport.*
```

最终目录应服从现有 CMake 和模块边界；不为目录美观大范围移动历史代码。

## 4. 核心 DTO

```text
MeshRepairOptions
  enabled
  mode = repair_then_strict
  positionToleranceMm
  maxBoundaryLoopEdges
  maxBoundaryPerimeterMm
  maxHoleAreaMm2
  maxAffectedFaceRatio
  allowVertexWeld
  allowDuplicateRemoval
  allowWindingRepair
  allowBoundaryFill
  allowNonManifoldSplit
  newFaceAttributePolicy

MeshRepairEligibility
  overallStatus
  issueDecisions[]
  blockerCodes[]
  suggestedActions[]

MeshRepairOperation
  operationId
  type
  reasonCode
  inputElementIds[]
  outputElementIds[]
  parameters
  attributeDecision

MeshRepairResult
  status
  repairedMesh
  sourceToOutputMapping
  operations[]
  preDiagnostics
  postDiagnostics
  attributeValidation
  hashes
  performance
  issues[]
```

公共 API 使用 STL 和项目 DTO；Public 接口按项目规范补充 Doxygen。

## 5. 诊断和资格分层

### 5.1 Fail Fast

```text
MESH_SELF_INTERSECTION_CONFIRMED
输入索引越界或 NaN/Inf
无法构建确定性 source mapping
```

### 5.2 首版保守候选

```text
degenerate face cleanup；
同 winding、同几何、同材质和同 UV 的 exact duplicate face；
可通过邻接传播唯一确定的 local winding inconsistency；
位置容差内、且不破坏 per-corner attribute 的 geometry vertex weld。
```

### 5.3 条件候选

```text
简单、非自交、可定向的 boundary loop；
局部 edge fan 可唯一分组的 non-manifold edge；
opposite duplicate face；
新增面需要 attribute/fallback policy 的 hole-fill。
```

任一条件存在多解时转为 `manual_repair_required`。

## 6. 修复算法顺序

固定首版顺序：

```text
1. 输入和属性索引校验；
2. 记录 pre-repair hash/diagnostics；
3. 退化面分类与显式移除；
4. exact duplicate face 去重；
5. 受约束 vertex weld；
6. 邻接图 local winding 传播；
7. 简单 boundary loop stitch/hole-fill；
8. 条件 non-manifold fan split；
9. 重建索引和 source mapping；
10. 属性保持校验；
11. post-repair diagnostics；
12. strict_closed admission。
```

每一步失败必须停止后续操作，保留已计划但未执行和已经执行的操作证据。首版不做回溯搜索或多方案自动择优。

## 7. Boundary Repair

boundary loop 只有同时满足以下条件才允许自动处理：

```text
形成单一简单闭环；
无重复顶点和自交；
平面或平面误差低于显式阈值；
边数、周长、面积和受影响面比例未超预算；
法向/组件方向可唯一确定；
新增面 attribute policy 明确；
修复后没有新 non-manifold/self-intersection。
```

真实大网格的自相交证据必须标明 `complete`。现有 `max_triangle_pair_checks` 采样结果只能进入
`manual_repair_required`；R3-01A 使用确定性 AABB broad-phase 和当前 triangle-intersection narrow-phase
补齐完整证据，不允许仅提高 cap 后把未完成检查标记为 PASS。

3 条孤立 boundary edge 不自动解释为一个合法三角孔；必须先确认端点连接形成闭环。

## 8. Non-Manifold Repair

首版只允许局部、可证明唯一的 edge fan 分解。必须验证：

```text
每个输出 edge 最多两个 incident faces；
组件不会静默合并；
分解后每个候选组件可定向；
UV/material triangle provenance 保持；
没有新增开放边界或自相交。
```

对于 `meigui_fudiao` 的大规模 non-manifold，先做来源分类：重复壳、重叠组件、共享边 fan、导出器重复面或
其他模式。没有稳定模式前不实现“批量修复”。

R3-01 已实现只读 pattern classifier：先移除全部 non-manifold edge，以 incidence=2 的 manifold edge 建立
稳定 residual components；再按 duplicate、attribute conflict、mixed winding、separable fan、overlapping
component、unclassified 的固定优先级分类。只有每个 residual fan group 恰含方向相反的两面，才标记
`uniqueFanSplitFeasible=true`。该结果只进入 eligibility/report，不创建 `split_edge_fan` operation。

R3-01A 已实现只读 `MeshCompleteSelfIntersectionAnalyzer`：按 triangle id 构建确定性 median-split AABB BVH，
稳定枚举非共享顶点候选 pair，并复用 `TestTriangleIntersection`。候选 pair 排序、去重并计算 SHA-256；完整
结果要求 tested=candidate。候选预算或内存不足时返回 `budget_or_resource_blocked`，不对部分结果做 PASS
判断。confirmed/coplanar 证据覆盖旧 sampled 结论并继续 fail-fast，不创建 repair operation。

真实结果证明三个 required OBJ 均存在 confirmed self-intersection。因此 R3-02 只复用当前保守操作形成
no-op/repair/manual/rejected matrix，不引入通用自相交重建，也不把 OpenVDB 体素化当作 UV/material 保持的
自动修复手段。

## 9. Attribute Preservation

`SceneModelTriangleMeshAdapter` 应提供或扩展 source mapping：

```text
outputTriangle -> source mesh/triangle/material；
outputCorner -> source UV/texture coordinate；
adapter-rejected degenerate -> source triangle id；
new triangle -> generating boundary loop + explicit attribute policy；
split vertex -> original geometry vertex；
welded vertex -> ordered source vertex set。
```

验证至少包含：

```text
existing triangle material unchanged；
existing per-corner UV unchanged within exact/declared tolerance；
texture resource id remains valid；
new face fallback is explicit；
unknownAttributeTriangles=0 for production candidate。
```

## 10. Hash Contract

使用确定性的 canonical serialization 后计算 SHA-256：

```text
sourceHash；
preRepairGeometryHash；
preRepairAttributeHash；
postRepairGeometryHash；
postRepairAttributeHash；
repairOperationHash；
optionsHash。
```

浮点 canonicalization、端序、索引顺序和组件排序必须固定。哈希用于回归与追溯，不宣称安全签名。

## 11. Stable Error Codes

建议冻结：

```text
E_12E_REPAIR_INPUT_INVALID
E_12E_REPAIR_NOT_ENABLED
E_12E_REPAIR_MODE_UNSUPPORTED
E_12E_REPAIR_SELF_INTERSECTION_REJECTED
E_12E_REPAIR_NOT_ELIGIBLE
E_12E_REPAIR_AMBIGUOUS_TOPOLOGY
E_12E_REPAIR_BUDGET_EXCEEDED
E_12E_REPAIR_ATTRIBUTE_CONFLICT
E_12E_REPAIR_ATTRIBUTE_LOST
E_12E_REPAIR_OPERATION_FAILED
E_12E_REPAIR_POST_STRICT_FAILED
E_12E_REPAIR_HASH_NONDETERMINISTIC
E_12E_REPAIR_MANUAL_REQUIRED
```

测试断言 code，不依赖完整自然语言。

## 12. Report

Schema 固定为：

```text
slicesoft.mesh_repair.12e_08c.1
```

报告是独立证据，可由 12E partition report 引用，但不修改 `p0.rgbwsv.2`。未执行 repair 时仍可输出
preflight/eligibility 报告，并明确 `repairAttempted=false`。

## 13. Performance

分离统计：

```text
diagnosticsMs
eligibilityMs
repairMs
attributeValidationMs
postDiagnosticsMs
hashMs
totalRepairCoreMs
peakWorkingSetBytes
```

Release budget 在 R3 根据真实模型数据冻结。JSON/TIFF/PNG 写盘不得计入 `totalRepairCoreMs`。

## 14. 依赖策略

R1/R2 首版不引入第三方修复库，先验证项目内保守操作是否足够。OpenVDB 不适合作为 UV/材质保持的
三角网格修复器。

若需要第三方库，必须单独 ADR，至少比较两个候选，并覆盖：

```text
CMake target 和 vcpkg manifest；
Windows/MSVC 支持；
许可证和商业分发风险；
triangle/UV/material provenance；
二进制体积、构建耗时和维护状态；
与默认 OpenVDB OFF lane 的隔离。
```

## 15. 测试分层

```text
L1：hash、eligibility、operation unit tests；
L2：generated topology fixture golden；
L3：OBJ/3MF attribute preservation；
L4：post-repair strict admission；
L5：12E partition/texture/raster/full closure；
L6：Release real-model benchmark；
L7：legacy regression、RIP strict、TIFF invariant。
```

## 16. 回滚

修复默认关闭，旧 Profile 不读取 repair 配置。任何失败都丢弃候选 mesh，保留原始 SceneModel 和报告，
不发布 package。代码应允许在不影响 legacy 路径的情况下移除或禁用 repair facade。

固定安全边界：OpenVDB optional/OFF；legacy production path 不替代；`p0.rgbwsv.2`、RGBWSV、uint8 和
`black_is_print` 不修改。

## 17. 双模式调用边界

`MeshRepairFacade` 只由 global_surface_shell 的 admission path 调用：

```text
SlicePipelineRouter(global_surface_shell)
  -> pre-strict
  -> optional repair
  -> post-strict
  -> global partition/raster/composer

SlicePipelineRouter(legacy)
  -> existing legacy path
```

repair 不得成为 Router 之前的无条件导入步骤，也不得修改 legacy 使用的原始 `SceneModel`。修复候选应使用
独立 mesh/attribute DTO；global 失败后丢弃候选并返回稳定错误，不调用 legacy writer 作为隐式回退。

## 18. R3-02 Real Model Repair Matrix 组合入口

R3-02 没有新增 repair 算法，而是在 `mesh_repair_preflight --execute-r3-02` 中显式组合：

```text
R2 cleanup/weld/winding/simple boundary options；
R2 evidence validator；
R3-01 non-manifold classifier；
R3-01A complete self-intersection analyzer。
```

执行顺序保持 self-intersection fail-fast 优先。confirmed/coplanar 输入在 mutation 前返回
`rejected_self_intersection`；无相交 no-op 输入仍执行 attribute/post-strict/hash validator。summary 通过
独立 `slicesoft.mesh_repair_matrix.12e_08c_r3_02.1` 表达 task evidence 和 production Gate，不改变单 case
repair report schema，也不把 diagnostic candidate 接入生产 writer。
