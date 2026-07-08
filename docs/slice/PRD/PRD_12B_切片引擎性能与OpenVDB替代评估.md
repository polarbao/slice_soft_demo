# PRD_12B_切片引擎性能与OpenVDB替代评估

> 文档版本：v0.2
> 文档状态：PRD / Stage 12B
> 生成日期：2026-07-05
> 更新日期：2026-07-08
> 适用范围：legacy 三角面元切片、OpenVDB SDF candidate、未来高性能切片引擎评估

---

## 0. 阶段拆分

12B 已拆分为三段执行：

```text
12B-R0：Benchmark 契约、真实模型 Release core-only 对比、OpenVDB replacement gate 结论；
12B-R1：Legacy 优化和 2.5D heightfield fast path 小型原型；
12B-R2：OpenVDB hybrid/SDF utility 定位。
```

12B-R0 已完成 benchmark 契约和真实 Release baseline。当前优先执行 12B-R1。拆分决策、benchmark schema 和 R1 设计见：

```text
docs/slice/DOC/DOC_DECISION_12B_R0_R1_R2_切片引擎性能阶段拆分.md
docs/slice/DOC/DOC_SCHEMA_12B_CoreBenchmarkReport.md
docs/slice/ROADMAP/ROADMAP_12B_切片引擎性能分阶段路线.md
docs/slice/REPORT/REPORT_12B_R0_Benchmark契约与真实Release对比当前状态.md
docs/slice/DEV/DEV_12B_R1_LegacyHeightfield优化原型设计.md
```

## 1. 背景

项目曾引入 OpenVDB，希望通过 SDF/体素几何内核降低复杂数学计算成本，提高切片精度和性能。但 11B 的 core-only benchmark 结果显示：

```text
legacy coreComputeMs ≈ 49.716
openvdb coreComputeMs ≈ 1038.711
ratio ≈ 20.893
outputSemanticsComparable = false
replacementPass = false
```

这说明当前 OpenVDB candidate 不能替代 legacy，也不能用当前结果证明 OpenVDB 提速。

12B 的目标是建立公平、可复现、只比较核心切片部分的性能评估体系，并判断 OpenVDB、legacy 优化、2.5D heightfield、GPU/BVH/hybrid 等路线的真实价值。

---

## 2. 产品目标

```text
1. 分离 core slicing time 与 TIFF/preview/report I/O time；
2. 在同模型、同姿态、同分辨率、同材料语义下比较引擎；
3. 解释 OpenVDB 当前更慢的原因；
4. 给出 OpenVDB 是否能解决、何时能替代的 gate；
5. 对其他高效引擎路线形成决策矩阵；
6. 避免继续把 experimental OpenVDB 当 production path 使用。
```

---

## 3. 性能问题定义

本阶段只比较：

```text
model import 后的几何切片核心计算；
model/support/material semantic mask 生成；
必要的纹理采样计算；
```

不纳入 coreComputeMs：

```text
TIFF 写入；
preview PNG/PPM 写入；
manifest/report 写入；
UI 图片加载；
RIP reader 校验；
Debug 控制台输出。
```

可以单独统计：

```text
ioWriteMs
previewWriteMs
reportWriteMs
endToEndMs
```

---

## 4. OpenVDB 当前更慢的原因

当前阶段判断，OpenVDB 更慢不是“库本身一定慢”，而是 demo 使用方式不适合直接替代 legacy：

```text
1. VDB 构建 SDF 有固定体素化成本，模型越小、层数越少越不划算；
2. 当前甲片模型接近 2.5D heightfield，legacy 可直接按列/层扫描，OpenVDB 反而做了更重的体素场构建；
3. 当前 OpenVDB candidate 有 strict_closed admission、拓扑检查、候选包写出等额外成本；
4. 当前 OpenVDB 支撑语义、纹理语义与 legacy 尚未等价，不能以同一输出为目标优化；
5. VDB grid 内存访问和窄带采样在小尺寸模型上未必比数组/heightfield 快；
6. Debug 构建和未充分并行会放大 OpenVDB 开销；
7. 当前实现尚未缓存 SDF、未复用距离场、未做 tile/layer 并行优化；
8. OpenVDB 擅长 SDF offset、平滑、布尔、壳层、间隙分析，不天然等价于最快逐层 raster slicer。
```

