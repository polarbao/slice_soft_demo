# REPORT_06_3MF与OBJ_MTL多材料输入当前实现状态

> 日期：2026-06-08  
> 阶段：06 / 3MF 与 OBJ/MTL 多材料输入基础版  
> 状态：已完成实现与 quick regression 验证

---

## 1. 本阶段目标

06 阶段在不改变输出协议的前提下，补齐多材料输入基础链路：

- OBJ/MTL 材质名称、Kd、map_Kd 到 MaterialRole 的映射。
- 3MF stored ZIP 包、`3D/3dmodel.model`、mesh、components、basematerials/displaycolor 的基础读取。
- 将输入材料统一映射到 `rgb / white / varnish / ignore / support_candidate`。
- 输出新增报告，便于判断输入材料如何进入 RGB/W/V 通道。

本阶段保持不变：

- `schema = p0.rgbwsv.2`
- `channelOrder = R G B W S V`
- `bitDepth = 8`
- `polarity = black_is_print`
- `0=打印，255=不打印`
- TIFF `stripped / tiled` 兼容逻辑
- 支撑仍由现有 Support 系统生成，输入材料默认不直接写 S 通道

---

## 2. 新增配置

新增根级配置：

```json
"materialRoleMapping": {
  "enabled": true,
  "mode": "rules_then_default",
  "defaultRole": "rgb",
  "allowInputSupportMaterial": false,
  "rules": [
    { "matchNameContains": "white", "role": "white" },
    { "matchNameContains": "varnish", "role": "varnish" },
    { "matchNameContains": "ignore", "role": "ignore" },
    { "matchNameContains": "support", "role": "support_candidate" }
  ]
}
```

当前支持：

- `mode`: 仅支持 `rules_then_default`。
- `defaultRole`: 默认 `rgb`。
- `allowInputSupportMaterial`: 默认 `false`。
- `rules[].matchNameContains`: 大小写不敏感包含匹配。
- `role`: `rgb / white / varnish / ignore / support_candidate / support`。

约束：

- 当 `allowInputSupportMaterial=false` 时，配置 `role=support` 会被拒绝。
- `support_candidate` 只记录候选，不直接写 S；S 通道仍由 Support pipeline 生成。

---

## 3. OBJ/MTL 支持范围

已支持：

- OBJ `mtllib`。
- OBJ `usemtl`。
- face material assignment。
- MTL `newmtl`。
- MTL `Kd` diffuse RGB。
- MTL `map_Kd` 贴图路径。
- RGB role 使用 texture / Kd / fallback。
- white role 写 W 通道。
- varnish role 写 V 通道。
- ignore role 不输出模型材料。
- `support_candidate` 不写 S，仅在报告中记录。

新增报告：

```text
reports/obj_mtl_material_report.json
reports/material_role_mapping_report.json
```

已验证样例：

- `samples/configs/material_mapping/obj_mtl_material_mapping_rgbwv.json`
- `samples/configs/material_mapping/obj_mtl_material_mapping_ignore.json`
- `samples/configs/material_mapping/obj_mtl_texture_material_mapping_rgbwv.json`

验证结果摘录：

- `ObjMtlMaterialMappingRgbwv`: `materialCount=3`，`mappedRgb=1`，`mappedWhite=1`，`mappedVarnish=1`。
- `ObjMtlTextureMaterialMappingRgbwv`: `loadedTextures=1`，`sampledPixels=5320`，`fallbackPixels=0`。

---

## 4. 3MF 支持范围

已支持：

- `.3mf` 格式自动识别。
- stored ZIP entry 读取。
- `[Content_Types].xml` 存在性检查。
- `_rels/.rels` model part 定位。
- fallback 到 `3D/3dmodel.model`。
- ZIP entry path traversal 防护。
- ZIP entry 数量和总解包大小限制。
- 解析 `model unit` 并转换到 mm。
- 解析 mesh object / vertices / triangles。
- 解析 build item。
- 解析 components object。
- 解析 component transform。
- 解析 basematerials name / displaycolor。
- 将 3MF material 转为 `MaterialInfo`。
- 将 triangle material 写入 `triangle_textures.material_name`。
- 调用 `materialRoleMapping` 进入 RGB/W/V 输出。

新增报告：

```text
reports/three_mf_report.json
reports/material_role_mapping_report.json
```

已验证样例：

- `samples/configs/3mf/three_mf_single_rgb.json`
- `samples/configs/3mf/three_mf_multi_object_transform.json`
- `samples/configs/3mf/three_mf_multi_material_rgbwv.json`

验证结果摘录：

- `ThreeMfSingleRgb`: `format=3mf`，`objectCount>0`，`triangleCount>0`。
- `ThreeMfMultiObjectTransform`: `objectCount=2`，`componentCount=2`，`meshObjectCount=1`，`triangleCount=8`。
- `ThreeMfMultiMaterialRgbwv`: `materialResourceCount=3`，`mappedRgb=1`，`mappedWhite=1`，`mappedVarnish=1`。

---

## 5. Reports 与 Manifest

06 阶段新增或增强：

