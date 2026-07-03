# TIFF RGB 与 UI 预览白色/黑色差异分析

> 生成日期：2026-07-03  
> 关联输出包：`output/ui_sessions/MF_nai_you_20260703_170638/package`  
> 关联配置：`output/ui_sessions/MF_nai_you_20260703_170638/slice_config.generated.json`  
> 重点层：`layer_000230.tiff` / `layerIndex=230` / `zMm=2.305`

## 1. 当前结论

UI 叠加预览中红框处的白色不是白墨材料。

该输出包的第 230 层统计显示：

```text
W whitePrintPixels = 0
V varnishPrintPixels = 0
S supportPrintPixels = 68470
RGB rgbPrintPixels = 15609
```

因此：

```text
UI 中白色区域：通常表示 preview 背景/未显示的 RGB 区域，不代表 W 白墨。
UI 中绿色区域：表示 S 支撑通道打印。
TIFF RGB 图层中的黑色：表示 RGB 通道生产值为 0，即 RGB 打印，不是空白。
```

这两个截图表现不一致的根因是：UI 叠加预览读取的是 `preview/*.png` 的显示图；Photoshop 打开的 TIFF RGB 图层读取的是生产 TIFF 的前三个通道 R/G/B。两者不是同一个语义层。

## 2. 证据

### 2.1 配置证据

当前一键生成配置关键字段：

```json
{
  "texture": {
    "enabled": true,
    "applyMode": "top_surface_band",
    "topSurfaceLayers": 50
  },
  "modelMaterial": {
    "materialChannel": "RGB",
    "applyMode": "solid_volume",
    "rgb": [0, 0, 0],
    "whiteValue": 255,
    "varnishValue": 255
  },
  "support": {
    "enabled": true,
    "mode": "full_vertical_projection",
    "value": 0
  },
  "preview": {
    "channels": ["texture_rgb", "support"]
  }
}
```

含义：

```text
texture_rgb preview 只用于显示表面纹理预览。
support preview 用绿色显示 S 通道。
modelMaterial.rgb = [0,0,0] 表示非纹理表面带的模型 RGB 默认写黑色打印值。
W/V 均为 255，即不打印。
```

### 2.2 第 230 层报告证据

`reports/slice_report.json` 中 layer 230：

```text
modelPixels   = 15609
supportPixels = 68470

R printPixels = 15609
G printPixels = 15609
B printPixels = 15609
W printPixels = 0
S printPixels = 68470
V printPixels = 0
```

所以当前层只有 RGB 模型材料和 S 支撑材料，没有白墨和光油。

### 2.3 TIFF 六通道抽样证据

`layer_000230.tiff` 是 6 通道 contiguous TIFF：

```text
width=229
height=455
samplesPerPixel=6
channelOrder=R G B W S V
bitDepth=8
polarity=black_is_print
printValue=0
emptyValue=255
```

典型像素：

```text
(164,138) = (255,255,255,255,0,255)
(170,160) = (255,255,255,255,0,255)
(190,230) = (255,255,255,255,0,255)
(200,230) = (0,0,0,255,255,255)
```

解释：

```text
(255,255,255,255,0,255)
  R/G/B/W/V 不打印，S 打印。
  这是支撑材料，不是白墨，也不是空白。

(0,0,0,255,255,255)
  R/G/B 打印，W/S/V 不打印。
  这是黑色 RGB 模型材料，不是支撑，也不是光油/白墨。
```

### 2.4 红框近似区域统计

右侧红框近似区域 `x=160..204, y=80..269`：

```text
S 支撑像素        6376
RGB 打印像素      2138
RGB 纯黑像素      1862
完全空白像素      36
W 白墨像素        0
V 光油像素        0
```

左侧红框近似区域 `x=25..59, y=80..249`：

```text
S 支撑像素        4508
RGB 打印像素      1423
RGB 纯黑像素      806
完全空白像素      19
W 白墨像素        0
V 光油像素        0
```

中心支撑区域近似 `x=90..144, y=120..329`：

```text
全部像素 = (255,255,255,255,0,255)
即全部为 S 支撑通道打印。
```

## 3. 为什么 UI 预览白色，而 TIFF RGB 是黑色

### 3.1 UI 叠加预览显示的是 preview PNG

当前 UI 的 `RGB + S 支撑` 叠加使用：

```text
preview/texture_rgb_000230.png
preview/support_s_000230.png
```

而不是直接读取：

```text
layers/layer_000230.tiff
```

`texture_rgb_000230.png` 是“表面纹理预览图”。它会把非表面纹理带的模型默认 RGB 隐藏为白色背景，避免把内部/非表面默认黑色误认为真实贴图颜色。

因此，UI 里看到的白色更接近“当前 preview 模式不显示此处 RGB 生产数据”，不是“此处有白墨”。

### 3.2 Photoshop 打开 RGB 图层看到的是生产 RGB

Photoshop 打开多通道 TIFF 的 RGB 图层时，通常只显示前三个样本：

```text
R G B
```

它不会自动告诉用户：

```text
第 4 通道 W
第 5 通道 S
第 6 通道 V
```

因此：

```text
RGB=(255,255,255) 可能是空白，也可能是 S 支撑像素，因为支撑写在第 5 通道。
RGB=(0,0,0) 表示 RGB 黑色打印，但不代表 S/W/V。
```

这就是 UI 与 Photoshop 视觉差异的主要原因。

## 4. 是否是配置原因

部分是配置原因，部分是显示语义原因。

### 4.1 配置原因

当前配置：

```json
"texture.applyMode": "top_surface_band",
"modelMaterial.rgb": [0, 0, 0]
```

组合效果是：

```text
顶部表面带：使用贴图颜色。
非顶部表面带/模型实体区域：回退到 modelMaterial.rgb，即 RGB 黑色。
```

