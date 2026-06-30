# PRD_04_彩色纹理模型切片基础版

> 文档版本：v0.1  
> 文档状态：Draft / PRD  
> 建议提交目录：`docs/slicer/`

## 1. 目标

支持基础彩色纹理切片：

```text
OBJ + MTL + Texture
→ UV 采样
→ RGB 通道写入
→ RGBWSV TIFF
→ preview / reports / rip_reader_test
```

当前阶段只增强 RGB 数据来源，不改变 RGBWSV 协议。

## 2. 必须支持

```text
OBJ vt 纹理坐标读取
OBJ face v/vt/vn 基础解析
face material assignment
MTL newmtl / Kd / map_Kd 读取
贴图路径解析
基础图片读取：PNG / JPG / BMP
UV 到 texture RGB 采样
texture_report.json
model_rgb preview
RGB channelStats
```

## 3. 优先业务场景

### 3.1 彩色浮雕美甲模型

```text
输入：OBJ + MTL + PNG/JPG texture
几何：relief_heightfield
颜色：从 top surface UV 采样
输出：RGB 彩色 + 可选 S 支撑
```

### 3.2 彩色浅浮雕徽章

```text
输入：带贴图 OBJ
几何：relief_heightfield
输出：RGBWSV TIFF，RGB 来自纹理
```

### 3.3 普通贴图 OBJ 基础验证

```text
输入：简单闭合 OBJ + MTL + texture
第一版可以先做 fallback 或有限采样，不要求完整 color shell
```

## 4. Texture Apply Mode

第一版建议只实现：

```text
texture.applyMode = solid_volume_from_top_surface
```

含义：

```text
对每个 XY column 找到最上表面命中 triangle；
用该 triangle 的 UV 采样 texture RGB；
将该 column 的模型占据层写入同一 RGB。
```

后续再扩展：

```text
surface_shell
per_layer_surface_sample
color_shell_volume
texture_driven_varnish
texture_driven_white
```

## 5. 输出语义

继续遵守 03 协议：

```text
RGBWSV
uint8
0 = 打印
255 = 不打印
black_is_print
```

纹理模型区域：

```text
R/G/B = sampled texture RGB
W = 255
S = 255
V = 255
```

支撑区域：

```text
R/G/B/W/V = 255
S = 0
```

空白区域：

```text
R/G/B/W/S/V = 255
```

## 6. Fallback 规则

| 场景 | 行为 |
|---|---|
| OBJ 无 vt | 使用 fallbackRgb |
| MTL 缺失 | 使用 fallbackRgb |
| map_Kd 缺失 | 使用 Kd 或 fallbackRgb |
| texture 文件不存在 | warn_and_fallback |
| texture decode 失败 | warn_and_fallback 或 fail_fast |
| UV 超出 0..1 | clamp 或 repeat |

推荐配置：

```json
{
  "texture": {
    "enabled": true,
    "applyMode": "solid_volume_from_top_surface",
    "sampler": "bilinear",
    "uvAddressMode": "clamp",
    "flipV": true,
    "fallbackRgb": [0, 0, 0],
    "missingTexturePolicy": "warn_and_fallback"
  }
}
```

## 7. Report

新增：

```text
reports/texture_report.json
```

字段建议：

```text
enabled
applyMode
materials
textureFiles
loadedTextures
missingTextures
facesWithUv
facesWithoutUv
sampledPixels
fallbackPixels
uvOutOfRangePixels
warnings
```

## 8. 验收标准

1. 带 OBJ/MTL/Texture 的 relief 模型可运行。
2. `texture.enabled = true` 时 RGB 来自纹理采样。
3. `model_rgb` preview 能看到非纯常量色纹理变化。
4. `texture_report.json` 输出贴图统计。
5. 缺失纹理时能 fallback 并记录 warning。
6. S 支撑通道不被破坏。
7. `rip_reader_test` 通过。
8. `run_regression.ps1` 仍通过 P0 / Relief / Support / Bad package。
9. 不改变 RGBWSV 协议。
10. 不实现 RIP 半色调、ICC、3MF、OpenVDB。

## 9. 非目标

```text
ICC / 色彩管理
CMYK
RIP 半色调
3MF
PBR
法线贴图
多贴图混合
纹理驱动白墨
纹理驱动光油
OpenVDB
Qt UI
```
