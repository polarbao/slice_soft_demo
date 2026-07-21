# DOC_EXEC_12E-08C-R3-01A 完整自相交证据结果

> 文档状态：COMPLETE / NON-PRODUCTION
> 日期：2026-07-21
> 分支：`feature/12e-08c-mesh-repair`

## 1. 任务结论

R3-01A 已完成确定性完整自相交分析链路。实现使用 AABB BVH 完整枚举候选 pair，复用既有
`TestTriangleIntersection` 进行 narrow-phase，不执行 repair，不写 TIFF 或 production package。

原有 `max_triangle_pair_checks=250000` 的 sampled 证据不再用于本任务决策。完整分析只有以下结果：

```text
complete_no_intersection
confirmed_intersection
coplanar_overlap
touching_only
budget_or_resource_blocked
```

## 2. 实现内容

新增 `MeshCompleteSelfIntersectionAnalyzer`，并完成以下契约：

```text
按最终切片坐标构建确定性 median-split AABB BVH；
最长轴相同时固定 X、Y、Z 优先级，中心相同时按 triangle id 排序；
排除共享顶点的相邻三角形；
候选 pair 统一为 (minTriangleId,maxTriangleId)，排序、去重并计算 SHA-256；
完整枚举后 testedPairCount 必须等于 candidatePairCount；
超候选预算或内存不足时输出稳定 blocker，不对部分结果做 PASS 判断；
confirmed/coplanar 继续进入 strict fail-fast；
耗时和 peak working set 单列，不参与稳定 hash。
```

`MeshRepairOptions` 新增：

```text
analyzeCompleteSelfIntersections=false
maxCompleteSelfIntersectionCandidatePairs=5000000
```

CLI 新增：

```text
--analyze-r3-01a
--complete-self-intersection-max-candidates <count>
```

报告新增 `completeSelfIntersectionAnalysis`，同时保留 schema
`slicesoft.mesh_repair.12e_08c.1`，因为这是同一 12E-08C 诊断契约的向后兼容字段扩展。

## 3. 单元与契约验证

验证覆盖：

```text
无候选闭合 fixture；
confirmed intersection；
coplanar overlap；
touching only；
共享顶点邻接 pair 排除；
超过一个 BVH node 的 fixture 与 O(N^2) 结果一致；
候选预算为零时稳定 blocked；
非法 vertex index 使用稳定 MeshRepairError；
preflight 用完整证据覆盖 sampled 结论；
report golden/options hash/CLI 参数契约。
```

已运行：

```powershell
cmake --build build --config Debug --target mesh_complete_self_intersection_analyzer_unit_tests mesh_repair_contract_unit_tests mesh_repair_preflight_unit_tests mesh_repair_preflight
ctest --test-dir build -C Debug -R "mesh_(complete_self_intersection|repair_(contract|preflight|r3_01a))" --output-on-failure
```

结果：5/5 定向 CTest PASS。

完整 Debug build、全量 CTest 36/36 和 Qt `--self-test` 也已 PASS。`run_ci_quick.ps1` 已实际运行，但在既有
golden `material_process_top2` 停止：期望 `widthPx=48`，当前实际 `226`。该失败发生在 legacy 真实模型
golden 尺寸基线，早于且独立于 R3-01A 自相交诊断；本任务没有修改缩放、姿态、legacy writer 或该 golden，
因此如实保留为仓库既有基线 blocker，不将 Quick CI 记录为通过。

## 4. 真实模型证据

证据入口：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_12e_08c_r3_01a_complete_self_intersection.ps1 -BuildDir build -Config Debug
```

结果文件：

```text
output/benchmarks/12e_08c_r3_01a_self_intersection/self_intersection_summary.json
```

四个 case 均运行两次，稳定投影完全一致：

| case | analysisStatus | candidates/tested | confirmed | coplanar | touching | pair hash 前缀 |
|---|---|---:|---:|---:|---:|---|
| `nai_you_new` | `confirmed_intersection` | 236181/236181 | 8409 | 0 | 1 | `10694e111bd9` |
| `aishen_fudiao` | `confirmed_intersection` | 491365/491365 | 19270 | 20 | 234 | `2906ecba3b57` |
| `meigui_fudiao` | `confirmed_intersection` | 346104/346104 | 5592 | 0 | 2 | `6cabaf0eddf5` |
| Texture2D 3MF | `complete_no_intersection` | 8/8 | 0 | 0 | 0 | `ecc710f3889e` |

全部 4 个 case 为 `complete=true`，budget blocked 为 0，repeatability 为 4/4 PASS。

## 5. 关键判断

三个真实 OBJ 现在不是“因 sampled 无法判断”，而是已获得完整的 confirmed self-intersection 证据。因此：

```text
不得对其执行当前保守自动修复后声称 strict PASS；
不得提高阈值、忽略相交或切换 legacy 来伪造 global production admission；
R3-02 Repair Matrix 可以开始，但必须把 confirmed/coplanar case 记录为 rejected/manual；
R3-02 不负责发明通用自相交重建算法；
12E-08D 继续 BLOCKED。
```

闭合 Texture2D 3MF 证明 no-op strict lane 可以通过完整证据守门。

## 6. 安全边界

```text
repairAttempted=false；
productionOutputWritten=false；
OpenVDB OFF；
legacy Profile 未修改；
p0.rgbwsv.2、RGBWSV、uint8、black_is_print 未修改；
没有新增第三方依赖。
```

## 7. 下一任务

下一允许原子任务为 `12E-08C-R3-02 真实模型 Repair Matrix`。专用准备文档为
`DOC_PREP_12E_08C_R3_02_真实模型RepairMatrix准备.md`。
