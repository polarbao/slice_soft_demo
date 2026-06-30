# PRD_09B_R2_表面壳层纹理鲁棒性性能与多材质策略收口

> 文档版本：v0.1
> 文档状态：Draft / PRD
> 适用阶段：09B-R2
> 建议提交目录：`docs/slicer/`

---

## 1. 产品目标

09B-R2 要回答三个问题：

```text
1. 真实指甲/浮雕模型是否能稳定生成正确壳层？
2. 高三角面数和小 voxel size 下性能/内存是否可接受？
3. 多材质、UV seam 和最近表面歧义如何形成稳定产品语义？
```

---

## 2. 必须纳入的 Fixture 类别

### 2.1 真实业务模型

至少：

```text
真实指甲 OBJ + MTL + PNG
真实指甲 3MF + Texture2D
复杂浮雕 OBJ/3MF
```

### 2.2 几何鲁棒性

至少：

```text
薄壁
尖角
小孔/小间隙
多个 disconnected components
局部反向面
重复面
自交
极小三角形
高纵横比三角形
```

### 2.3 材质与纹理

至少：

```text
多 material
多 texture
UV seam
clamp
repeat
missing texture
missing UV
UV out-of-range
同距离 triangle tie
```

### 2.4 性能规模

至少：

```text
1k triangles
10k triangles
50k triangles
100k triangles（环境允许时）
```

---

## 3. Mesh Policy

保留：

```text
strict_closed
warn_and_attempt
```

新增建议：

```text
strict_closed：
  boundary/non-manifold/self-intersection/duplicate-face 按策略拒绝

diagnostic_only：
  只生成拓扑报告，不进入 OpenVDB

warn_and_attempt：
  可实验执行，但报告必须标记 nonProduction=true
```

---

## 4. 多材质与 Seam 产品策略

必须明确：

### 4.1 最近表面命中

多个 triangle 距离处于：

```text
abs(distanceA - distanceB) <= tieEpsilonMm
```

时，使用稳定 tie-break：

```text
1. 最小 distance；
2. 最大 barycentric interior margin；
3. 最小 source_triangle_index。
```

### 4.2 UV Seam

同一几何边两侧 UV 不连续时：

```text
命中哪一侧 triangle，就使用哪一侧 UV；
不得对跨 seam 的两个 triangle UV 做平均。
```

### 4.3 Material Seam

命中 triangle 的 material 为唯一来源。不得跨 material seam 混色，除非未来单独定义 blend policy。

### 4.4 Texture Source

保持：

```text
texture → material diffuse → fallback
```

并增加按 material/texture 的统计。

---

## 5. 性能与内存门槛

第一版门槛使用相对基线，不使用绝对生产 SLA。

必须记录：

```text
importMs
adapterMs
topologyMs
levelSetMs
bvhBuildMs
transferMs
previewMs
totalMs
processPeakWorkingSetBytes
OpenVdbGridBytes
bvhEstimatedBytes
textureCacheBytes
maskBytes
triangleCount
shellVoxelCount
nearestQueryCount
averageVisitedBvhNodes
maxVisitedBvhNodes
```

BVH 必须与 brute force 对照：

```text
小模型：结果一致
中模型：BVH 明显快于 brute force
大模型：不得执行完整 brute force，只做采样对照
```

---

## 6. Voxel/Thickness Matrix

至少验证：

```text
voxelSizeMm = 0.10 / 0.05 / 0.025
shellThicknessMm = 0.05 / 0.10 / 0.20
```

检查：

```text
shell 单调性
inside 体积趋势
纹理转移距离
内存增长
运行时间增长
小特征保留
```

---

## 7. Golden Baseline

新增：

```text
tests/golden/expected/surface_shell_real_model_r2.json
```

基线只保存稳定摘要，不保存机器相关绝对时间：

```text
schema
input hash/fixture id
triangle counts
topology counts
inside/shell/interior
texture source counts
outsideColoredVoxels
unclassifiedVoxels
warnings/errors codes
```

性能结果单独写 benchmark report，不进入严格 golden equality。

---

## 8. 验收标准

```text
1. 至少一个真实指甲 OBJ 和一个真实指甲 3MF 通过；
2. 多材质/多纹理 fixture 通过；
3. UV seam 行为有专用 fixture；
4. 薄壁/尖角/小间隙有统计和 preview；
5. duplicate/local reversed/self-intersection 被诊断；
6. strict_closed 对不可接受输入失败；
7. warn_and_attempt 标记 nonProduction；
8. 10k+ triangle fixture 完成 Release benchmark；
9. BVH 与 brute force 小样本一致；
10. 内存统计覆盖主要对象；
11. voxel/thickness matrix 完成；
12. golden baseline 通过；
13. OFF run_ci_quick.ps1 通过；
14. production RGBWSV 未修改。
```

---

## 9. 非目标

```text
不接入 production slicer_cli
不写 production TIFF
不实现最终自动修复器
不实现纹理跨 seam 混合
不实现 compensated varnish
不实现支撑 SDF
