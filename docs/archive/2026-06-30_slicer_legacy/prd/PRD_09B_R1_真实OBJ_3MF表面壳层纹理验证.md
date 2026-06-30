# PRD_09B_R1_真实OBJ_3MF表面壳层纹理验证

> 文档版本：v0.1  
> 文档状态：Draft / PRD  
> 适用阶段：09B-R1  
> 建议提交目录：`docs/slicer/`

---

## 1. 产品目标

在 09B generated-box 原型基础上，验证真实纹理模型的完整实验链路：

```text
OBJ/MTL/PNG 或 3MF Texture2D
→ SceneModel
→ TriangleMeshData + SurfaceAttributes
→ OpenVDB Level Set
→ 3D Surface Shell
→ Nearest Triangle + Barycentric UV
→ Texture Sample / Material Diffuse / Fallback
→ Report / Preview
```

---

## 2. 必须验证的真实输入

至少准备：

```text
1 个闭合 OBJ + MTL + PNG 纹理模型
1 个闭合 3MF + Texture2D 纹理模型
```

优先使用同一几何内容分别导出 OBJ 和 3MF，以便比较跨格式一致性。

建议配置：

```text
samples/configs/openvdb/surface_shell_obj_real.json
samples/configs/openvdb/surface_shell_3mf_real.json
```

---

## 3. 纹理转移优先级

壳层 voxel 的颜色来源按以下顺序：

```text
1. 源 triangle 有 UV 且材质纹理存在：
   使用 nearest triangle + barycentric UV + sample_texture_rgb

2. triangle 无有效纹理，但 MaterialInfo.has_diffuse：
   使用 material diffuse RGB

3. 以上均不可用：
   使用配置 fallback RGB
```

必须分别统计三种来源。

---

## 4. Mesh 质量策略

第一版支持两种模式：

```text
strict_closed
warn_and_attempt
```

`strict_closed`：

```text
boundaryEdges > 0 → fail
nonManifoldEdges > 0 → fail
有效 triangle 数为 0 → fail
```

`warn_and_attempt`：

```text
记录 warning；
允许进入 OpenVDB；
不得把失败或异常分类伪装为 PASS。
```

---

## 5. 最近表面与 UV 转移

对每个 shell voxel center：

```text
1. 转换到 world mm；
2. 查询最近源 triangle；
3. 获取 closest point；
4. 获取 barycentric coordinates；
5. 插值 triangle UV；
6. 根据 material_name 查找纹理或 diffuse；
7. 采样 RGB；
8. 记录查询距离。
```

必须支持：

```text
maxTransferDistanceMm
```

超过阈值时：

```text
warning + fallback
```

---

## 6. Report

真实模型 case 输出：

```text
reports/surface_shell_texture_report.json
schema = p0.surface_shell_texture_report.2
```

保留 09B `.1` 的基础字段，并新增：

```text
input
meshDiagnostics
transferStats
textureStats
performance
```

---

## 7. 负向测试

必须覆盖：

```text
无 UV
纹理缺失
UV 越界
开口 mesh
非流形 edge
退化 triangle
非法材质引用
```

其中开口和非流形在 `strict_closed` 下必须失败。

---

## 8. 验收标准

```text
1. OBJ 真实 fixture 可导入并生成 level set；
2. 3MF 真实 fixture 可导入并生成 level set；
3. 两个 fixture 的 shellVoxels > 0；
4. 两个 fixture 的 interiorVoxels > 0；
5. outsideColoredVoxels = 0；
6. unclassifiedVoxels = 0，或有被明确接受的原因；
7. sampledTextureVoxels > 0；
8. 完整纹理 fixture 的 missingUv/fallback 数符合预期；
9. OBJ/3MF 同几何 shell voxel 数差异在约定容差内；
10. open/non-manifold negative fixture 在 strict 模式失败；
11. report schema = p0.surface_shell_texture_report.2；
12. preview 可见真实纹理变化；
13. OFF run_ci_quick.ps1 通过；
14. production RGBWSV 不变。
```

---

## 9. 非目标

```text
不接入 production SliceConfig
不输出 production RGBWSV TIFF
不实现高性能生产级 BVH 的最终版本
不自动修复复杂非流形模型
不做光油补偿
不做设备/RIP链路
