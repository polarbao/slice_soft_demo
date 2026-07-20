# DOC_EXEC_12E-08C-R2-01 保守清理结果

> 文档状态：COMPLETE
> 日期：2026-07-20

## 1. 实现范围

新增 `MeshRepairService` 的显式 `repair_then_strict` cleanup operation set，只允许：

```text
remove_degenerate_triangle；
remove_exact_duplicate_face（同几何、同方向、同 material、同 per-corner UV）。
```

禁止自动删除 opposite duplicate；同向重复面属性冲突时整组 cleanup 不执行；confirmed self-intersection
继续保持 fail-fast 优先级。候选网格是隔离副本，不修改输入 SceneModel，不写生产 TIFF/package。

## 2. Source Mapping

`SceneModelTriangleMeshAdapter` 现在保留 adapter 过滤掉的退化面 source triangle id。报告新增稳定
`sourceMappings[]`：

```text
retained：source -> output triangle；
removed_degenerate：source 被退化面规则移除；
removed_exact_duplicate：source 指向保留面的 source id。
```

操作固定按 degenerate、exact duplicate 排序，operation id 从 1 递增。operation hash 排除计时，包含删除原因、
source id 和保留面映射。

## 3. Generated Fixture 结果

```text
adapter-filtered degenerate + exact duplicate：2 个 operation，candidate post strict PASS；
UV conflict duplicate：manual_repair_required，0 operation，候选几何不变；
opposite duplicate：manual_repair_required，0 operation，不删除；
confirmed self-intersection + attribute conflict：rejected_self_intersection，fail-fast 优先；
repair enabled=false：E_12E_REPAIR_NOT_ENABLED；
相同输入重复执行：operation/post geometry/post attribute/source mapping 稳定。
```

## 4. 真实模型结果

| Case | operation | candidate T | post boundary | post non-manifold | post duplicate/opposite | 状态 |
|---|---:|---:|---:|---:|---:|---|
| `nai_you_new` | 1 个 adapter degenerate 记录 | 117705 | 113 | 0 | 0 / 0 | `manual_repair_required` |
| `aishen_fudiao` | 1 个 adapter degenerate 记录 | 84533 | 3 | 59 | 2 / 2 | `manual_repair_required` |
| `meigui_fudiao` | 0 | 76926 | 0 | 10940 | 7192 / 7192 | `manual_repair_required` |
| Texture2D 3MF | 0 | 12 | 0 | 0 | 0 / 0 | `strict_pass_no_repair` |

两个 OBJ 的退化面在 adapter 阶段已被过滤，因此 operation 补齐 provenance，但 candidate triangle 数不再减少。
`aishen_fudiao` 和 `meigui_fudiao` 的重复面均为 opposite duplicate，本任务按约束保持不变。

## 5. 确定性证据

| Case | operation SHA-256 | stable evidence SHA-256 |
|---|---|---|
| `nai_you_new` | `2d0381e1a4c1c6395286a94a643718a9063f827a8c175f52c3f6ec4847422ca1` | `8c7ae928bfaa31a26555f367c029b5f02cef0102c9c096744425ec54325bcaee` |
| `aishen_fudiao` | `167cc33e30bbb05191254cb61704dad6e4fde92f39e5e3ec5014df8465036400` | `d9495ee62a8fe54be29c018fe7512ca33382965ef5561835695daee30f61cf74` |
| `meigui_fudiao` | `baffd01c0c6815537d9a6b46196a99302d9c00391e8dd8161e658011749f87a6` | `f0383e77f80d5e424727c0118f7e7dad9ba0e7a02c8946ee31830586f055cc03` |
| Texture2D 3MF | `baffd01c0c6815537d9a6b46196a99302d9c00391e8dd8161e658011749f87a6` | `eb3d3c11e2ca878450cb470fa1c468c7e6b329f7dab2512a24d8925284b951db` |

本地证据：`output/benchmarks/12e_08c_r2_cleanup/cleanup_summary.json`，可由
`scripts/run_12e_08c_r2_cleanup_evidence.ps1` 重建，不提交 `output`。

## 6. 阶段结论

R2-01 完成，但没有把三个 OBJ 变成 strict PASS，这符合保守边界：R2-01 只清理唯一可证明的局部问题。
下一原子任务为 `R2-02 Vertex Weld、Winding 与组件守门`；boundary、non-manifold、opposite duplicate 和
sampled self-intersection 继续由后续 Gate 处理。12E-08D 仍为 BLOCKED。

## 7. 验证结果

```text
Debug 全量构建：PASS；
Debug CTest：28/28 PASS；
Qt self-test：startup、experimental-report-summary PASS；
真实模型 cleanup evidence：4/4 case 各重复两次，稳定证据比较 PASS；
run_ci_quick.ps1：FAIL，既有 material_process_top2 golden 期望 widthPx=48、实际=226。
```

Quick CI 的 golden 差异已在 R1-01、R1-02、R1-03 记录，本任务未修改该 Profile、模型缩放或 golden
断言；失败发生在 Mesh Repair 专项之外，不作为 R2-01 保守清理逻辑的伪通过记录。

## 8. 固定边界

```text
repair 默认关闭；
legacy 不调用 cleanup；
OpenVDB optional/OFF；
productionOutputWritten=false；
p0.rgbwsv.2 / R G B W S V / uint8 / black_is_print 不变。
```
