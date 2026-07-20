# DOC_PREP_12E-08C-R2 Conservative Repair 准备

> 文档状态：R2-01 COMPLETE / R2-02 READY
> 日期：2026-07-20

## 1. 准备结论

R2 的保守修复顺序、操作资格、属性边界、回滚和验证矩阵已明确。R2-01 已完成显式 cleanup 和真实模型
证据；R2-02 已补齐独立准备，可以在用户明确启动后实施。本准备不授权 R2-03/R2-04 或写生产包。

## 2. 固定执行链

```text
original SceneModel
  -> pre diagnostics/hash
  -> eligibility
  -> isolated candidate mesh copy
  -> one ordered repair operation set
  -> attribute preservation validation
  -> post diagnostics/strict
  -> keep candidate evidence or discard candidate
```

任何失败都丢弃 candidate，原始 SceneModel 不变。repair 默认关闭；legacy 不调用该链路。

## 3. R2-01 Degenerate 与 Exact Duplicate Cleanup

允许：删除面积/边长低于 scale-aware tolerance 的退化面；删除顶点循环和所有材料/UV/纹理属性完全一致的
同向 exact duplicate。

禁止：合并属性冲突面；自动处理 opposite duplicate；删除后隐式合并组件。

必须保留 source triangle mapping、删除原因、原/新索引和 operation hash。

R1-04 范围修正：`nai_you_new` 与 `aishen_fudiao` 的各 1 个退化面已在当前 adapter 阶段过滤。R2-01 必须
把该事实转为显式、可追溯的 cleanup/source mapping，不得把已拒绝面重新放回 downstream candidate。
`aishen_fudiao` 的 2 个 duplicate 和 `meigui_fudiao` 的 7192 个 duplicate 全部为 opposite duplicate，
不属于本任务允许自动删除的 same-attribute exact duplicate。

## 4. R2-02 Vertex Weld、Winding 与组件守门

Vertex weld 仅允许在显式阈值内、不会产生退化/属性冲突/组件隐式 merge 时执行。阈值来自
`MeshScaleTolerance` 和配置快照，不允许硬编码绝对值。

Winding 只在单个可定向组件内、传播结果唯一时调整。多解、冲突或跨组件情况输出
`manual_repair_required`。每次翻转必须同步 per-corner UV 顺序。

## 5. R2-03 Boundary Loop Repair

首版只处理简单、无自交、边唯一、尺寸在预算内的 boundary loop。Hole fill 必须有显式 new-face material/UV
policy；无法确定纹理属性时不得生成 production candidate。

非平面、大孔、多环相交、边 fan 歧义和 confirmed self-intersection 均不自动修复。

## 6. R2-04 Post-Repair Strict 与 Attribute Guard

候选结果必须重新运行完整 topology/robustness strict diagnostics，并验证：

```text
boundary/non-manifold/duplicate/opposite duplicate/local winding blocker 为零；
confirmed self-intersection 为零；
source-mapped triangle 的 material/UV/texture resource 不变；
new face 全部有可审计 policy；
pre/post/operation/options hash 可重复；
任何失败时 productionOutputWritten=false。
```

R2 即使 repaired strict PASS，也只生成 12E-08D 输入证据，不写 TIFF/package。

## 7. 预计模块

```text
src/slicer_core/geometry/repair/MeshRepairService.*；
src/slicer_core/geometry/repair/RepairOperations.*；
src/slicer_core/geometry/repair/AttributePreservationValidator.*；
tests/unit/mesh_repair_*；
tests/golden/mesh_repair_*。
```

实施前按当前 CMake 结构确认实际文件和 target，不复制第二套 mesh DTO。

## 8. 原子 Gate

| 原子任务 | 前置 | 退出标准 |
|---|---|---|
| R2-01 | R1 complete | generated cleanup fixture post strict PASS |
| R2-02 | R2-01 | weld/winding 唯一 case PASS，歧义 case manual |
| R2-03 | R2-02 | 简单 loop PASS，复杂/属性未知 case blocked |
| R2-04 | R2-03 | post strict、attribute、hash、negative matrix 全通过 |

## 9. R1-04 Baseline 复核

| Case | 与 R2 直接相关的事实 | R2 约束 |
|---|---|---|
| `nai_you_new` | degenerate=1、boundary=113、components=10 | R2-01 记录 cleanup；R2-03 分类 boundary；禁止 merge |
| `aishen_fudiao` | degenerate=1、boundary=3、nonManifold=59、opposite=2、components=10 | R2-01 只处理 degenerate；opposite 不自动去重 |
| `meigui_fudiao` | nonManifold=10940、opposite=7192、components=2 | R2-01 no-op；留给 R3 pattern classifier |
| Texture2D 3MF | strict pass | 所有 R2 操作必须 no-op |

三个 OBJ 还包含 sampled self-intersection evidence。R2-04 必须保持 sampled 为 strict blocker；完整真实模型
证据由新增 R3-01A 处理，R2 不以提高 pair cap 的临时方式伪造 PASS。

R2-01 实际结果进一步确认：`nai_you_new`、`aishen_fudiao` 各记录 1 个 adapter-filtered degenerate operation；
`meigui_fudiao` 和闭合 3MF 为 cleanup no-op。三个 OBJ 均未因本任务变为 strict PASS，所有 opposite duplicate
保持未删除。R2-02 的详细契约见 `DOC_PREP_12E_08C_R2_02_VertexWeldWindingComponentGuard准备.md`。

## 10. 停止条件

需要 destructive boolean、voxel remesh、第三方修复库、UV 重投影或放宽 strict 时立即停止并创建独立 ADR。
