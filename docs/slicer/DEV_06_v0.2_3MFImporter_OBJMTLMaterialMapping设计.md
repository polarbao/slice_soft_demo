# DEV_06_v0.2_3MFImporter_OBJMTLMaterialMapping设计

> 文档版本：v0.2  
> 文档状态：Draft / DEV  
> 建议提交目录：`docs/slicer/`

## 1. 技术目标

新增统一材料角色映射层，使以下输入都能映射到 RGB/W/V：

```text
3MF material/color
OBJ usemtl
MTL newmtl / Kd / map_Kd
```

不新增几何内核，不改变 p0.rgbwsv.2 输出协议。

## 2. 推荐模块

建议新增：

```text
src/slicer_core/material_mapping/
  material_role_mapping.*
  material_role_report.*

src/slicer_core/three_mf/
  three_mf_package.*
  three_mf_xml.*
  three_mf_importer.*
  three_mf_report.*

src/slicer_core/obj_mtl/
  obj_mtl_material_report.*
```

如果当前不拆目录，也必须形成以下函数边界：

```text
map_input_material_to_role
apply_obj_mtl_material_mapping
apply_3mf_material_mapping
write_material_role_mapping_report
write_obj_mtl_material_report
write_three_mf_report
```

## 3. 统一配置结构

```cpp
struct MaterialRoleRule {
    std::string match_name_contains;
    std::string role; // rgb / white / varnish / ignore / support_candidate / support
};

struct MaterialRoleMappingConfig {
    bool enabled{true};
    std::string mode{"rules_then_default"};
    std::string default_role{"rgb"};
    bool allow_input_support_material{false};
    std::vector<MaterialRoleRule> rules;
};
```

`SliceConfig` 新增：

```cpp
MaterialRoleMappingConfig material_role_mapping;
```

## 4. MaterialRole

```cpp
enum class MaterialRole {
    Rgb,
    White,
    Varnish,
    Ignore,
    SupportCandidate,
    Support
};
```

默认：

```text
SupportCandidate 不写 S；
Support 仅 allow_input_support_material=true 时生效。
```

## 5. OBJ/MTL Material Mapping

### 5.1 输入

当前 OBJ/MTL 已具备或应具备：

```text
face.material_name
MaterialInfo.name
MaterialInfo.diffuse_rgb
MaterialInfo.diffuse_texture_path
TriangleTextureInfo
```

### 5.2 映射流程

```text
for each material in material_infos:
  role = map_input_material_to_role(material.name)
  record role

for each face:
  material_name = face.material_name
  role = material_role_map[material_name]
  write role metadata to triangle / face material context
```

### 5.3 RGB role

RGB role 使用优先级：

```text
texture sampled RGB
MTL Kd diffuse color
config fallbackRgb
modelMaterial.rgb
```

### 5.4 White / Varnish role

```text
white role:
  W = 0

varnish role:
  V = 0
```

第一版可对该 material 所属 face 的占据区域写入对应通道。

## 6. 3MF Importer

### 6.1 Package

支持：

```text
ZIP open
[Content_Types].xml
_rels/.rels
3D/3dmodel.model
```

### 6.2 XML

支持：

```text
model unit
resources
object
mesh vertices
mesh triangles
build items
components
component transform
basematerials / display color
```

### 6.3 安全

必须处理：

```text
path traversal
过大文件
过多文件
压缩炸弹风险
```

## 7. 统一 InputMaterialInfo

```cpp
struct InputMaterialInfo {
    std::string source_format; // obj_mtl / 3mf
    std::string id;
    std::string name;
    std::array<std::uint8_t, 3> rgb;
    bool has_rgb{false};
};
```

统一调用：

```cpp
MaterialRole map_input_material_to_role(
    const InputMaterialInfo& material,
    const MaterialRoleMappingConfig& config);
```

## 8. Report

### 8.1 material_role_mapping_report.json

```json
{
  "enabled": true,
  "inputFormat": "obj_mtl",
  "defaultRole": "rgb",
  "allowInputSupportMaterial": false,
  "stats": {
    "materialCount": 0,
    "mappedRgb": 0,
    "mappedWhite": 0,
    "mappedVarnish": 0,
    "mappedIgnore": 0,
    "mappedSupportCandidate": 0,
    "facesWithMappedMaterial": 0,
    "facesWithoutMappedMaterial": 0
  },
  "warnings": []
}
```

### 8.2 obj_mtl_material_report.json

```json
{
  "materialCount": 0,
  "materials": [],
  "facesWithMaterial": 0,
  "facesWithoutMaterial": 0,
  "textures": []
}
```

### 8.3 three_mf_report.json

记录：

```text
packagePath
modelPartPath
unit
unitScaleToMm
objectCount
componentCount
meshObjectCount
triangleCount
materialResourceCount
unsupportedExtensions
warnings
```

## 9. 样例

OBJ/MTL：

```text
samples/models/material_mapping/
  obj_mtl_rgb_white_varnish.obj
  obj_mtl_rgb_white_varnish.mtl
  textures/checker.png

samples/configs/material_mapping/
  obj_mtl_material_mapping_rgbwv.json
  obj_mtl_material_mapping_ignore.json
  obj_mtl_texture_material_mapping_rgbwv.json
```

3MF：

```text
samples/models/3mf/
  single_rgb_cube.3mf
  multi_object_transform.3mf
  multi_material_rgb_white_varnish.3mf

samples/configs/3mf/
  three_mf_single_rgb.json
  three_mf_multi_object_transform.json
  three_mf_multi_material_rgbwv.json
```

## 10. 回归

`run_regression.ps1 -Mode quick` 增加小型 3MF 和 OBJ/MTL material mapping cases。

建议新增 case 分组：

```text
materialMappingCases
threeMfCases
```

## 11. 非目标

```text
OpenVDB
新的 GeometryKernel
完整 3MF Texture2DGroup
PBR
OBJ alpha 正式语义
texture-driven varnish/white mask
ICC / CMYK
RIP 半色调
Qt UI
```
