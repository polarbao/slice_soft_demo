# PRD_05A_真实材料工艺参数验证

> 文档版本：v0.1  
> 文档状态：Draft / PRD  
> 适用阶段：REPORT_06B 之后  
> 所属模块：MaterialPolicy / MaterialRoleMapping / Reports / Regression  
> 建议提交目录：`docs/slicer/`

## 1. 背景

当前项目已具备：

```text
OBJ/MTL/Texture
3MF basematerial
3MF ColorGroup
3MF Texture2DGroup
MaterialRoleMapping
MaterialPolicy
Support
p0.rgbwsv.2 输出
```

05A 要把这些能力推进为：

```text
可复用、可比较、可回归的材料工艺 profile。
```

## 2. 产品目标

建立真实材料工艺参数验证体系：

```text
MaterialProcessProfile
→ RGB / W / V / S 输出
→ 通道统计
→ 层分布统计
→ profile compare
→ material_process_report.json
```

## 3. MaterialProcessProfile 配置

新增：

```json
{
  "materialProcessProfile": {
    "enabled": true,
    "name": "nail_rgb_white_varnish_v1",
    "target": "uv_relief_nail",
    "rgb": {
      "enabled": true,
      "source": "texture_or_color"
    },
    "white": {
      "enabled": true,
      "mode": "underbase",
      "coverage": "all_model",
      "value": 0,
      "expandPx": 0,
      "shrinkPx": 0
    },
    "varnish": {
      "enabled": true,
      "mode": "top_n_layers",
      "topLayers": 2,
      "value": 0,
      "coverage": "model_surface"
    },
    "support": {
      "expected": true,
      "mode": "existing_support_pipeline"
    },
    "validation": {
      "requireRgbPixels": true,
      "requireWhitePixels": true,
      "requireVarnishPixels": true,
      "requireSupportPixels": false,
      "maxUnexpectedOverlapPixels": 0
    }
  }
}
```

## 4. 与现有 MaterialPolicy 的关系

```text
MaterialProcessProfile = 工艺验收层 / profile 层
MaterialPolicy = 当前具体材料组合执行层
```

第一版推荐：

```text
MaterialProcessProfile 先作为 validation/report 层；
不强制覆盖 MaterialPolicy，避免大改现有执行逻辑。
```

## 5. Report 需求

新增：

```text
reports/material_process_report.json
```

字段：

```text
enabled
profileName
target
inputFormat
sourceModel
grid
layerCount
rgb.printPixels
white.printPixels
varnish.printPixels
support.printPixels
rgb.coverageRatio
white.coverageRatio
varnish.coverageRatio
support.coverageRatio
varnish.topLayers
varnish.activeLayerIndices
white.missingUnderbasePixels
unexpectedOverlapPixels
validation.pass
validation.failures
warnings
```

新增可选：

```text
reports/material_profile_compare_report.json
```

## 6. 样例配置

新增目录：

```text
samples/configs/material_process/
```

建议样例：

```text
nail_rgb_white_varnish_top1.json
nail_rgb_white_varnish_top2.json
nail_rgb_white_varnish_top3.json
nail_white_underbase_only.json
nail_varnish_only.json
three_mf_texture_rgb_white_varnish.json
obj_mtl_texture_rgb_white_varnish.json
```

## 7. 验收标准

1. `material_process_report.json` 输出。
2. RGB + W + V profile 可跑通。
3. topLayers=1/2/3 可生成不同 V 层分布。
4. W underbase 覆盖模型有效 RGB 区域。
5. V top_n_layers 只出现在顶部层范围。
6. 3MF Texture2DGroup 模型可使用 profile。
7. OBJ/MTL Texture 模型可使用 profile。
8. Support S 与 MaterialPolicy 不互相覆盖。
9. `rip_reader_test --summary` 通过。
10. `run_regression.ps1 -Mode quick` 通过。
11. p0.rgbwsv.2 输出协议不变。

## 8. 非目标

```text
ICC / CMYK
RIP 半色调
真实喷头 bitstream
设备通信
OpenVDB
Qt UI
3MF CompositeMaterials 完整语义
PBR
支撑形态大改
```
