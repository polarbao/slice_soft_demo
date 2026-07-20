# DOC_PREP_12E-08C-R2-03 Boundary Loop Repair 准备

> 文档状态：PREPARED / R2-03 READY
> 日期：2026-07-20
> 前置：12E-08C-R2-02 COMPLETE

## 1. 目标与边界

R2-03 只允许自动填补具有唯一解释的简单 boundary loop。它不是通用网格重建器，不处理开放曲面的大面积
封底、不猜测模型内外、不合并组件，也不处理 non-manifold、opposite duplicate 或 sampled
self-intersection 模型。

## 2. 允许的 Boundary Loop

一个 loop 必须同时满足：

```text
每条边 incidence=1；
边界子图中每个顶点 degree=2；
形成单一无重复顶点闭环；
共享边方向可组成一致有向环；
所在组件 local winding issue=0；
loop 平面误差、边数、直径、周长、面积和受影响面比例均在显式预算内；
投影多边形无自交且严格凸；
完整 self-intersection evidence 可用；
new-face attribute policy 有唯一结果。
```

任一条件失败，整个 boundary stage 返回 `manual_repair_required`，不保留部分填充。

## 3. 计划配置契约

在现有 `MeshRepairOptions` 上补充：

```text
allowBoundaryFill=false；
maxBoundaryLoopEdges=0；
maxBoundaryLoopDiameterMm=0；
maxBoundaryLoopPerimeterMm=0；
maxBoundaryPlanarityErrorMm=0；
maxHoleAreaMm2=0；
maxAffectedFaceRatio=0；
newFaceAttributePolicy="reject"。
```

所有 `0` 预算均表示自动填充禁用，不使用隐式无限预算。首版唯一允许的属性策略为
`inherit_uniform_material_no_uv`：loop 相邻三角形必须使用同一 material 且均无 UV；纹理边界、混合材质或
未知属性一律阻断。该限制是安全门禁，不是最终产品能力结论。

## 4. 算法约束

```text
1. 从 incidence=1 edge 建立稳定排序的 directed boundary graph；
2. 按最小 vertex id 提取 loop，canonicalize 起点；
3. 计算 Newell normal、最佳投影轴、planarity、perimeter、diameter 和 area；
4. 校验二维 simple/strict-convex；
5. 按现有 boundary direction 的反方向生成确定性 fan；
6. 为新面写入显式 material/no-UV attributes；
7. 记录 generating loop、output triangle 和 policy；
8. 全量重跑 topology/robustness/post strict；
9. 任何新增 non-manifold/duplicate/self-intersection 或组件变化均丢弃 candidate。
```

首版不添加中心顶点，避免生成新的几何位置和 UV 插值。fan anchor 固定为 canonical loop 的最小 vertex id。

## 5. Provenance 与报告

新增面不能伪装成 source triangle。R2-03 应新增独立 `generatedTriangleMappings[]`：

```text
outputTriangleIndex；
generatingBoundaryVertexIndices；
attributePolicy；
materialName；
hasUv=false。
```

`fill_boundary_loop` operation 的 `inputElementIds` 记录 canonical boundary vertex ids，`outputElementIds` 记录
全部新 triangle ids，parameters 记录所有预算实测值。上述内容进入 operation hash；计时不进入 hash。

## 6. Generated 测试矩阵

```text
closed box no-op：strict PASS；
缺少一个方形面的 box：2 个 new face，post strict PASS；
boundary degree 非 2：blocked；
非平面 loop：blocked；
凹 loop：blocked；
edge/perimeter/diameter/area/ratio 任一超预算：blocked；
UV 或混合材质边界：blocked_attribute_conflict；
self-intersection sampled/confirmed：blocked/fail-fast；
相同输入两次：operation/generated mapping/post hash 一致。
```

## 7. 真实模型预期

| Case | R2-03 预期 |
|---|---|
| `nai_you_new` | sampled self-intersection 证据未补齐，保持 manual，不自动填 113 条 boundary edge |
| `aishen_fudiao` | non-manifold=59 且 boundary=3，保持 manual |
| `meigui_fudiao` | 无 boundary，但 non-manifold/opposite duplicate 保持 manual |
| Texture2D 3MF | closed no-op strict PASS |

真实模型“没有执行 fill”可以是正确验收结果；不得为追求 strict PASS 放宽边界。

## 8. 实施模块

优先新增 `MeshRepairBoundaryOperations.*`，由 `MeshRepairService` 在 R2-02 后显式调用。只扩展诊断 CLI、DTO、
report、tests 和 evidence script；不接入 Qt、pipeline router、TIFF writer 或 OpenVDB。

## 9. 验证命令计划

```powershell
cmake --build build --config Debug --target mesh_repair_preflight mesh_repair_cleanup_unit_tests
ctest --test-dir build -C Debug -R "mesh_repair_(r2_03|cleanup|preflight)" --output-on-failure
.\scripts\run_12e_08c_r2_03_boundary_evidence.ps1 -BuildDir build -Config Debug
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

## 10. 退出标准

generated square-hole fixture post strict PASS；所有歧义、属性冲突和超预算 fixture 稳定 blocked；四个真实
case 有双运行确定性证据；默认 OFF/legacy/协议不变。满足后才允许启动 R2-04 统一 Post-Repair Strict 与
Attribute Guard。
