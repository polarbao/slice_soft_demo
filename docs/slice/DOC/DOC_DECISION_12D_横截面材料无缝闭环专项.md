# DOC_DECISION_12D 横截面材料无缝闭环专项

> 文档状态：Decision
> 日期：2026-07-13
> 前置阶段：12A 彩色纹理材料填充支撑光油策略、12C Qt UI 配置预览工作台

## 1. 决策结论

12D 新增为正式专项：横截面材料无缝闭环。

本专项不改变 RGBWSV 协议、不改变 `black_is_print` 极性、不引入 RIP 半色调。它只解决一个生产语义问题：每个切片层中，颜色层、模型内部填充层、支撑填充层、可选光油层之间不应存在会导致打印塌陷的一像素或多像素空白缝隙。

## 2. 背景

用户确认后的“无缝闭环”含义如下：

```text
每个切片数据中，颜色层数据（内、外层）、模型填充层数据、支撑填充层数据之间不应该有像素差。
如果这些材料之间存在缝隙，模型可能在打印过程中出现塌陷。
```

当前 UI 叠加预览中，从视觉上某些层看起来已经闭合；但视觉闭合不等于生产语义闭合。生产判断必须基于 TIFF 六通道和核心语义 mask：

```text
RGB = 颜色层
W/V/RGB = 模型内部填充层，取决于 modelFill.material
S = 支撑填充层
V = 表面光油或外侧光油壳层，必须区分来源
255 = 不打印
```

## 3. 当前状态判断

当前阶段不能判定为严格无缝闭环，原因如下：

1. 当前 report 已能统计 `textureSurfacePixels`、`modelFillPixels`、`supportPixels`、`internalVoidSupportPixels`、`outerVarnishPixels`、`outerSurfaceVarnishPixels`、`innerSurfaceVarnishPixels`。
2. 当前 report 明确说明 `outerInnerColorSplit.available=false`，即 legacy pipeline 尚未拆分外表面色彩层和内表面色彩层。
3. 当前没有正式的 `material_closure_report.json`，无法逐层输出闭环 PASS/FAIL。
4. 当前没有闭环修复策略，无法把一像素空白缝隙自动归入模型填充或支撑填充。
5. 之前针对真实 aishen 输出包的六通道扫描发现仍存在候选缝隙，说明必须建立正式诊断，而不能只依赖肉眼判断。

因此，12D 的目标不是证明当前已经正确，而是把“闭环”定义、检测、报告和后续修复做成稳定功能。

## 4. 正式判定口径

### 4.1 视觉闭环

视觉闭环是 UI 叠加预览中的观察结果，仅用于辅助人工判断。它不能作为生产验收依据。

### 4.2 语义闭环

语义闭环是生产验收依据。它要求在每个 layer 中：

```text
1. 颜色层与模型填充层之间不得存在空白缝隙；
2. 模型材料区域与支撑材料区域之间不得存在空白缝隙；
3. 内部镂空区域默认由 S 支撑填充；
4. 外侧背景必须保持 Empty，不能因闭环修复被误填；
5. 光油层不得覆盖模型填充或支撑语义，只能按优先级参与组合。
```

## 5. 材料优先级

12D 沿用 12A 已确认优先级：

```text
Model > OuterVarnishShell > Support > Empty
```

其中 `Model` 包含：

```text
TextureSurfaceColor
ModelFill
SurfaceVarnish
InnerSurfaceVarnish
```

说明：

1. 模型本体和支撑冲突时，模型优先；
2. 外侧光油壳层和支撑冲突时，外侧光油壳层优先；
3. 闭环修复不得把模型外部背景全部填成支撑；
4. 闭环修复必须输出修复像素统计。

## 6. 决策范围

12D 要做：

```text
1. 定义 material closure 需求和验收；
2. 新增逐层材料缝隙诊断；
3. 新增报告 schema；
4. 设计可选修复策略；
5. 在 UI 中显示闭环状态、问题层和材料缝隙类型；
6. 使用 model/obj 下真实模型建立验证集。
```

12D 不做：

```text
1. 不改变 RGBWSV 通道顺序；
2. 不实现 RIP；
3. 不引入半色调；
4. 不默认切换 OpenVDB；
5. 不把所有外部空白填充为支撑；
6. 不把 preview PNG 当作生产真源。
```

## 7. 后续入口

产品需求见：

```text
docs/slice/PRD/PRD_12D_横截面材料无缝闭环验收与修复.md
```

技术设计见：

```text
docs/slice/DEV/DEV_12D_材料闭环诊断与修复设计.md
```

任务清单见：

```text
docs/codex_task/current/TASKS_12D_横截面材料无缝闭环任务清单.md
```

阶段拆分、schema 和验证入口见：

```text
docs/slice/DOC/DOC_DECISION_12D_R0_R1_R2_R3_材料闭环阶段拆分.md
docs/slice/DOC/DOC_SCHEMA_12D_MaterialClosureReport.md
docs/slice/DEMO/DEMO_12D_横截面材料无缝闭环验证方案.md
docs/slice/DOC/DOC_MATRIX_12D_Fixture与验收矩阵.md
docs/codex_task/current/CODEX_PROMPT_12D_横截面材料闭环执行指令.md
```
