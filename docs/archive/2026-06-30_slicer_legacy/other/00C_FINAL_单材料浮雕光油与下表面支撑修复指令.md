# 00C_FINAL_单材料浮雕光油与下表面支撑修复指令

> 文档版本：v1.0 / 定稿  
> 文档状态：Final / Codex 执行用  
> 建议提交目录：`docs/slicer/`  
> 关联阶段：00C 单材料浮雕模型切片修正  
> 适用项目：`polarbao/slice_soft_demo`  
> 核心修复：浮雕模型默认必须同时输出 **V 光油模型材料** 与 **S 下表面支撑材料**

---

## 0. 本文件的权威性

本文件作为 00C 阶段的当前定稿修复指令。

它覆盖此前 00C / 00C-R1 文档中的以下旧结论：

```text
旧结论：浮雕模型默认 support.enabled=false
旧结论：新增 slice_config_relief_varnish_support.json
旧结论：保留 slice_config_relief_varnish.json 作为无支撑主线样例
```

从本文件开始，项目主线采用：

```text
浮雕模型默认 = 单材料光油模型 + 下表面支撑
```

也就是：

```text
模型材料通道：V
支撑材料通道：S
```

---

## 1. 业务背景修正

当前浮雕模型不是普通贴底浮雕，而是美甲甲片类拱形 3D 模型。

该类模型具有以下特征：

```text
1. 模型本体是拱形结构；
2. 模型下表面与构建平台之间存在空间；
3. 打印时需要模型材料与支撑材料共同参与；
4. 当前单材料模型材料使用光油 V 通道输出；
5. 下表面支撑必须使用 S 通道输出。
```

因此，00C 阶段不能只输出光油模型材料，也不能默认关闭支撑。

---

## 2. 最终语义定义

### 2.1 输出协议

继续遵守 00B 协议：

```text
TIFF 位深：uint8
数值范围：0 - 255
打印极性：0 = 打印，255 = 不打印
通道顺序：R G B W S V
```

### 2.2 模型材料区域

当前单材料浮雕模型使用光油材料，模型区域写入：

```text
R = 255
G = 255
B = 255
W = 255
S = 255
V = 0
```

含义：

```text
RGB 不打印
W 不打印
S 不打印
V 光油打印
```

### 2.3 支撑材料区域

下表面支撑区域写入：

```text
R = 255
G = 255
B = 255
W = 255
S = 0
V = 255
```

含义：

```text
只有 S 支撑通道打印
```

### 2.4 空白区域

空白区域写入：

```text
R = 255
G = 255
B = 255
W = 255
S = 255
V = 255
```

### 2.5 像素优先级

保持：

```text
Model > Support > Empty
```

如果某个位置同时被判断为模型和支撑，必须优先写模型材料，不允许 S 通道覆盖模型区域。

---

## 3. 最终配置策略

### 3.1 不新增 support 专用浮雕配置文件

不要新增：

```text
samples/configs/slice_config_relief_varnish_support.json
```

原因：

```text
当前业务主线基本不存在无支撑浮雕场景。
新增 support 配置会误导后续开发者认为“无支撑浮雕”是默认主线。
```

### 3.2 直接修改原有浮雕配置

必须直接修改原有文件：

```text
samples/configs/slice_config_relief_varnish.json
```

使其成为业务默认配置：

```text
单材料浮雕光油 + 下表面支撑
```

---

## 4. `slice_config_relief_varnish.json` 目标配置

请将 `samples/configs/slice_config_relief_varnish.json` 调整为以下语义。

如果已有字段路径不同，可以保持现有字段顺序，但必须保证语义一致。

```json
{
  "slicingMode": "relief_heightfield",
  "input": {
    "modelPath": "samples/models/0.3.obj",
    "format": "auto"
  },
  "output": {
    "packageDir": "output/SlicePackage_relief_varnish",
    "dpiX": 600,
    "dpiY": 600,
    "layerThicknessMm": 0.01,
    "channelOrder": ["R", "G", "B", "W", "S", "V"],
    "bitDepth": 8,
    "planarConfig": "contiguous",
    "tiled": true,
    "tileSize": [256, 256]
  },
  "autoOrient": {
    "enabled": true,
    "maxHeightMm": 6.0,
    "strategy": "minimize_height_by_right_angle_rotation"
  },
  "background": {
    "value": 255
  },
  "modelMaterial": {
    "materialChannel": "V",
    "applyMode": "solid_volume",
    "rgb": [255, 255, 255],
    "whiteValue": 255,
    "varnishValue": 0
  },
  "support": {
    "enabled": true,
    "mode": "bottom_projection",
    "value": 0,
    "offsetMm": 0.0,
    "minAreaPx": 0
  },
  "relief": {
    "fillMode": "intersection_range",
    "baseZMm": 0.0
  },
  "preview": {
    "enabled": true,
    "format": "png",
    "interval": 10,
    "channels": ["varnish", "support"],
    "onlyNonEmptyLayers": true
  }
}
```

### 4.1 关键字段说明

#### `support.enabled = true`

浮雕美甲模型默认需要下表面支撑。

#### `relief.fillMode = intersection_range`

不能继续使用：

```text
surface_to_base
```

原因：

```text
surface_to_base 会将 baseZ 到 zMax 全部作为模型材料填充。
这样模型从平台开始就已经被 V 通道占据，S 支撑没有空间生成。
```

必须使用：

```text
intersection_range
```

含义：

