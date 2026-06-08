# REPORT_04A_纹理阶段收口修复当前实现状态

> 文档版本：v0.1  
> 文档状态：当前实现状态  
> 阶段范围：04A / 纹理 Fallback 样例与支撑割裂诊断收口  
> 生成时间：2026-06-08

---

## 1. 阶段结论

04A 已完成 04 彩色纹理基础阶段的收口修复：

```text
1. Missing texture fallback 已恢复为小型 fixture 验证
2. No-UV fallback 已恢复为小型 fixture 验证
3. fallback 用例不再依赖 38MB 真实大模型
4. support connectivity diagnostics 已写入 report
5. run_regression.ps1 已加入 fallback 语义校验
6. 完整回归已通过
```

04A 未改变 RGBWSV 协议：

```text
schema = p0.rgbwsv.1
channelOrder = R G B W S V
bitDepth = 8
polarity = black_is_print
printValue = 0
emptyValue = 255
Model > Support > Empty
```

---

## 2. Fixture 修复

### 2.1 Missing Texture Small

新增：

```text
samples/models/textured/fixtures/missing_texture_small.obj
samples/models/textured/fixtures/missing_texture_small.mtl
```

特征：

```text
OBJ 包含 vt
MTL 包含 map_Kd
map_Kd 指向不存在的 textures/does_not_exist.png
小型 relief_heightfield 体块，快速生成 20 层
```

配置已切换：

```text
samples/configs/textured/textured_missing_texture_fallback.json
→ ../../models/textured/fixtures/missing_texture_small.obj
```

验证结果：

```text
grid = 48 x 24 x 20
missingTextures = 1
warnings = 1
facesWithUv = 4
sampledPixels = 0
fallbackPixels = 22560
rip_reader_test = pass
```

### 2.2 No UV Small

新增：

```text
samples/models/textured/fixtures/no_uv_small.obj
samples/models/textured/fixtures/no_uv_small.mtl
```

特征：

```text
OBJ 不包含 vt
face 使用 v 格式
MTL 使用 Kd 作为 fallback 颜色来源
小型 relief_heightfield 体块，快速生成 20 层
```

配置已切换：

```text
samples/configs/textured/textured_no_uv_fallback.json
→ ../../models/textured/fixtures/no_uv_small.obj
```

验证结果：

```text
grid = 48 x 24 x 20
facesWithUv = 0
facesWithoutUv = 4
sampledPixels = 0
fallbackPixels = 22560
rip_reader_test = pass
```

---

## 3. Heavy Model 分流

当前 38MB 真实纹理模型仍保留在：

```text
samples/models/textured/
```

为了避免快速 fallback 回归误用大模型，新增说明目录：

```text
samples/models/textured/heavy/README.md
samples/configs/textured/heavy/README.md
```

当前默认 fallback configs 已不再指向 38MB 模型。`TexturedReliefRgb` 主样例仍使用真实纹理模型，作为重型主链路验证。

---

## 4. 支撑连通性诊断

04A 新增 report 级支撑连通域诊断，输出位置：

```text
reports/support_report.json
reports/slice_report.json
```

每层字段：

```json
"supportConnectivity": {
  "enabled": true,
  "componentCount": 0,
  "largestComponentPixels": 0,
  "smallComponentCount": 0,
  "tinyComponentCount": 0,
  "tinyComponentAreaPx": 8,
  "smallComponentAreaPx": 512,
  "components": [
    {
      "areaPx": 0,
      "bbox": {
        "minX": 0,
        "minY": 0,
        "maxX": 0,
        "maxY": 0
      }
    }
  ]
}
```

顶层 summary 字段：

```text
supportConnectivity.enabled
supportConnectivity.layersWithSupportComponents
supportConnectivity.layersWithFragmentation
supportConnectivity.maxComponentCount
supportConnectivity.layerWithMaxComponentCount
supportConnectivity.maxSmallComponentCount
supportConnectivity.maxTinyComponentCount
```

### 4.1 TexturedReliefRgb 第 68 层诊断

当前输出：

```text
componentCount = 3
largestComponentPixels = 181150
smallComponentCount = 1
tinyComponentCount = 1
```

components：

```text
areaPx = 181150, bbox = x 6..276, y 0..716
areaPx = 383,    bbox = x 280..287, y 500..573
areaPx = 4,      bbox = x 3..3, y 475..478
```

顶层 summary：

```text
layersWithFragmentation = 322
maxComponentCount = 8
```

结论保持 04 报告判断：

```text
该现象是支撑形态策略问题，不是 texture RGB 覆盖 support，也不是 RGBWSV 协议错误。
04A 只做诊断输出，不做支撑形态修复。
```

---

## 5. 回归

已运行：

```powershell
cmake --build build --config Debug

build\Debug\slicer_cli.exe --config samples\configs\textured\textured_missing_texture_fallback.json
build\Debug\rip_reader_test.exe --package output\TexturedMissingTextureFallback

build\Debug\slicer_cli.exe --config samples\configs\textured\textured_no_uv_fallback.json
build\Debug\rip_reader_test.exe --package output\TexturedNoUvFallback

build\Debug\slicer_cli.exe --config samples\configs\textured\textured_relief_rgb.json
build\Debug\rip_reader_test.exe --package output\TexturedReliefRgb

.\scripts\run_regression.ps1
```

结果：

```text
TexturedMissingTextureFallback = pass
TexturedNoUvFallback = pass
TexturedReliefRgb = pass
run_regression.ps1 = pass
最终输出 Regression complete.
```

`scripts/run_regression.ps1` 已新增语义校验：

```text
missing texture:
  missingTextures > 0
  warnings > 0
  fallbackPixels > 0

no UV:
  facesWithUv = 0
  facesWithoutUv > 0
  fallbackPixels > 0
```

---

## 6. 当前未实现范围

04A 按阶段约束未实现：

```text
材料策略
白墨 underbase
光油 top layer
texture-driven varnish
texture-driven white
color_shell_volume
闭合模型完整外壳纹理投影
支撑形态修复
支撑小岛合并 / 剔除
OpenVDB
Qt UI
RIP 半色调
ICC / CMYK
```

---

## 7. 是否建议进入 05

建议可以进入 05 材料策略阶段。

理由：

```text
04 RGB texture 主链路已通过真实模型验证；
04A fallback fixture 已恢复，missing texture / no-UV fallback 可快速回归；
support connectivity diagnostics 已输出，支撑割裂不再只依赖人工观察；
RGBWSV 协议未改变；
完整回归通过。
```

但进入 05 前应保留一个明确边界：

```text
05 只处理材料策略和 RGB/W/V 组合规则；
第 68 层这类支撑割裂如果要做形态修复，应另开 04B 或 08 支撑形态优化阶段。
```
