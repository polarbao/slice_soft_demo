# DEMO_06_v0.2_3MF与OBJ_MTL多材料输入验证方案

> 文档版本：v0.2  
> 文档状态：Draft / DEMO  
> 建议提交目录：`docs/slicer/`

## 1. Demo 目标

验证 06 的两条输入路径：

```text
3MF material/color → MaterialRoleMapping → RGB/W/V
OBJ usemtl + MTL → MaterialRoleMapping → RGB/W/V
```

最终仍输出：

```text
p0.rgbwsv.2 SlicePackage
```

## 2. 样例目录

新增：

```text
samples/models/3mf/
samples/configs/3mf/

samples/models/material_mapping/
samples/configs/material_mapping/
```

## 3. 必须验证样例：3MF

### 3.1 Single RGB 3MF

```text
three_mf_single_rgb.json
```

验收：

```text
model_report.format = 3mf
three_mf_report.objectCount > 0
RGB printPixels > 0
rip_reader_test --summary pass
```

### 3.2 Multi Object Transform

```text
three_mf_multi_object_transform.json
```

验收：

```text
componentCount > 0
bbox_mm 符合 transform 后结果
triangleCount > 0
rip_reader_test --summary pass
```

### 3.3 Multi Material RGB/W/V

```text
three_mf_multi_material_rgbwv.json
```

验收：

```text
mappedRgb > 0
mappedWhite > 0
mappedVarnish > 0
RGB/W/V printPixels > 0
S support 不被输入材料错误覆盖
rip_reader_test --summary pass
```

## 4. 必须验证样例：OBJ/MTL

### 4.1 OBJ/MTL RGB/W/V Material Mapping

```text
obj_mtl_material_mapping_rgbwv.json
```

OBJ/MTL 材料名示例：

```text
color_body
white_base
varnish_top
```

验收：

```text
material_role_mapping_report.mappedRgb > 0
material_role_mapping_report.mappedWhite > 0
material_role_mapping_report.mappedVarnish > 0
RGB/W/V printPixels > 0
rip_reader_test --summary pass
```

### 4.2 OBJ/MTL Ignore Role

```text
obj_mtl_material_mapping_ignore.json
```

验收：

```text
mappedIgnore > 0
ignore 材料区域不输出 RGB/W/V
rip_reader_test --summary pass
```

### 4.3 OBJ/MTL Texture + Role Mapping

```text
obj_mtl_texture_material_mapping_rgbwv.json
```

验收：

```text
RGB role 材料继续使用 map_Kd texture
white role 写 W
varnish role 写 V
texture_report.sampledPixels > 0
material_role_mapping_report 有 RGB/W/V 统计
```

## 5. 验证命令

```powershell
cmake --build build --config Debug

build\\Debug\\slicer_cli.exe --config samples\\configs\\3mf\\three_mf_multi_material_rgbwv.json
build\\Debug\\rip_reader_test.exe --package output\\ThreeMfMultiMaterialRgbwv --summary

build\\Debug\\slicer_cli.exe --config samples\\configs\\material_mapping\\obj_mtl_material_mapping_rgbwv.json
build\\Debug\\rip_reader_test.exe --package output\\ObjMtlMaterialMappingRgbwv --summary

.\\scripts\\run_regression.ps1 -Mode quick
```

## 6. 回归 Checklist

- [ ] 3MF single RGB pass。
- [ ] 3MF multi object transform pass。
- [ ] 3MF multi material RGB/W/V pass。
- [ ] OBJ/MTL RGB/W/V material mapping pass。
- [ ] OBJ/MTL ignore role pass。
- [ ] OBJ/MTL texture + material role pass。
- [ ] material_role_mapping_report.json 存在。
- [ ] three_mf_report.json 存在。
- [ ] obj_mtl_material_report.json 存在。
- [ ] p0.rgbwsv.2 不变。
- [ ] quick regression pass。
- [ ] 03C summary / quiet 仍可用。

## 7. 非目标

```text
完整 3MF texture2dgroup
PBR
Production Extension
Beam Lattice
Slice Stack
OBJ alpha 正式语义
texture-driven varnish mask
texture-driven white mask
OpenVDB
Qt UI
RIP 半色调
ICC / CMYK
```

## 8. 状态报告

完成后生成：

```text
docs/slicer/REPORT_06_3MF与OBJ_MTL多材料输入当前实现状态.md
```
