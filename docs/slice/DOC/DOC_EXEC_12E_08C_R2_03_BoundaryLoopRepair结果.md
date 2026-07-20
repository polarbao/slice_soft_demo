# DOC_EXEC_12E-08C-R2-03 Boundary Loop Repair 结果

> 文档状态：COMPLETE
> 日期：2026-07-20

## 1. 实现范围

R2-03 在 R2-01 cleanup、R2-02 guarded topology 之后新增独立 boundary operation set。只允许自动填补：

```text
boundary incidence=1；
每个 boundary vertex 的无向 degree=2，且恰有一入一出；
简单、平面、严格凸的闭环；
edge/diameter/perimeter/planarity/area/affected-face-ratio 全部在显式预算内；
完整 self-intersection evidence 可用；
相邻面材质完全一致且无 UV；
newFaceAttributePolicy=inherit_uniform_material_no_uv。
```

任一闭环不满足条件时，丢弃整个 R2-03 candidate；R2-01/02 已安全完成的前序候选不被反向破坏。

## 2. 确定性几何策略

边界边保留原三角形有向边，按最小 vertex id 提取稳定闭环。填补方向与原边界方向相反，并将最小 vertex id
固定为 fan anchor。当前只接受严格凸环，避免在 R2-03 引入多解耳切、约束 Delaunay 或通用曲面重建。

填补后重新运行 topology/robustness：boundary 必须按闭环边数精确减少，组件数不变，不能新增 degenerate、
non-manifold、duplicate、opposite duplicate、winding issue 或 confirmed self-intersection；前后检查均不得 sampled。

## 3. 属性和来源

新增 `generatedTriangleMappings[]`，每个新面记录：

```text
outputTriangleIndex；
generatingBoundaryVertexIndices[3]；
attributePolicy；
materialName；
hasUv=false。
```

新面获得确定、唯一且不与既有 source id 冲突的内部 triangle id。它们不伪装成原始 source mapping；
`sourceMappings[]` 继续只描述原输入三角形，`attributePreservation.newTriangles` 单独统计新面。

## 4. Generated Fixture

```text
无 UV、统一材质、缺顶面的 box：补 2 面，post strict PASS；
非平面四边环：blocked_boundary_planarity；
边数/受影响比例超预算：blocked_boundary_budget；
带 UV 的边界：blocked_boundary_attribute_policy；
两个环仅共享一个顶点形成 degree=4：blocked_boundary_topology；
相同输入双运行：operation/post geometry/generated provenance 稳定。
```

## 5. 真实模型证据

四个 case 使用同一组显式预算并各执行两次：

| Case | pre/post boundary | fill/new face | 属性状态 | 结果 |
|---|---:|---:|---|---|
| `nai_you_new` | 113 / 113 | 0 / 0 | `blocked_boundary_intersection_evidence` | manual |
| `aishen_fudiao` | 3 / 3 | 0 / 0 | `blocked_boundary_intersection_evidence` | manual |
| `meigui_fudiao` | 0 / 0 | 0 / 0 | `passed` | manual（既有 non-manifold/opposite） |
| Texture2D 3MF | 0 / 0 | 0 / 0 | `passed` | no-op strict PASS |

两个有 boundary 的真实 OBJ 仍只有 sampled self-intersection evidence，R2-03 按设计拒绝生成新面；没有为了
减少 boundary 数量绕过 R3-01A。`meigui_fudiao` 没有 boundary，R2-03 不尝试处理其 non-manifold fan。

## 6. 验证入口

```powershell
cmake --build build --config Debug --target mesh_repair_preflight mesh_repair_cleanup_unit_tests mesh_repair_contract_unit_tests
ctest --test-dir build -C Debug -R "mesh_repair_(r2_03|r2_02|cleanup|contract|preflight)" --output-on-failure
.\scripts\run_12e_08c_r2_03_boundary_evidence.ps1 -BuildDir build -Config Debug
```

实际验证结果：

```text
Debug 全量构建：PASS；
Debug 全量 CTest：30/30 PASS；
R2-01/02/03 + contract/preflight 定向 CTest：7/7 PASS；
Qt startup self-test：PASS；
Qt experimental-report-summary self-test：PASS；
真实模型证据：4/4 case 双运行 stable projection PASS；
productionOutputWritten：全部为 false。
```

本任务没有再次执行 `run_ci_quick.ps1`。同一开发会话的 R2-02 提交前已执行该入口，唯一失败仍为仓库既有
`material_process_top2 widthPx expected=48 actual=226` 基线；R2-03 未触及生产切片、TIFF、Qt 或该 fixture，
不能把该既有失败写成 R2-03 PASS，也不把它归因于边界修复。

## 7. 阶段结论

R2-03 完成，generated simple-hole fixture 已证明安全填补路径成立，但三个 required OBJ 仍未 strict PASS。
下一任务为 R2-04 统一 post-strict 与 attribute guard；12E-08D 继续 BLOCKED。

固定边界保持不变：OpenVDB optional/OFF；repair 默认关闭；legacy 不调用 repair；不写生产 package/TIFF；
`p0.rgbwsv.2`、RGBWSV、uint8、`black_is_print` 不变。
