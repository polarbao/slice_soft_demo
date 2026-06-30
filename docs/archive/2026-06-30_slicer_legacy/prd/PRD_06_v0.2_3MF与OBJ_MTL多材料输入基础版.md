# PRD_06_v0.2_3MF与OBJ_MTL多材料输入基础版

> 文档版本：v0.2  
> 文档状态：Draft / PRD  
> 建议提交目录：`docs/slicer/`

## 1. 背景

当前项目已支持：

```text
OBJ / STL 基础模型输入
OBJ + MTL + Texture 彩色纹理输入
RGB/W/V MaterialPolicy
Support S 通道
p0.rgbwsv.2 输出协议
stripped/tiled TIFF storageMode
quick/full/heavy regression
```

06 阶段修订后要同时支持：

```text
3MF 多材料输入
OBJ/MTL 多材料角色映射
```

## 2. 产品目标

建立统一材料角色映射层：

```text
Input Material
→ MaterialRoleMapping
→ Internal Role: rgb / white / varnish / ignore / support_candidate
→ RGBWSV output
```

适用输入格式：

```text
3MF
OBJ + MTL
OBJ + MTL + Texture
```

## 3. 用户场景

### 3.1 3MF 多材料

```text
3MF basematerials / color / object / component
→ 根据材质名或颜色规则映射到 RGB/W/V
```

### 3.2 OBJ/MTL 多材料

```text
OBJ usemtl + MTL newmtl
材质名示例：
  color_body
  white_base
  varnish_top

映射：
  color_body → RGB
  white_base → W
  varnish_top → V
```

### 3.3 OBJ/MTL + Texture + 多材料

```text
RGB role 材料继续使用 texture/map_Kd；
white role 材料写 W；
varnish role 材料写 V。
```

## 4. 统一 MaterialRoleMapping 配置

新增统一配置：

```json
{
  "materialRoleMapping": {
    "enabled": true,
    "mode": "rules_then_default",
    "defaultRole": "rgb",
    "allowInputSupportMaterial": false,
    "rules": [
      { "matchNameContains": "white", "role": "white" },
      { "matchNameContains": "varnish", "role": "varnish" },
      { "matchNameContains": "clear", "role": "varnish" },
      { "matchNameContains": "ignore", "role": "ignore" }
    ]
  }
}
```

## 5. Role 定义

```text
rgb:
  写 RGB，颜色来自 texture / Kd / 3MF color / fallback

white:
  写 W = 0

varnish:
  写 V = 0

ignore:
  不输出该材料区域

support_candidate:
  只记录候选支撑，不写 S

support:
  仅 allowInputSupportMaterial=true 时允许写 S
```

默认：

```text
allowInputSupportMaterial = false
```

## 6. 必须支持范围

### 6.1 3MF

```text
ZIP package 打开
[Content_Types].xml 基础检查
_rels/.rels 定位 model part
3D/3dmodel.model 读取
model unit
resources
object / mesh / vertices / triangles
build item
component objectid
component transform
basematerials / display color
```

### 6.2 OBJ/MTL

```text
OBJ usemtl
OBJ face material assignment
MTL newmtl
MTL Kd diffuse color
MTL map_Kd texture path
facesWithMaterial / facesWithoutMaterial 统计
material name based role mapping
```

## 7. 与 MaterialPolicy 的关系

MaterialRoleMapping 负责：

```text
输入材料的局部/分区角色映射
```

MaterialPolicy 继续负责：

```text
全局 W underbase
全局 V top_n_layers
旧配置兼容
```

第一版执行顺序建议：

```text
输入材料解析
→ MaterialRoleMapping
→ Texture / Kd / 3MF color resolve
→ slicing 得到 model masks
→ Material composition
→ 可选 MaterialPolicy overlay
→ Support composition
→ TIFF writer
```

## 8. 输出语义

继续遵守：

```text
schema = p0.rgbwsv.2
RGBWSV
uint8
black_is_print
```

映射后：

```text
rgb role:
  RGB = material color / texture
  W/S/V = 255

white role:
  W = 0
  RGB/S/V = 255

varnish role:
  V = 0
  RGB/W/S = 255

ignore role:
  all = 255
```

## 9. Report 需求

新增统一报告：

```text
reports/material_role_mapping_report.json
```

字段：

```text
enabled
inputFormat
rules
defaultRole
allowInputSupportMaterial
materialCount
mappedRgb
mappedWhite
mappedVarnish
mappedIgnore
mappedSupportCandidate
facesWithMappedMaterial
facesWithoutMappedMaterial
warnings
```

3MF 额外：

```text
reports/three_mf_report.json
```

OBJ/MTL 额外：

```text
reports/obj_mtl_material_report.json
```

## 10. 验收标准

1. 3MF single RGB 可切片。
2. 3MF multi material 可映射 RGB/W/V。
3. OBJ/MTL material name 可映射 RGB/W/V。
4. OBJ/MTL + Texture 的 RGB role 继续使用 texture。
5. white role 可写 W。
6. varnish role 可写 V。
7. ignore role 不输出。
8. 默认不允许输入材料直接写 S。
9. `material_role_mapping_report.json` 输出。
10. `three_mf_report.json` 输出。
11. `obj_mtl_material_report.json` 输出。
12. `rip_reader_test --summary` 通过。
13. `run_regression.ps1 -Mode quick` 通过。
14. 不改变 p0.rgbwsv.2 输出协议。

## 11. 非目标

```text
完整 3MF Texture2DGroup
完整 3MF Production Extension
PBR / metallic / roughness
OBJ alpha / transparency 正式语义
texture-driven varnish mask
texture-driven white mask
OpenVDB
新体素内核
Qt UI
RIP 半色调
ICC / CMYK
复杂支撑形态优化
```
