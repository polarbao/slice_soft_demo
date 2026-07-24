# PRD 12E-09C X/Y DPI 配置与生产协议兼容

> 状态：READY FOR IMPLEMENTATION AFTER 09B
> 日期：2026-07-23

## 1. 目标

允许用户在 Qt 配置界面分别设置 X/Y DPI；新建或一键切片默认 `635/600`，并保证 Legacy、Global、
TIFF package、manifest、preview/report 和 RIP Reader 对非等方像素保持一致。

## 2. 用户故事

### US-09C-01 设置 X/Y DPI

用户可在“配置 -> 常用 -> 输出分辨率”中设置：

```text
X 方向 DPI；
Y 方向 DPI；
只读 X/Y 像素尺寸（mm/px）。
```

### US-09C-02 一键切片采用当前值

“一键切片”和 Global 一键入口必须使用当前 session Effective Config 的 DPI，不再硬编码 600。

### US-09C-03 旧配置兼容

旧配置显式 `600/600` 时输出行为不变；不得因新默认批量改变历史 fixture。

### US-09C-04 可审计输出

manifest/report 至少能还原：

```text
dpiX/dpiY；
pixelSizeXmm/pixelSizeYmm；
widthPx/heightPx；
requested/effective pipeline mode；
当前 session/config/package identity。
```

## 3. 功能需求

| ID | 要求 |
|---|---|
| FR-01 | 产品默认 `dpiX=635`、`dpiY=600` |
| FR-02 | UI 使用两个整数控件，范围 72..2400，步长 1 |
| FR-03 | UI 显示 `25.4/dpi`，至少 6 位小数 |
| FR-04 | Config Editor 保存/回读/回退时两轴独立 |
| FR-05 | 一键切片不再硬编码 600/600 |
| FR-06 | Legacy 和 Global 使用相同 DPI 配置与 raster 物理定义 |
| FR-07 | Reader 接受合法的非等方 DPI，并拒绝缺失/非法/内部不一致 |
| FR-08 | 显式 600/600 的旧 fixture 与 golden 保持不变 |
| FR-09 | DPI 变化使既有 admission/package 状态 stale |
| FR-10 | 帮助文档明确 DPI 与模型缩放不是同一概念 |
| FR-11 | 外侧光油厚度按 X/Y 物理像素分别换算，不继续依赖单一 42.3um |
| FR-12 | 预览按物理像素比例显示，635/600 不产生约 5.83% 的视觉横向拉伸 |
| FR-13 | UI 区分“配置合法”和“设备已认证”；首批认证组合为 600/600、635/600 |

## 4. 生产不变量

```text
schema = p0.rgbwsv.2；
channelOrder = R G B W S V；
bitDepth = 8；
polarity = black_is_print；
printValue = 0；
emptyValue = 255。
```

09C 只扩展 grid 分辨率取值，不改变以上协议。

## 5. 验收标准

```text
省略 DPI 的新配置解析为 635/600；
显式 600/600 保持 600/600；
635/600 在 Legacy 和 Global 都能生成一致物理范围的 TIFF package；
manifest 的 dpi、pixelSize 和 TIFF 宽高一致；
RIP strict 对 635/600 PASS，对 0、负数、过大值、字段矛盾 FAIL；
UI 修改、保存、回读、一键切片和重新加载均保持两轴值；
外侧光油的 X/Y 物理厚度在离散误差内满足配置值；
层预览和叠加预览按物理纵横比显示；
旧 600/600 golden/hash 不发生无关变化。
```

## 6. 非目标

RIP 半色调、喷头映射、打印机运动补偿、任意小数 DPI、图像后缩放和设备自动探测不属于本阶段。