- `manifest.json`
  - `source.format`
  - `reports.materialRoleMapping`
  - `reports.objMtlMaterial`
  - `reports.threeMf`
- `reports/material_role_mapping_report.json`
  - `enabled`
  - `inputFormat`
  - `rules`
  - `defaultRole`
  - `allowInputSupportMaterial`
  - `mappedRgb / mappedWhite / mappedVarnish / mappedIgnore / mappedSupportCandidate / mappedSupport`
  - `facesWithMappedMaterial / facesWithoutMappedMaterial`
  - `materials`
  - `warnings`
- `reports/obj_mtl_material_report.json`
  - `inputFormat`
  - `materialCount`
  - `materials`
  - `facesWithMaterial / facesWithoutMaterial`
  - `textures`
- `reports/three_mf_report.json`
  - `enabled`
  - `packagePath`
  - `modelPartPath`
  - `unit`
  - `unitScaleToMm`
  - `objectCount`
  - `componentCount`
  - `meshObjectCount`
  - `triangleCount`
  - `materialResourceCount`
  - `unsupportedExtensions`
  - `warnings`

---

## 6. 样例与回归

新增模型样例：

```text
samples/models/3mf/single_rgb_cube.3mf
samples/models/3mf/multi_object_transform.3mf
samples/models/3mf/multi_material_rgb_white_varnish.3mf
samples/models/material_mapping/obj_mtl_rgb_white_varnish.obj
samples/models/material_mapping/obj_mtl_rgb_white_varnish.mtl
samples/models/material_mapping/obj_mtl_ignore.obj
samples/models/material_mapping/obj_mtl_ignore.mtl
samples/models/material_mapping/obj_mtl_texture_rgbwv.obj
samples/models/material_mapping/obj_mtl_texture_rgbwv.mtl
```

新增配置样例：

```text
samples/configs/3mf/three_mf_single_rgb.json
samples/configs/3mf/three_mf_multi_object_transform.json
samples/configs/3mf/three_mf_multi_material_rgbwv.json
samples/configs/material_mapping/obj_mtl_material_mapping_rgbwv.json
samples/configs/material_mapping/obj_mtl_material_mapping_ignore.json
samples/configs/material_mapping/obj_mtl_texture_material_mapping_rgbwv.json
```

`scripts/run_regression.ps1 -Mode quick` 已新增：

- `materialMappingCases`
- `threeMfCases`
- OBJ/MTL material role mapping 断言
- OBJ/MTL texture sampledPixels 断言
- 3MF format/object/component/material mapping 断言

---

## 7. 已运行验证

已运行并通过：

```powershell
cmake --build build --config Debug
build\Debug\slicer_cli.exe --config samples\configs\3mf\three_mf_multi_material_rgbwv.json
build\Debug\slicer_cli.exe --config samples\configs\material_mapping\obj_mtl_material_mapping_rgbwv.json
build\Debug\rip_reader_test.exe --package output\ThreeMfMultiMaterialRgbwv --summary
build\Debug\rip_reader_test.exe --package output\ObjMtlMaterialMappingRgbwv --summary
.\scripts\run_regression.ps1 -Mode quick
```

RIP reader 摘要确认：

```text
ThreeMfMultiMaterialRgbwv:
schema=p0.rgbwsv.2
storageMode=stripped
bitDepth=8
channelOrder=R G B W S V
channelPrintPixels: R=4760 G=4760 B=4760 W=4480 S=3430 V=4480
warnings=0
```

```text
ObjMtlMaterialMappingRgbwv:
schema=p0.rgbwsv.2
storageMode=stripped
bitDepth=8
channelOrder=R G B W S V
channelPrintPixels: R=4760 G=4760 B=4760 W=4480 S=3430 V=4480
warnings=0
```

---

## 8. 当前未支持范围

3MF 当前仍是基础子集，不支持：

- deflate 压缩 ZIP entry。
- ZIP64。
- 3MF Texture2D / Texture2DGroup。
- 3MF ColorGroup。
- 3MF CompositeMaterials。
- 3MF MultiProperties。
- Production extension。
- Beam lattice / slice extension。
- PBR material。
- alpha / 透明度。
- 加密或外部关系资源。

OBJ/MTL 当前仍不支持：

- 多纹理层。
- PBR 参数。
- alpha / dissolve 参与输出。
- bump/normal/roughness 等贴图。
- 每顶点颜色。

本阶段明确未做：

- OpenVDB。
- Qt UI。
- RIP 半色调。
- 复杂支撑树。
- 新输出 schema。

---

## 9. 下一阶段建议

建议优先进入 `06A`，对 3MF importer 做兼容性增强与负向测试：

- 支持 deflate 3MF 包，或明确提供 stored-only 转换工具。
- 增加 bad 3MF package 用例：缺少 model part、非法关系、path traversal、过大 entry、未知 material id。
- 将 XML 解析从轻量字符串解析替换为受限 XML parser，降低真实 3MF 文件兼容风险。
- 扩展 `three_mf_report.json`，记录被忽略的 extension 和 material group 类型。

如果业务优先级转向打印效果，再进入 `05A` 强化白墨/光油策略；如果优先集成设备链路，再进入 `07`。