```text
模型材料只占据真实模型厚度区间 zMin..zMax。
z=0 到 zMin 之间的空间可用于生成下表面支撑。
```

#### `preview.channels = ["varnish", "support"]`

调试时必须同时查看：

```text
V 光油模型区域
S 支撑材料区域
```

---

## 5. Codex 执行任务

Codex 应按以下步骤执行。

### Step 1：修改原有配置文件

修改：

```text
samples/configs/slice_config_relief_varnish.json
```

要求：

```text
support.enabled = true
support.mode = bottom_projection
support.value = 0
relief.fillMode = intersection_range
preview.channels 包含 varnish 和 support
```

不要新增：

```text
slice_config_relief_varnish_support.json
```

---

### Step 2：先运行验证，不要立即大改算法

执行：

```powershell
build\Debug\slicer_cli.exe --config samples\configs\slice_config_relief_varnish.json
build\Debug\rip_reader_test.exe --package output\SlicePackage_relief_varnish
```

然后检查：

```text
reports/support_report.json
reports/slice_report.json
preview/*support*.png
preview/*varnish*.png
```

---

### Step 3：判断现有代码是否已支持

如果以下条件成立：

```text
support_report.supportPixels > 0
slice_report 中部分 layer supportPixels > 0
preview 中存在 support_s 图像
V 通道打印像素 > 0
S 通道打印像素 > 0
rip_reader_test 通过
```

则不需要修改核心算法，只需要更新报告和文档即可。

---

### Step 4：如果仍没有 S 支撑，则修正算法

如果 `supportPixels = 0`，Codex 需要检查并修正：

```text
1. relief.fillMode=intersection_range 是否真正只填充 zMin..zMax；
2. compute_first_model_layers 是否基于 relief 模型 mask 得到 zMin 层；
3. compose_layer 是否在 relief_heightfield 模式下仍然执行支撑逻辑；
4. support.enabled 是否被 relief 模式或配置读取逻辑强制覆盖为 false；
5. support.mode=bottom_projection 是否在 relief 模式下被支持。
```

必要时新增显式 relief column lower layer 数据：

```cpp
struct ReliefColumnInfo {
    bool has_model;
    int lower_layer;
    int upper_layer;
};
```

支撑判定逻辑应为：

```cpp
if (!is_model && support.enabled && column.has_model && layer_index < column.lower_layer) {
    write_support_pixel();
}
```

但必须保持：

```text
Model > Support
```

---

## 6. 报告输出要求

### 6.1 `support_report.json`

应明确记录：

```json
{
  "enabled": true,
  "mode": "bottom_projection",
  "value": 0,
  "slicingMode": "relief_heightfield",
  "supportSource": "relief_lower_surface",
  "supportPixels": 0
}
```

其中 `supportPixels` 在有效测试模型中应大于 0。

### 6.2 `relief_report.json`

建议记录：

```json
{
  "slicingMode": "relief_heightfield",
  "fillMode": "intersection_range",
  "baseZMm": 0.0,
  "support": {
    "enabled": true,
    "source": "lower_surface",
    "expectedSupport": true
  }
}
```

### 6.3 `slice_report.json`

每层应继续记录：

```text
modelPixels
supportPixels
varnishNonZeroPixels
supportNonZeroPixels
```

注意：在 00B 极性下，`nonZeroPixels` 名称容易误导，因为打印像素是 0。后续可改名为：

```text
printPixels
```

但本阶段不强制重命名。

---

## 7. 验收标准

00C 最终修复验收标准：

1. `samples/configs/slice_config_relief_varnish.json` 是默认浮雕光油 + 支撑配置。
2. 不新增 `slice_config_relief_varnish_support.json`。
3. `support.enabled = true`。
4. `relief.fillMode = intersection_range`。
5. `modelMaterial.materialChannel = V`。
6. 输出 TIFF 为 uint8。
7. Manifest 中 `polarity = black_is_print`。
8. V 通道存在打印像素，即模型区域 `V = 0`。
9. S 通道存在打印像素，即支撑区域 `S = 0`。
10. `support_report.supportPixels > 0`。
11. Preview 同时输出 `varnish` 与 `support` 通道图。
12. `rip_reader_test` 通过。
13. 原普通模型 `closed_mesh_scanline` 配置不被破坏。
14. 不实现彩色纹理。
15. 不实现完整光油覆盖策略。

---

## 8. 非目标

本阶段不要处理：

```text
彩色纹理
UV / MTL / Texture
完整光油覆盖策略
top_surface_only
top_n_layers
局部光油
支撑树
支撑密度
支撑可拆除结构
OpenVDB 正式内核
```

---

## 9. 需要更新的文件

Codex 至少应检查或修改：

```text
samples/configs/slice_config_relief_varnish.json

src/slicer_core/config.*
src/slicer_core/slicer.*
src/slicer_core/report.*
src/slicer_core/manifest.*
docs/slicer/REPORT_00_P0_Demo当前实现状态.md
```

如果现有代码已能生成 S 支撑，则核心代码可不改，只更新配置和报告。

---

## 10. 推荐提交说明

推荐 commit message：

```text
修正 00C 浮雕默认配置为光油模型与下表面支撑联合输出
```

---

## 11. 最终结论

00C 最终定稿口径：

```text
浮雕美甲模型默认需要下表面支撑。
原有 slice_config_relief_varnish.json 应直接改为 V 光油 + S 支撑配置。
无支撑浮雕不是当前业务默认场景，不作为主线样例维护。
```
