# DOC_AUDIT_13G Reality 模型朝向与内部支撑连续性

> 文档状态：CURRENT EVIDENCE / Stage 13G
> 版本：v1.0
> 日期：2026-07-30
> 证据等级：A（当前代码、真实 Package、真实 OBJ 只读统计）

## 1. 审计问题

本文件回答以下三个问题：

1. `model/obj/reality` 模型为什么在低层先出现中间区域、随后向外扩展；
2. 第 20 层存在中心支撑、第 21 层后中心支撑消失，是否由摆放造成；
3. 当前 `internalVoid` 是否等价于产品所需的“甲片内侧连续承托”。

## 2. 当前代码事实

### 2.1 自动定向没有完成正反面判断

`src/slicer_core/model.cpp` 当前自动定向只处理：

```text
高度限制；
候选姿态占地和高度排序；
minZ=0 落台；
平放甲片长轴转向场景 +Y。
```

当模型原始 Z 高度已经小于 `maxHeightMm=9` 时，现有路径保留 Z 正反面，只执行落台和
平面内 Z 轴转向。它不会在“正面朝 +Z”和“正面朝 -Z”之间选择。

### 2.2 internalVoid 是逐层二维闭合检测

`AddInternalVoidSupportForLayer` 对每一层独立执行：

```text
从画布边界 flood-fill 外部空白；
剩余未与画布边界连通的空白组件视为 internal_void；
面积达到 minAreaPx 后写入 S。
```

因此当前实现中的 internal void 精确定义是：

> 当前二维切片中完全封闭、且不与画布外部连通的空洞。

它不是跨层空腔追踪、三维内腔识别，也不是甲片内侧支撑体积。

## 3. Reality 五模型的 Z 朝向证据

五个 OBJ 的原始 Z 高度均小于 9 mm，因此均进入“保留 Z 正反面”的路径。对短轴两侧
12.5% 带区和中心带区的下包络进行只读统计，结果如下：

| 模型 | 原始 Z 高度 | 中心最低点 | 两侧较低端最低点 | 当前低层趋势 |
|---|---:|---:|---:|---|
| segment_101 | 6.9752 mm | 0.000 mm | 1.788 mm | 中心先出现 |
| segment_102 | 6.6433 mm | 0.000 mm | 0.954 mm | 中心先出现 |
| segment_103 | 5.5598 mm | 0.000 mm | 1.362 mm | 中心先出现 |
| segment_104 | 5.0418 mm | 0.000 mm | 1.359 mm | 中心先出现 |
| segment_105 | 4.0404 mm | 0.000 mm | 1.078 mm | 中心先出现 |

甲片按“内侧开口朝下、外表面朝 +Z”放置时，预期是两侧边缘先接近打印平台，中心拱顶
后出现。当前五个模型恰好相反，说明它们在进入切片时保留了源文件的反向 Z 姿态。

结论：

```text
低层先中间、后向外扩展的主因是 Z 正反面摆放错误；
不是 layerIndex 从高到低；
不是 UI 把 TIFF 层序倒放；
不是 XY 排版位置；
也不是 internalVoid 自身先生成了模型数据。
```

## 4. segment_105 第 20/21 层证据

证据 Package：

```text
runtime/slicesoft/Release/output/ui_sessions/
260729-16-40-21-739-segment_105.txt_single_material_relief_scene_legacy_20260730_134628_083/package
```

有效配置：

```text
slicingMode=relief_heightfield
layerThicknessMm=0.038
dpiX=635
dpiY=600
support.placement=lower
support.internalVoid.enabled=true
support.internalVoid.fillRule=all_internal_voids
```

生产 TIFF 六通道与模型 mask 的核对结果：

| 层 | z | W 模型像素 | S 支撑像素 | 被模型完全包围的空洞 |
|---:|---:|---:|---:|---:|
| 20 | 0.779 mm | 16098 | 39863 | 10304 px / 1 个 |
| 21 | 0.817 mm | 15747 | 28618 | 0 px |
| 30 | 1.159 mm | 14381 | 20799 | 0 px |

第 20 层的模型 mask 在局部形成闭环，中心 10304 px 被判定为 `internal_void`。第 21 层
该闭环在模型端部打开，中心空白与画布外部连通，逐层二维算法因此停止写入
`InternalVoid` 支撑。S 像素同步下降不是随机断层，而是现有算法按定义执行的结果。

## 5. 五模型是否都有相同现象

使用 0.10 mm 只读诊断栅格对五个模型进行趋势复核。该结果用于根因分类，不作为生产
像素验收：

| 模型 | 短暂二维闭合空洞层区间 | 下一阶段表现 |
|---|---|---|
| segment_101 | 11..17 | 空洞重新与外部连通 |
| segment_102 | 10..12 | 空洞重新与外部连通 |
| segment_103 | 10..14 | 空洞重新与外部连通 |
| segment_104 | 11..18 | 空洞重新与外部连通 |
| segment_105 | 12..20 | 第 21 层重新与外部连通 |

