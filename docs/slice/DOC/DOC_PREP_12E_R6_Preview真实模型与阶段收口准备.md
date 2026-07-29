# DOC_PREP_12E-R6 Preview、真实模型与阶段收口准备

> 文档状态：FULL EXECUTION PACKAGE PREPARED / WAIT 09A-05
> 日期：2026-07-20
> 最近审计：2026-07-29
> 覆盖任务：12E-10 Preview、Real Model Matrix 与 REPORT_12E

## 1. 准备结论

12E-10 的输出、依赖、证据所有权、原子任务、矩阵 schema、PRD/DEV/DEMO/TASKS/CODEX_PROMPT
和验收顺序已经补齐。R4/08D、09B、09C、13C-03 依赖已解除，但最终收口仍等待 09A-05：

```text
Preview 依赖 12E-09A-05 同层 UI、12E-09B-05 生产结果绑定和 12E-09C 物理比例显示；
真实模型正向矩阵使用已通过的 xiao_ma/yecan 两族；
复杂浮雕 0/3 作为 BLOCKED_EXPECTED 披露，不再阻断正向矩阵；
生产 package 可复用 12E-08D 已完成证据，并在 09B UI 路径重新验证；
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
xiao_ma_wu_yu_new 当前 R4/08D baseline；
yecan/3.obj；
samples/models/3mf/texture2d_checker_cube.3mf（格式控制）；
aishen_fudiao / meigui_fudiao / titian_fudiao（阻断披露）。
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

最终状态报告固定为：

```text
docs/slice/REPORT/REPORT_12E_全局纹理壳层与模型填充当前状态.md
```

10A 依赖 09A-05、09B-05 和 Stage 13C-03 TIFF 原生生产预览；10B/10C 的 R4/08D、09B、09C
证据前置已完成。10D 等待 10A/10B/10C 后执行。

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
概念、schema 与独立执行文档：COMPLETE；
12E-10A：WAIT 12E-09A-05；Stage 13C-03、12E-09C 已完成；
12E-10B：PREPARED / 09B、09C EVIDENCE AVAILABLE；
12E-10C：PREPARED / 08D、09B、09C EVIDENCE AVAILABLE；
12E-10D：WAIT 10A/10B/10C。
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

## 10. 2026-07-22 准备度刷新

```text
12E-10 概念任务拆分、Final Closure Matrix schema 和独立执行文档：COMPLETE；
12E-10A：09B-05、09C 物理比例显示和 13C-03 TIFF 原生生产预览已完成，等待 12E-09A-05 同层语义 preview；
12E-10B：xiao_ma/yecan/3MF 控制/复杂浮雕阻断矩阵已冻结，09B-06、09C 已完成；
12E-10C：可复用 R4-07-R2、08D-06、09B-06 和 09C-06 Release 证据；
12E-10D：等待 10A/10B/10C 后收口；
生产 package/RIP：08D 前置和 09B UI 路径均已验证。
```

## 11. 2026-07-24 09C 后准备度审计

```text
09C：COMPLETE；
10A：仅被 09A-05 阻断；
10B/10C：技术证据前置已满足，可进入执行文档准备；
10D：等待 10A/10B/10C；
独立 PRD、DEV、DEMO、TASKS、CODEX_PROMPT：COMPLETE；
结论：12E-10 执行包完整，但主线必须等待 09A-05/06 后从 10A 开始。
```

## 12. 2026-07-24 Stage 13 依赖补充

Stage 13 新需求不改变 12E-10 的单模型双引擎收口范围，但会替换生产预览的数据来源：

```text
13C-03 负责从 RGBWSV TIFF 派生 RGB/W/S/V 和 RGB+S+W+V；
09A-05 在同一真实 layerIndex 上叠加 Texture/Fill/Partition 诊断语义；
10A 验证两者的同层一致性；
不再要求 10A 为生产材料重复生成逐通道 preview PNG。
```

## 13. 2026-07-27 优先级与冻结补充

```text
13A-01/13B-01 先冻结 scene identity；
随后执行 scene-aware 09A-02；
13C-03 和 09A-05 完成后才启动 10A；
10B/10C 可提前补齐执行文档，但不得把局部证据写成 12E-10 COMPLETE；
12G-TCWS 纹理载体/白色分色/RIP 铺底专项已冻结，不属于 12E-10。
```
