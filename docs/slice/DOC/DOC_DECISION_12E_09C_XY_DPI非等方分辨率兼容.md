# DOC_DECISION 12E-09C X/Y DPI 非等方分辨率兼容

> 状态：APPROVED FOR PREPARATION
> 日期：2026-07-23
> 决策：作为 12E-09B 后、12E-10 前的独立专项实施

## 1. 背景

当前配置已存在 `output.dpiX` 和 `output.dpiY`，几何与 raster 主链也分别使用 X/Y 像素物理尺寸；
但产品默认、UI 一键配置、核心校验和 RIP Reader 仍把两轴固定为 `600/600`。

目标默认值调整为：

```text
dpiX = 635；
dpiY = 600；
pixelSizeXmm = 25.4 / 635 = 0.040000 mm；
pixelSizeYmm = 25.4 / 600 = 0.042333... mm。
```

同一物理宽度下，635 dpi 相比 600 dpi 的 X 像素数、单层数据量和相关计算量理论上约增加
`5.83%`；最终耗时必须按同模型、同模式、同 DPI 实测。

## 2. 决策

不把 DPI 修改塞入 09B-01..06。09B 只负责双模式产品入口、Profile 能力和生产 session；DPI 会影响：

```text
配置默认值和校验；
一键切片生成配置；
Legacy/Global raster 尺寸；
manifest grid 和物理像素；
RIP Reader 严格校验；
TIFF 尺寸、hash、golden 和性能基线；
Qt 配置控件、帮助和 Effective Config。
```

因此新增 `12E-09C X/Y DPI 非等方分辨率与协议兼容`，在 09B 完成后执行。

## 3. 兼容规则

```text
显式写为 600/600 的旧配置继续有效；
旧 golden/benchmark fixture 保持显式 600/600，不因产品默认变化重写历史 hash；
仅在配置省略 DPI、UI 新建配置或一键切片时采用 635/600；
Legacy 与 Global 必须读取同一 Effective Config DPI；
RIP Reader 不再要求固定 600/600，而是校验合法范围和 manifest 内部一致性；
p0.rgbwsv.2、RGBWSV、uint8、black_is_print 均不改变。
```

## 4. 范围

产品 UI 建议范围：

```text
72..2400 dpi，整数步长 1；
默认 X=635、Y=600；
显示对应 X/Y 像素物理尺寸；
超出范围禁止保存和运行。
```

核心和 Reader 的防御性上限由 09C-01 单测冻结，必须覆盖零、负数、过大值、X/Y 不同和缺失字段。
“配置合法”不等于“已在具体打印设备上认证”；首批生产验收组合固定为 `600/600` 和 `635/600`，
其他组合必须在 UI 标记为未做设备认证。

## 5. 路线

```text
12E-09B-01..06
  -> 12E-09C-01..06
  -> 12E-09A-02..06
  -> 12E-10A..D
```

09A 同层 preview 在最终 DPI 合同上验证，避免后续因 raster 尺寸变化重复收口。

## 6. 不做内容

```text
不修改 TIFF 通道和极性；
不引入重采样或 RIP 半色调；
不把 DPI 与模型 scale 混成同一参数；
不批量把所有历史 fixture 改为 635/600；
不因 X/Y 不同而自动旋转模型；
不在 09B-01 中提前修改生产默认。
```