五个模型具有同类源姿态和同类支撑变化，因此不是 segment_105 单文件故障。

## 6. 根因分层

### 根因 A：模型摆放

当前 Reality 模型以中心最低、两侧较高的 Z 姿态落台，和产品要求的甲片正面朝 +Z
相反。这是必须先修复的根因。

### 根因 B：二维 internalVoid 定义过窄

当前逐层闭合规则适合解释真正封闭的二维孔洞，但不能表示：

```text
甲片内侧从平台向上连续生长的承托体；
跨层曾经闭合、随后局部开口的空腔；
由模型下表面投影确定的连续支撑柱；
支撑底部最大投影铺底。
```

### 非根因：切片层序

生产层仍按：

```text
layerIndex=0 -> z=(0+0.5)*layerThicknessMm
layerIndex=n -> z=(n+0.5)*layerThicknessMm
```

由低 Z 向高 Z 输出。视觉异常来自输入姿态和支撑语义，不是层序反转。

## 7. 修正优先级

```text
P0：完成甲片正反面判定，使 Reality 五模型外表面朝 +Z；
P1：在正确姿态下重新生成五模型支撑连续性证据；
P2：实现 support.baseProjection 最大支撑投影铺底，默认 30 层；
P3：只有 P1 仍证明内部承托中断时，才引入跨层 cavity/support continuity 策略。
```

禁止先通过扩大 internalVoid 或全画布填 S 掩盖反向摆放问题。

## 8. 2026-07-30 修正后复测

`13G-00B` 已在自动定向中加入薄壳甲片正反面判断。Reality 五模型的只读
`--inspect-model` 结果均为：

```text
selectedOrientation=rotate_x_180_rotate_z_minus_90
rotationDeg=[180, 0, -90]
```

这表示先绕模型原始长轴翻转 180 度，使两侧边缘先接近平台，再把平面内长轴和尖端统一到
场景 `+Y`。显式 `autoOrient.enabled=false` 仍保持源姿态。

随后用 segment_105、635/600 DPI、0.038 mm、`placement=lower` 和
`internalVoid=all_internal_voids` 生成 Release Package：

```text
output/13g_segment105_corrected_20260730_150423/package
```

关键层统计如下：

| layerIndex | z | 模型像素 | S 支撑像素 | bottom_projection | internal_void |
|---:|---:|---:|---:|---:|---:|
| 20 | 0.779 mm | 3695 | 52242 | 52242 | 0 |
| 21 | 0.817 mm | 3748 | 52138 | 52138 | 0 |
| 30 | 1.159 mm | 4429 | 50950 | 50950 | 0 |
| 90 | 3.439 mm | 17485 | 4416 | 45 | 4371 |

S 支撑从 `layerIndex=0` 连续存在到 `layerIndex=93`，在 `layerIndex=94` 才归零。由此确认：

```text
第 20/21 层中心承托中断首先是反向摆放造成；
正确姿态下 lower support 已跨过原故障层连续生长；
无需为了该现象立即扩大 internalVoid 定义；
support.baseProjection 仍是独立的“前 N 层最大支撑投影铺底”工艺能力。
```

## 9. 13G 铺底启用后的 Release 复测

生产协议复测 Package：

```text
output/13g_segment105_base_projection_release_20260730/package
```

配置保持 `635/600 DPI`、`0.038 mm`、`placement=lower`，新增：

```json
{
  "support": {
    "baseProjection": {
      "enabled": true,
      "layerCount": 30,
      "source": "max_support_footprint"
    }
  }
}
```

结果：

| 指标 | 结果 |
|---|---:|
| 自动定向 | `rotate_x_180_rotate_z_minus_90` |
| Z 范围 | `0..4.0404 mm` |
| 有效铺底层 | `layerIndex 0..29` |
| 最大支撑 footprint | 55791 px |
| 最终 `projection_base` | 5533 px |
| 总 S | 3529089 px |
| S 连续范围 | `layerIndex 0..93` |
| Release 总耗时 | 986.358 ms |
| RIP strict | PASS |

关键层：

| layerIndex | model | S | bottom_projection | projection_base | internal_void |
|---:|---:|---:|---:|---:|---:|
| 20 | 3695 | 52485 | 52242 | 243 | 0 |
| 21 | 3748 | 52432 | 52138 | 294 | 0 |
| 29 | 4341 | 51826 | 51103 | 723 | 0 |
| 30 | 4429 | 50950 | 50950 | 0 | 0 |
| 90 | 17485 | 4416 | 45 | 0 | 4371 |

这组数据同时证明：

```text
layerCount=30 精确对应 layerIndex 0..29；
第 30 索引层不再由铺底策略新增 S；
原 20/21 层问题的首要修复仍是正确摆放；
internalVoid 继续只处理单层二维闭合空洞；
baseProjection 负责低层工艺铺底，二者不应合并为一个含糊策略。
```
