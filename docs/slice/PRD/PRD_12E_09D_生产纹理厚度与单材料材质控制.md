# PRD_12E-09D 生产纹理厚度与单材料材质控制

> 文档状态：READY FOR IMPLEMENTATION REVIEW
> 日期：2026-07-31

## 1. 目标

让用户在 UI 中修改的纹理厚度和单材料类型真正进入生产 Effective Config，并在切片输出中
可验证，而不是只影响诊断或只修改部分互相矛盾的字段。

## 2. 用户故事

### US-09D-01 Legacy 纹理层数

作为工艺用户，我可以设置 Legacy 顶面纹理层数，并看到等效 Z 厚度。重新切片后，
TextureSurface 层数和 RGB 统计应按设置变化。

### US-09D-02 Global 壳层宽度

作为已准入 Global Profile 用户，我可以按 mm 设置三维纹理壳层宽度或选择全纹理，
重新切片后 report 中 requested/effective/mode 与 TIFF 语义一致。

### US-09D-03 诊断与生产分离

作为用户，我能清楚区分“只做评估”的诊断宽度和“会改变输出”的生产参数，不会误以为
拖动诊断滑块已经修改生产包。

### US-09D-04 单材料浮雕材质

作为单材料浮雕用户，我可以选择白墨或光油。生产 TIFF 只在对应 W 或 V 通道写打印值，
支撑继续写 S。

## 3. 功能需求

### FR-09D-01 条件化控件

UI 根据 Effective Profile 显示：

```text
Legacy：顶面纹理层数；
Global：纹理壳层宽度 + 全纹理；
单材料浮雕：模型材料 W/V；
诊断区：独立诊断宽度，只读声明。
```

### FR-09D-02 请求值与有效值

每个生产控件显示：

```text
requested；
effective；
backend；
量化方式；
是否需要重新切片；
是否被 Profile 锁定。
```

### FR-09D-03 保存与一键切片

修改必须进入：

```text
当前 scene draft；
session effective config；
一键切片；
保存场景/配置回读；
报告和运行摘要。
```

### FR-09D-04 单材料原子切换

W/V 切换必须由 resolver 一次生成完整配置，任一字段冲突时保存/切片 fail closed。

### FR-09D-05 兼容

旧 Profile 未显式使用新 UI 状态时，输出字节和统计不得改变。

## 4. 默认值

```text
Legacy topSurfaceLayers：沿用 Profile 当前值，缺失时 1；
Global widthMm：沿用 Profile 当前值，不由诊断滑块覆盖；
Global mode：沿用 partial_shell/all_texture；
单材料浮雕：沿用所选 Profile 的 W 或 V；
诊断 widthMm：独立默认 0.10 mm。
```

## 5. 错误反馈

```text
当前 Profile 不支持该生产参数；
Global 模型未通过 admission；
requested 超出 Profile 范围；
W/V 配置字段冲突；
修改尚未保存或结果已 stale；
当前显示的是诊断值，不是生产值。
```

## 6. 验收标准

1. 修改 Legacy 层数后，session config 和生产结果同时变化。
2. 修改 Global width 后，Global report 和 Texture/Fill 分区变化。
3. 选择 allTexture 后 ModelFill coverage=0 且分区闭环。
4. 诊断滑块不改变生产配置，生产控件不伪装为诊断。
5. 单材料 W/V 各生成一个可被 RIP Reader 接受的 package。
6. W case `W>0/V=0`，V case `V>0/W=0`，S 按支撑策略存在。
7. 场景保存/回读和一键切片保持值一致。

## 7. 非目标

```text
纯白/透明 RIP 分色；
纹理铺底层；
TIFF Writer 后端替换；
OpenVDB 新准入；
自动材料标定。
```
