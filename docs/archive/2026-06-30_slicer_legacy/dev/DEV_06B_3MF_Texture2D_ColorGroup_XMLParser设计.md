# DEV_06B_3MF_Texture2D_ColorGroup_XMLParser设计

> 文档版本：v0.1  
> 文档状态：Draft / DEV  
> 适用阶段：PRD_06B  
> 所属模块：3MF Importer / Texture / ColorGroup / XML  
> 建议提交目录：`docs/slicer/`

---

## 1. 技术目标

在 06A 的 3MF importer 基础上新增：

```text
3MF ColorGroup
3MF Texture2D
3MF Texture2DGroup
3MF 内部 texture resource loading
TextureSampler 复用
three_mf_report 增强
```

不改变：

```text
p0.rgbwsv.2
MaterialRoleMapping
MaterialPolicy
Support pipeline
TIFF writer / reader
```

---

## 2. XML Parser 设计

### 2.1 推荐方案

推荐引入：

```text
tinyxml2
```

CMake：

```cmake
find_package(tinyxml2 CONFIG REQUIRED)
target_link_libraries(slicer_core PRIVATE tinyxml2::tinyxml2)
```

如果项目暂不希望增加 vcpkg 依赖，则采用：

```text
ThreeMfXmlReader v2
```

但必须支持：

```text
namespace/local-name
self-closing tag
属性解析
嵌套资源节点
错误定位
```

### 2.2 安全要求

无论使用 tinyxml2 还是自研 reader，必须保持：

```text
不解析外部实体
不访问网络
不加载外部 DTD
DOCTYPE / ENTITY 拒绝
```

---

## 3. 数据结构建议

```cpp
struct ThreeMfColorGroup {
    std::string id;
    std::vector<std::array<std::uint8_t, 3>> colors;
};

struct ThreeMfTexture2D {
    std::string id;
    std::string path;
    std::string content_type;
    TextureImage image;
    bool loaded{false};
};

struct ThreeMfTex2Coord {
    double u{0.0};
    double v{0.0};
};

struct ThreeMfTexture2DGroup {
    std::string id;
    std::string texid;
    std::vector<ThreeMfTex2Coord> coords;
};
```

Triangle property：

```cpp
struct ThreeMfTriangleProperty {
    std::string pid;
    int p1{-1};
    int p2{-1};
    int p3{-1};
};
```

---

## 4. ColorGroup 解析

解析：

```text
<colorgroup id="...">
  <color color="#RRGGBB" />
</colorgroup>
```

建立映射：

```text
group id -> vector RGB
```

triangle resolve：

```text
pid = colorgroup id
p1/p2/p3 = color indices
```

第一版简化策略：

```text
p1/p2/p3 相同：直接用该色
p1/p2/p3 不同：平均三色作为 triangle RGB
```

并记录：

```text
interpolatedColorFallbackCount
```

---

## 5. Texture2D / Texture2DGroup 解析

解析：

```text
<texture2d id="..." path="/3D/Textures/tex.png" contenttype="image/png" />
<texture2dgroup id="..." texid="...">
  <tex2coord u="..." v="..." />
</texture2dgroup>
```

建立映射：

```text
texture id -> TextureImage
texture group id -> texture id + UV coords
```

triangle resolve：

```text
pid = texture2dgroup id
p1/p2/p3 = texcoord indices
```

转换为当前已有的 triangle texture metadata：

```text
TriangleTextureInfo:
  material_name
  uv0/uv1/uv2
  texture path/source
```

第一版应尽量复用现有 OBJ/MTL TextureSampler。

---

## 6. 3MF 内部 Texture Loading

3MF texture path 必须从 ZIP entries 读取：

```text
/3D/Textures/tex.png
3D/Textures/tex.png
```

路径规则：

```text
去掉开头 /
标准化 \
拒绝 ..
不访问文件系统外部路径
```

当前 TextureImage loader 基于文件路径和 Windows WIC。

06B 可选实现策略：

### 6.1 临时文件策略

```text
将 3MF 内部 texture entry 安全写入 output/cache 或 temp 目录；
调用现有 load_texture_image(path)；
完成后可保留用于 debug 或清理。
```

优点：

```text
改动小，复用现有 WIC loader。
```

缺点：

```text
需要管理临时文件生命周期。
```

### 6.2 内存解码策略

```text
新增 load_texture_image_from_memory(bytes)。
```

优点：

```text
更干净。
```

缺点：

```text
当前 WIC memory stream 实现量更大。
```

第一版推荐：

```text
临时文件策略
```

后续再演进为内存解码。

---

## 7. 与 MaterialRoleMapping 的结合

解析出 RGB source 后：

```text
ColorGroup / Texture2DGroup 默认 role = rgb
```

如果 object / material name 命中：

```text
white / varnish / ignore
```

则按 MaterialRoleMapping 处理。

第一版不要求 texture-driven white/varnish mask。

---

## 8. Report 增强

`three_mf_report.json` 新增：

```json
{
  "colorGroups": {
    "count": 0,
    "colorCount": 0,
    "resolvedTriangles": 0,
    "interpolatedColorFallbackCount": 0
  },
  "textures": {
    "texture2dCount": 0,
    "texture2dGroupCount": 0,
    "tex2CoordCount": 0,
    "resourceCount": 0,
    "loadedCount": 0,
    "missingCount": 0,
    "sampledPixels": 0
  }
}
```

`texture_report.json` 记录：

```text
source = 3mf_internal
```

---

## 9. Bad 3MF 增强

新增 bad package：

```text
bad_3mf_texture_path_missing
bad_3mf_texture_decode_failed
bad_3mf_texture2dgroup_missing_texid
bad_3mf_tex2coord_index_out_of_range
bad_3mf_colorgroup_index_out_of_range
bad_3mf_unsupported_compositematerials
bad_3mf_unsupported_multiproperties
```

---

## 10. 回归

`run_regression.ps1 -Mode quick` 增加：

```text
3MF ColorGroup positive
3MF Texture2DGroup positive
3MF unsupported resource fallback
```

`-Mode full` 增加：

```text
更多 bad texture/color group negative tests
```

---

## 11. 实施顺序

```text
1. XML parser 方案确认；
2. ColorGroup 解析与 positive sample；
3. Texture2D / Texture2DGroup 解析；
4. 3MF internal texture loading；
5. triangle property → RGB source resolve；
6. report 增强；
7. bad packages；
8. quick regression；
9. REPORT_06B。
```

---

## 12. 非目标

不做：

```text
PBR
ICC / CMYK
OpenVDB
Qt UI
RIP 半色调
CompositeMaterials / MultiProperties 完整实现
texture-driven white/varnish mask
```

---

## 13. 结论

DEV_06B 的重点是：

```text
让 3MF 的基础颜色与贴图资源接入现有 RGB 采样链路。
```
