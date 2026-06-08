# DEV_04_OBJ_MTL_Texture采样与RGB输出设计

> 文档版本：v0.1  
> 文档状态：Draft / DEV  
> 建议提交目录：`docs/slicer/`

## 1. 技术目标

在不改变 `p0.rgbwsv.1` 协议的前提下，为 `OBJ + MTL + Texture` 增加基础纹理采样能力。

目标：

```text
texture RGB → layer RGB channels
support 仍写 S
manifest schema 仍为 p0.rgbwsv.1
rip_reader_test 仍通过
```

## 2. 推荐模块

建议新增或逐步拆分：

```text
src/slicer_core/texture/
  texture_image.*
  texture_loader.*
  texture_sampler.*
  material_library.*
  texture_report.*
```

短期也可以先放在 `model.* / slicer.*`，但函数边界必须清晰。

## 3. OBJ Loader 增强

需要支持：

```text
vt texture coordinates
face formats:
  v
  v/vt
  v//vn
  v/vt/vn
face material assignment
```

建议结构：

```cpp
struct TexCoord {
    double u;
    double v;
};

struct FaceVertex {
    int position_index;
    int texcoord_index{-1};
    int normal_index{-1};
};

struct MeshFace {
    std::array<FaceVertex, 3> vertices;
    std::string material_name;
};
```

## 4. MTL Loader

读取：

```text
newmtl
Kd r g b
map_Kd texture_path
```

建议结构：

```cpp
struct MaterialInfo {
    std::string name;
    std::array<std::uint8_t, 3> diffuse_rgb{0, 0, 0};
    std::filesystem::path diffuse_texture_path;
    bool has_texture{false};
};
```

路径规则：

```text
map_Kd 先相对 MTL 文件目录解析；
若失败，再相对 OBJ 文件目录解析。
```

## 5. Texture Loader

第一版推荐：

```text
stb_image
```

原因：

```text
header-only
不依赖 Qt
适合 CLI/core
支持 PNG/JPG/BMP
```

不要在 `slicer_core` 中引入 Qt image 依赖。

结构：

```cpp
struct TextureImage {
    int width;
    int height;
    int channels;
    std::vector<std::uint8_t> rgba;
};
```

## 6. Texture Sampler

支持：

```text
sampler = nearest / bilinear
uvAddressMode = clamp / repeat
flipV = true / false
```

默认：

```text
bilinear
clamp
flipV = true
```

## 7. Relief Top Surface Sampling

第一版优先支持 relief：

```text
对每个 XY column:
  找到 top hit triangle
  记录 barycentric coordinate
  插值 UV
  采样 texture RGB
  将该 column 的 model occupied layers 写入 sampled RGB
```

建议结构：

```cpp
struct ReliefColorColumnInfo {
    bool has_color{false};
    std::array<std::uint8_t, 3> rgb{0, 0, 0};
    bool used_fallback{false};
};
```

## 8. Layer Compose

当：

```text
texture.enabled = true
```

且当前模型像素有 texture RGB：

```text
R/G/B = sampled RGB
W/S/V = 255
```

支撑仍然：

```text
S = 0
```

优先级保持：

```text
Model > Support > Empty
```

## 9. texture_report

新增：

```json
{
  "enabled": true,
  "applyMode": "solid_volume_from_top_surface",
  "stats": {
    "facesWithUv": 0,
    "facesWithoutUv": 0,
    "sampledPixels": 0,
    "fallbackPixels": 0,
    "uvOutOfRangePixels": 0
  },
  "warnings": []
}
```

## 10. 样例

新增：

```text
samples/models/textured/
  textured_relief.obj
  textured_relief.mtl
  textures/checker.png
  textures/gradient.png

samples/configs/textured/
  textured_relief_rgb.json
  textured_missing_texture_fallback.json
  textured_no_uv_fallback.json
```

## 11. 回归要求

04 完成后必须保证：

```text
scripts/run_regression.ps1 仍通过
ordinary P0 仍通过
Relief V/W/RGB 仍通过
Support 仍通过
Bad package 仍通过
```

并增加 textured 正向样例。

## 12. 非目标

```text
ICC
CMYK
RIP halftone
PBR
normal map
multi-texture blending
3MF
OpenVDB
Qt
```
