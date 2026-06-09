# DEMO_05A_真实材料工艺参数验证方案

> 文档版本：v0.1  
> 文档状态：Draft / DEMO  
> 适用阶段：PRD_05A / DEV_05A  
> 建议提交目录：`docs/slicer/`

## 1. Demo 目标

验证：

```text
RGB + W + V 工艺 profile
W underbase 覆盖
V top_n_layers 差异
3MF Texture2DGroup 输入
OBJ/MTL Texture 输入
material_process_report 输出
profile compare 输出
```

## 2. 必须验证样例

### 2.1 RGB + W + V top1

```text
nail_rgb_white_varnish_top1.json
```

验收：

```text
RGB printPixels > 0
W printPixels > 0
V printPixels > 0
V active layer count ≈ 1
material_process_report.validation.pass = true
```

### 2.2 RGB + W + V top2

```text
nail_rgb_white_varnish_top2.json
```

验收：

```text
V active layer count ≈ 2
V printPixels > top1 V printPixels
```

### 2.3 RGB + W + V top3

```text
nail_rgb_white_varnish_top3.json
```

验收：

```text
V active layer count ≈ 3
V printPixels > top2 V printPixels
```

### 2.4 3MF Texture2DGroup + RGBWV Profile

```text
three_mf_texture_rgb_white_varnish.json
```

验收：

```text
texture_report.source = 3mf_internal
RGB/W/V printPixels > 0
rip_reader_test --summary pass
```

### 2.5 OBJ/MTL Texture + RGBWV Profile

```text
obj_mtl_texture_rgb_white_varnish.json
```

验收：

```text
texture_report.stats.sampledPixels > 0
material_role_mapping_report.mappedRgb > 0
RGB/W/V printPixels > 0
rip_reader_test --summary pass
```

## 3. Profile Compare

命令示例：

```powershell
.\scripts\compare_material_profiles.ps1 `
  -PackageA output\NailRgbWhiteVarnishTop1 `
  -PackageB output\NailRgbWhiteVarnishTop3 `
  -Output output\MaterialProfileCompare_top1_top3.json
```

验收：

```text
delta.varnishPrintPixels > 0
changedLayers > 0
```

## 4. 回归 Checklist

- [ ] top1 profile pass。
- [ ] top2 profile pass。
- [ ] top3 profile pass。
- [ ] topLayers 差异可观测。
- [ ] material_process_report.json 存在。
- [ ] material_profile_compare_report.json 可生成。
- [ ] 3MF Texture2DGroup profile pass。
- [ ] OBJ/MTL texture profile pass。
- [ ] Support S 不被 RGB/W/V 覆盖。
- [ ] p0.rgbwsv.2 不变。
- [ ] quick regression pass。

## 5. 状态报告

完成后生成：

```text
docs/slicer/REPORT_05A_真实材料工艺参数验证当前实现状态.md
```
