# DOC_DECISION_12D R0/R1/R2/R3 材料闭环阶段拆分

> 文档状态：Accepted
> 日期：2026-07-13
> 前置阶段：12A 材料语义、12C Qt 工作台

## 1. 决策结论

12D 按以下四个阶段执行：

```text
R0：需求、schema、fixture 和执行边界冻结；
R1：配置、报告骨架和 TIFF 候选诊断；
R2：semantic mask 精确诊断与 repair-disabled 生产不变性；
R3：显式 1px 修复、外部背景保护、UI 和真实模型验收。
```

本次文档收口完成 R0。12D 代码实施须等待 12C-R2-05 完成，避免同时修改 UI 诊断和预览结构。

## 2. 背景

当前 RGBWSV TIFF 能表达最终通道值，但不能完整区分纹理表层、模型填充、内部镂空支撑和不同来源光油。直接从 TIFF 自动修复会把“像素打印状态”误当作“材料业务角色”。另一方面，完全等待 semantic masks 后再建立报告，会失去早期发现真实输出候选缝隙的能力。

因此需要把 candidate 诊断、exact 诊断和 repair 分离，分别设置不同安全门槛。

## 3. 冻结口径

### 3.1 生产真源

```text
生产验收真源：composer semantic masks + RGBWSV TIFF；
辅助证据：material_closure_report.json；
人工定位：gap preview；
禁止：依据 UI 伪彩或 preview PNG 推断生产闭环通过。
```

### 3.2 状态判定

```text
source=semantic_masks, confidence=exact：允许 pass/fail；
source=rgbwsv_tiff_inferred, confidence=candidate：最多 warning，不允许 production pass；
source 不可用：not_available；
critical gap > 0 且 failOnGap=true：fail；
critical gap = 0：pass。
```

### 3.3 修复宽度

```text
R1/R2：只诊断，不修改生产 mask；
R3：只允许 maxGapPx=1 的显式修复；
2px 及以上只报告，不自动修复；
repair.enabled 默认 false；
rgbwsv_tiff_inferred 禁止修复。
```

### 3.4 光油与支撑

`VarnishSupportGap` 只有在 `SupportRequiredMask` 明确要求外侧光油与支撑接触时才成立。正常外部空气不是 gap，不得补为支撑。

### 3.5 Gap Preview

`writeGapPreview` 默认 false。fixture/人工诊断可显式开启；预览必须标注 `diagnostic_only`，不得成为生产验收真源。

## 4. 备选方案与取舍

### 4.1 直接从 TIFF 检测并修复

拒绝。TIFF 能识别 RGB/W/S/V 是否打印，但不能可靠恢复每个像素的业务 mask 来源，存在误填模型或外部背景的风险。

### 4.2 只做 semantic mask exact，不提供 candidate

未采用。安全性高，但在 exact 链路完成前无法对已有真实 package 提供候选诊断。保留 candidate 有助于排查，但必须禁止 production pass 和 repair。

### 4.3 一次性实现诊断、修复和 UI

拒绝。该方案同时修改 config、composer、report、TIFF 和 Qt UI，回归面过大，也无法证明 repair-disabled 不变性。

### 4.4 分阶段 candidate -> exact -> repair

采用。它使每一阶段都能独立验证，并把生产修改推迟到 exact mask 与背景保护通过之后。

## 5. 阶段退出标准

### R0 文档准入

```text
PRD 开放项关闭；
DEV 补充 required-domain 与修复限制；
report schema 独立成文；
DEMO、fixture matrix、TASKS、CODEX_PROMPT 和准备报告齐全。
```

### R1 候选诊断

```text
materialClosure 配置可解析并校验；
material_closure_report.json 可生成；
TIFF inferred 输出 confidence=candidate；
candidate 结果不能显示 production pass；
不修改 TIFF。
```

### R2 精确诊断

```text
composer 输出 exact semantic masks；
逐层 gap 分类不依赖 preview；
repair disabled 前后 TIFF hash 相同；
slice_report 汇总 closureStatus 和 worst layer。
```

### R3 修复与验收

```text
只修复 1px gap；
外部背景保持全 255；
UI 可显示状态、gap 类型和 worst layers；
三个真实模型有可追溯报告；
RGBWSV 协议保持不变。
```

## 6. 影响

正向影响：

```text
candidate 与 production evidence 不再混淆；
repair-disabled 可先证明零生产输出影响；
报告、UI 和修复共用稳定 schema/reason code；
真实模型失败可以定位到具体层和 gap 类型。
```

代价：

```text
需要保留 TIFF inferred 和 semantic mask 两条诊断来源；
composer 需要输出额外只读 mask；
完整功能必须经过 R1/R2/R3，不能一次交付；
真实模型可能继续 fail，但必须以报告形式解释。
```

## 7. 模块边界

```text
config：解析 MaterialClosureConfig；
pipeline/composer：提供只读 semantic masks；
diagnostics：检测、分类、汇总 gap；
material policy/composer：仅在显式 repair 时应用修复 mask；
reports：序列化既有诊断结果，不拥有业务决策；
Qt UI：只读展示报告，不自行重新判定闭环。
```

## 8. 验证

```text
R1：schema/candidate invariant 自动检查；
R2：semantic fixture 与 repair-disabled TIFF SHA-256；
R3：1px/2px fixture、external background guard、RIP reader、Qt smoke 和真实模型报告。
```

具体验证矩阵见 `DEMO_12D_横截面材料无缝闭环验证方案.md` 和 `DOC_MATRIX_12D_Fixture与验收矩阵.md`。

## 9. 安全边界

```text
不修改 p0.rgbwsv.2；
不修改 R G B W S V；
不修改 uint8 / black_is_print；
不默认开启 repair；
不默认开启 OpenVDB；
不把 external background 填为支撑；
不将 candidate 诊断当作 exact production evidence。
```