所以 TIFF RGB 图层中的黑色不是随机错误，而是当前配置和生产写入策略共同导致的结果。

### 4.2 显示语义原因

UI 为了避免误导，把 `texture_rgb preview` 做成“所见即贴图表面颜色”，不会把所有生产 RGB 都显示出来。

这会带来一个副作用：

```text
UI 叠加预览看起来像白色/空白；
生产 TIFF RGB 图层实际存在黑色 RGB 打印。
```

这不是 TIFF 协议错误，而是“生产数据”和“预览显示数据”的语义没有在 UI 上明确区分。

## 5. 是否可以解决

可以解决，但要区分两类目标。

### 5.1 只解决显示误解

建议增加 UI 功能：

```text
1. 增加“生产 RGB 预览”模式，直接从 TIFF R/G/B 读取显示；
2. 保留“纹理 RGB 预览”模式，只显示真实贴图表面；
3. 在叠加预览角标显示当前图层来源：
   - texture_rgb preview
   - production RGB from TIFF
   - support pseudo color
4. 增加像素探针，点击像素显示 R/G/B/W/S/V 六通道值；
5. 图例明确：
   - 绿色 = S 支撑
   - 白色 = preview 背景或生产空值，需看通道值区分
   - 黑色 = RGB 生产打印值 0
```

这类修复不改变切片结果，只解决 UI 和 Photoshop 解释不一致的问题。

### 5.2 解决 TIFF RGB 黑色生产数据

如果产品期望“非纹理表面带不打印 RGB 黑色”，则需要改配置或增加策略。

短期配置方案：

```json
"modelMaterial": {
  "rgb": [255, 255, 255]
}
```

这会让非纹理表面带的 RGB 不打印。但要注意：如果某些模型需要实体 RGB 底色，这会改变材料输出。

更稳妥的工程方案：

```text
新增 texture.nonSurfaceRgbPolicy：
  - model_material: 当前行为，非纹理表面带写 modelMaterial.rgb；
  - empty: 非纹理表面带 RGB 写 255，不打印；
  - fallback_rgb: 使用 texture.fallbackRgb；
  - material_policy: 交给 MaterialPolicy/Profile 决定。
```

对当前甲片彩色纹理模型，建议默认：

```text
texture.nonSurfaceRgbPolicy = empty
```

这样：

```text
表面贴图区域有 RGB；
支撑区域只写 S；
非表面实体内部不再默认写 RGB 黑色。
```

### 5.3 白墨不是当前问题

本层 W 通道全为 255，白墨没有打印。若后续需要白墨底层，应启用 `materialProcessProfile` 或 `materialPolicy.white`，而不是通过 Photoshop RGB 白色判断。

## 6. 配置文件数量分析

当前 `samples/configs` 下约有 70 个 JSON 配置，分布在：

```text
3mf                 13
material_mapping     3
material_policy      6
material_process     7
obj_standard         1
openvdb             16
openvdb_candidate    1
relief               4
storage_mode         4
support              6
textured             3
ui_smoke             2
root                 3
```

这些配置不能简单合并成一个文件，原因是：

```text
1. 很多配置是回归 fixture，用来固定某个模块行为；
2. 3MF / OBJ / OpenVDB / support / storage mode 的验收目标不同；
3. 自动化脚本需要稳定、可复现的输入文件；
4. 历史阶段配置承担了文档化验收证据。
```

但它们不应该全部暴露给普通 UI 用户。

## 7. 建议的配置收敛方案

### 7.1 保留少量生产/调试 Profile

UI 默认只展示少量长期 Profile：

```text
1. OBJ 彩色纹理 Legacy
2. OBJ 彩色纹理 + 白墨 + 光油
3. 3MF 彩色纹理
4. 单材料浮雕
5. OpenVDB 诊断
6. OpenVDB 非生产候选
```

其余配置移动到“测试夹具/高级调试”分类。

### 7.2 引入 UI 设置界面

建议新增“切片设置”界面，替代用户手工选择大量 JSON：

```text
基础：
  模型路径、输出目录、DPI、层厚、缩放、自动旋转

材料：
  RGB 来源、非表面 RGB 策略、白墨策略、光油策略

支撑：
  支撑开关、支撑模式、膨胀、桥接、孤岛过滤

纹理：
  贴图开关、采样器、flipV、topSurfaceLayers、缺失贴图策略

预览：
  preview 间隔、显示模式、伪彩颜色、生产 RGB / 纹理 RGB 切换

实验：
  OpenVDB 诊断、OpenVDB 候选、非生产输出开关
```

UI 保存时生成：

```text
output/ui_sessions/<session>/slice_config.generated.json
```

长期 Profile 只作为模板，不要求用户直接维护全部 JSON。

### 7.3 配置文件分层

建议目录调整为：

```text
samples/profiles/
  production/
  debug/
  experimental/

samples/configs/fixtures/
  support/
  storage_mode/
  openvdb/
  bad_packages/
  ui_smoke/
```

并让 `samples/scenarios/slicer_scenarios.json` 只索引常用 Profile；测试脚本仍可读取 fixtures。

## 8. 推荐后续任务

建议拆成三个原子任务：

```text
Task A：UI 增加像素探针和生产 TIFF RGB 预览模式。
Task B：新增 texture.nonSurfaceRgbPolicy，并给 OBJ 彩色纹理一键配置默认 empty。
Task C：配置/Profile 收敛，将 UI 默认场景减少到长期可用 Profile，测试 fixture 移入高级分类。
```

优先级：

```text
P0：Task A，先避免误读生产数据。
P1：Task B，解决非表面模型 RGB 黑色是否应打印的问题。
P2：Task C，降低 UI 配置复杂度。
```
