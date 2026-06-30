# PRD_09B_OpenVDB_SDF表面壳层纹理原型

> 文档版本：v0.1  
> 文档状态：Draft / PRD  
> 适用阶段：09B  
> 建议提交目录：`docs/slicer/`

---

## 1. 背景

当前纹理策略边界已预留：

```text
FullVolume
SurfaceShell
TopSurfaceOnly
OuterSurfaceShell
```

但当前正式配置和生产路径仍主要使用：

```text
solid_volume_from_top_surface
top_surface_only
top_surface_band
```

09A-R2 已证明 OpenVDB 12.0.1 可以在独立 ON 构建中真实运行。09B 应验证 3D SDF 壳层能否承载后续 `surface_shell_texture`。

---

## 2. 产品目标

09B 目标：

```text
1. 从简单闭合三角网格生成 OpenVDB level set；
2. 从 SDF 提取模型内部 outer shell；
3. 将 RGB 只应用于 shell；
4. 将 interior 标记为 fill/base role；
5. 输出实验 report 和逐层 preview；
6. 保持 production slicer 与 RGBWSV 输出不变。
```

---

## 3. 用户场景

### 3.1 生成式闭合模型验证

开发者运行：

```powershell
.\build-openvdb-09b\Debug\surface_shell_texture_demo.exe `
  --case generated-box `
  --shell-mm 0.10 `
  --voxel-mm 0.05 `
  --output output\SurfaceShellTextureBox
```

期望：

```text
OpenVDB level set 生成成功
shellVoxels > 0
interiorVoxels > 0
outsideColoredVoxels = 0
preview 生成成功
```

### 3.2 壳层厚度单调性

分别运行：

```text
shell-mm = 0.05
shell-mm = 0.10
shell-mm = 0.20
```

期望：

```text
壳层厚度增加时 shellVoxels 不减少；
interiorVoxels 不增加；
模型外包络不扩大。
```

### 3.3 实验 RGB 壳层

第一版允许使用：

```text
constant_rgb
checker_rgb
existing_surface_sampler_adapter
```

其中 `constant_rgb` 或 `checker_rgb` 为必须完成，真实纹理采样可作为增强项。

---

## 4. 必须支持能力

### 4.1 OpenVDB Level Set Builder

输入：

```text
vertices
triangle indices
voxelSizeMm
halfWidthVoxels
```

输出：

```text
OpenVDB FloatGrid level set
activeVoxelCount
world/index transform
grid bbox
```

### 4.2 Surface Shell Classification

输出：

```text
inside voxels
shell voxels
interior voxels
outside voxels
```

必须满足：

```text
shell ∩ interior = empty
shell ∪ interior = inside
outsideColoredVoxels = 0
```

### 4.3 Shell Texture Prototype

输出实验分类：

```text
shellRole = rgb
interiorRole = fill/base
outsideRole = empty
```

第一版不写入生产 MaterialPolicy。

### 4.4 Report

新增：

```text
reports/surface_shell_texture_report.json
schema = p0.surface_shell_texture_report.1
```

### 4.5 Preview

至少输出：

```text
preview/shell_layer_*.png
preview/interior_layer_*.png
preview/composite_layer_*.png
```

---

## 5. 验收标准

```text
1. USE_OPENVDB=ON 下 surface_shell_texture_demo 可构建；
2. generated-box case 可运行；
3. OpenVDB level set 非空；
4. shellVoxels > 0；
5. interiorVoxels > 0；
6. outsideColoredVoxels = 0；
7. shell + interior = inside；
8. 壳层厚度单调性测试通过；
9. report schema 正确；
10. preview 至少生成 1 层；
11. OFF 默认构建和 run_ci_quick.ps1 通过；
12. production slicer_cli / RGBWSV 不变。
```

---

## 6. 非目标

```text
不接入 production SliceConfig
不写 production RGBWSV TIFF
不实现完整 OBJ/3MF nearest-surface texture transfer 的生产版
不处理所有非流形/开口 mesh
不实现 compensated varnish
不替换现有 texture pipeline