---

## 5. 引擎候选路线

### 5.1 Legacy 优化路线

适用：

```text
OBJ/MTL、单材料、甲片 2.5D、当前 production 输出。
```

优化方向：

```text
active edge table；
按 z bucket 过滤三角面；
BVH/AABB 空间索引；
tile cache；
每层/每 tile 并行；
SIMD 扫描；
纹理采样缓存；
支撑投影列缓存。
```

优点：

```text
协议和语义最稳定；
改动可渐进；
最容易与现有 report/preview 对齐。
```

风险：

```text
复杂闭合网格、外侧壳层、布尔类操作仍较难。
```

### 5.2 2.5D Heightfield Fast Path

适用：

```text
甲片、浮雕、可表示为上下表面高度场的模型。
```

思路：

```text
先 rasterize topHeight/bottomHeight；
每层通过 z 与高度范围比较生成 model mask；
支撑按列/轮廓快速生成；
纹理按表面 UV/heightfield 映射。
```

优点：

```text
非常贴合甲片业务；
理论上比逐层三角求交更快；
易做内部填充和下表面支撑。
```

风险：

```text
非 2.5D 多壳、多洞、强倒扣模型不适用；
需要 admission 判断。
```

### 5.3 OpenVDB SDF Hybrid

适用：

```text
外侧光油壳层；
表面壳层厚度；
clearance/offset；
复杂拓扑诊断；
后续需要距离场的支撑分析。
```

定位：

```text
不是直接替代所有 legacy 切片；
作为 SDF 能力模块服务于壳层、距离、补洞、诊断。
```

### 5.4 GPU Raster / Compute

适用：

```text
大量层、大分辨率、需要实时预览或批处理。
```

优点：

```text
吞吐高；
纹理采样天然适配 GPU；
可显著降低大模型切片时间。
```

风险：

```text
Windows/OpenGL/DirectX/Vulkan/CUDA 依赖复杂；
CI 和无 GPU 环境难处理；
生产确定性和调试成本较高。
```

### 5.5 Embree/BVH CPU Ray Query

适用：

```text
复杂三角网格、需要高性能 ray/segment query。
```

优点：

```text
成熟 CPU BVH；
不需要体素化；
适合提升几何查询。
```

风险：

```text
新增依赖；
需要许可证/CMake/vcpkg 评估；
与当前 scanline pipeline 仍需整合。
```

---

## 6. OpenVDB 替代 Gate

OpenVDB 只有满足以下条件，才能从 candidate 升为 production candidate：

```text
1. outputSemanticsComparable = true；
2. 支撑、纹理、模型填充、光油语义与 12A 一致；
3. coreComputeMs 在 Release 下不慢于 legacy 1.2x，或在特定复杂能力上有不可替代价值；
4. endToEndMs 可解释，不把 I/O 计入核心比较；
5. 真实模型通过至少 3 个 fixture；
6. 失败时能自动回退 legacy；
7. OpenVDB OFF 构建仍可通过。
```

若 OpenVDB 无法满足性能 gate，但在壳层/距离场上有价值，应定位为 `SDF utility engine`，而不是 `default slicer engine`。

---

## 7. 成功标准

12B 完成需输出：

```text
1. legacy/OpenVDB core-only benchmark 表；
2. Debug/Release 分离；
3. same-pose/same-resolution/same-semantics 比较；
4. I/O 与 core timing 拆分；
5. 引擎路线决策矩阵；
6. OpenVDB replacement gate 结论。
```
