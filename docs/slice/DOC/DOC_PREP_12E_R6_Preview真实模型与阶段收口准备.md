# DOC_PREP_12E-R6 Preview、真实模型与阶段收口准备

> 文档状态：PREPARED / EXECUTION BLOCKED BY 09A AND 08C-R3
> 日期：2026-07-20
> 覆盖任务：12E-10 Preview、Real Model Matrix 与 REPORT_12E

## 1. 准备结论

12E-10 的输出、依赖、证据所有权、原子任务和验收顺序已明确，准备工作完成。但实现不能提前开始：

```text
Preview 依赖 12E-09A 同层 UI/diagnostic facade；
真实模型生产矩阵依赖 12E-08C-R3 repair/post-strict；
生产 package 收口依赖 12E-08D；
最终 REPORT_12E 必须记录 PASS 或 keep diagnostic，而不是默认宣称 production。
```

## 2. 输出范围

```text
Texture Surface preview；
Model Fill preview；
Partition composite preview；
Support/Varnish/full-closure 同层叠加；
真实 OBJ/3MF minimum/intermediate/allTexture matrix；
Release runtime/peak memory summary；
用户手册；
REPORT_12E。
```

## 3. Preview 合同

所有 preview 必须使用同一 `layerIndex/zMm`，消费真实 raster semantic masks，不按 PNG 序号猜层号。

```text
Texture Surface：显示真实纹理 RGB；
Model Fill：按 W/V/RGB 材料角色伪彩；
Partition：两者互补，overlap/unassigned=0；
Support/Varnish：保持现有可配置 overlay 色；
Empty：白色背景；
blocked/not_evaluated：明确状态，不渲染伪 PASS。
```

preview 是显示证据，不替代 TIFF/report/RIP。

## 4. Real Model Matrix

固定首版 cases：

```text
nai_you_new；
aishen_fudiao；
meigui_fudiao；
Texture2D/ColorGroup 3MF fixture。
```

每个 case 固定三个宽度点：effective minimum、representative intermediate、allTexture threshold。若模型在
12E-08C-R3 为 manual/blocked，则 matrix 必须保留 blocked 行和原因，不能省略或伪造数值。

## 5. Evidence Ownership

```text
mesh_repair_report：修复、属性和 post strict；
texture_fill_partition_report：宽度、分区、纹理传递、raster 和 closure；
manifest/slice report：未来 08D production package；
RIP Reader：package 协议；
Release summary：core time/peak memory；
UI smoke：显示与交互。
```

## 6. 原子任务

```text
12E-10A：Texture/Fill/Partition 同层 preview 与状态图例；
12E-10B：真实 OBJ/3MF minimum/intermediate/allTexture matrix；
12E-10C：Release core/repair/peak-memory 汇总与阈值结论；
12E-10D：用户手册、REPORT_12E、索引和上下文封口。
```

10A 依赖 09A-05；10B/10C 依赖 08C-R3；生产 package/RIP 行依赖 08D。10D 可以输出
`keep diagnostic`，不强制以 production PASS 结束阶段。

## 7. 验收

```text
同层 layerIndex/zMm；
preview 与 semantic mask count 一致；
blocked case 不显示假覆盖率/假 0 ms；
真实模型 matrix 不遗漏；
Release 计时排除写盘；
协议和旧 Profile 回归保持；
REPORT_12E 列出实际命令、结果、阻断和后续建议。
```

## 8. 当前 Gate

```text
文档准备：COMPLETE；
12E-10A：BLOCKED BY 12E-09A-05；
12E-10B/10C：BLOCKED BY 12E-08C-R3；
12E-10 production package evidence：BLOCKED BY 12E-08D；
12E-10D：等待前述证据后执行。
```

## 9. 双模式阶段收口补充

12E-10 的真实模型矩阵必须为每个模型分别记录 `legacy` 与 `global_surface_shell`。global 未准入或被拓扑
阻断时保留明确的 blocked 行，不允许以 legacy 结果代替。

两种生产成功行都必须包含：

```text
生产 TIFF layer count / hash / RIP strict；
RGBWSV 通道统计；
Texture/Fill/Support/White/Varnish 同层 preview；
requested/effective pipeline；
core、compose、TIFF、preview/report 写盘分段耗时；
真实模型 grid/layerCount/peak memory。
```

Preview 不增加第二套生产文件格式。两种模式都读取现有 package 与 report；差异仅来自 layer composer。
global 诊断结果可单独显示，但必须标注“诊断，未生成可打印 TIFF”。
