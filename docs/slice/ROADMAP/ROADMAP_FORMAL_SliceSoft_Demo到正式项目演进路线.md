# ROADMAP_FORMAL_SliceSoft_Demo到正式项目演进路线

> 文档版本：v0.1
> 文档状态：Formal Roadmap
> 生成日期：2026-06-30
> 当前阶段：Stage 12E-09C、09A-01/02 已完成；Stage 13 P0 设计和原子任务准备完成，13A-01..05/13B-01..04 已实现

---

## 1. 路线图目标

本路线图回答：

```text
1. 当前项目如何从 demo 演进为正式项目；
2. 每个阶段解决什么问题；
3. 哪些阶段是生产准入前置；
4. 09P-R2 前后如何安排；
5. 后续 PRD / DEV / TASKS / REPORT 如何生成。
```

---

## 2. 总体阶段线

当前项目已经走过：

```text
P0 / 00A / 00B / 00C
→ 01 / 02 / 03 / 03B / 03C
→ 04 / 04A / 05 / 05A / 06 / 06A / 06B
→ 07 / 07A / 07B / 07B-R1
→ R0 / R1 / R2
→ 08 / 08A
→ 09 / 09A / 09A-R1 / 09A-R2
→ 09B / 09B-R1 / 09B-R2 / 09B-R3
→ 09P-R1
```

当前位置：

```text
09P-R2 hardening 已完成
10 切片输出交付契约与纹理保真验收已完成
当前：Stage 13 P0 需求/设计/验证及 17 个近程原子任务准备完成 / 13A-01..05、13B-01..04、跨阶段 09A-02 COMPLETE / 13B-05 FIXTURE READY
```

推荐后续：

```text
10
→ 11
→ 09P-R3 / 09P-R4 或 mesh repair / admission gate 专项，可按风险插入
```

---

## 3. 从 demo 到正式项目要发生的变化

### 3.1 阶段 1：功能 demo

目标：

```text
证明最小闭环能跑通。
```

代表阶段：

```text
P0 到 06B
```

产物：

```text
slicer_cli
rip_reader_test
RGBWSV TIFF
manifest
OBJ/MTL/3MF/Texture2D/ColorGroup
MaterialPolicy / Support / Varnish
```

风险：

```text
代码集中；
report 不统一；
配置缺少 schema；
真实模型质量问题未准入化。
```

### 3.2 阶段 2：调试与工程化

目标：

```text
让工程师能调试、查看、回归。
```

代表阶段：

```text
07 / 07A / 07B / R0 / R1 / R2
```

产物：

```text
Qt Debug UI
config editor
profile visualization
module boundary wrappers
ConfigSchema / ConfigMigration
ReportBase
schema tests
golden tests
run_ci_quick
```

风险：

```text
模块边界虽然建立，但 legacy 文件仍有大量职责；
文档阶段过多，当前入口混乱。
```

### 3.3 阶段 3：OpenVDB 实验几何能力

目标：

```text
证明 OpenVDB/SDF 对真实模型的表面壳层纹理路线可行。
```

代表阶段：

```text
09 / 09A / 09B / 09B-R1/R2/R3
```

产物：

```text
OpenVDB smoke
surface shell / interior
nearest triangle / texture transfer
真实 OBJ/3MF golden
topology diagnostics
stable issue code
process peak memory
ProductionAdmissionPolicy 前置依据
```

结论：

```text
OpenVDB 壳层纹理实验链路可跑通；
真实 OBJ/3MF 仍不是 production-safe；
必须通过 strict admission 或 repair_then_strict 才能进入生产输出。
```

### 3.4 阶段 4：Experimental production pipeline

目标：

```text
把 OpenVDB 实验能力接入正式 pipeline 边界，但不默认生产输出。
```

代表阶段：

```text
09P-R1
```

已完成：

```text
feature flag
experimental CLI diagnostic path
ProductionAdmissionPolicy
OpenVdbGeometryKernelService
SurfaceShellTextureService
MaterialChannelComposer bridge
09P validation script
```

### 3.5 阶段 5：Hardening / production candidate

目标：

```text
把 experimental path 做成可回归、可 UI 展示、可生产准入判断的候选路径。
```

代表阶段：

```text
09P-R2 / 09P-R3 / 09P-R4
```

---

## 4. 09P-R2 计划

09P-R2 定位：

```text
OpenVDB experimental production pipeline hardening。
```

阶段目标：

```text
1. 固化 experimental report schema；
2. 扩展 topology admission gate；
3. 明确 mesh repair 前置判断；
4. 收敛 OpenVDB / texture / composer 数据契约；
5. 设计 RGBWSV experimental golden / downstream output contract / texture fidelity compatibility；
6. Qt Debug UI 对接 experimental report；
7. 建立 OpenVDB OFF / ON CI matrix；
8. 输出 REPORT_09P_R2。
```

阶段红线：

```text
不默认启用 OpenVDB
不替代 legacy slicer_cli
不直接写真实 OBJ/3MF production RGBWSV
不修改 p0.rgbwsv.2
不把 warn_and_attempt 视为 production-safe
不做大规模 mesh repair 实现
```

---

## 5. 09P-R3 计划

09P-R3 定位：

```text
Qt UI / Profile / Report / CI 工程化。
```

阶段目标：

```text
1. Qt UI 读取 experimental report；
2. 显示 OpenVDB status；
3. 显示 productionAdmission；
4. 显示 blockerCodes / warningCodes；
5. Profile 中展示 surface shell 参数；
6. CI 增加 OpenVDB ON smoke lane；
7. report viewer 支持 experimental schema。
```

---

## 6. 09P-R4 计划

09P-R4 定位：

```text
Production gate / release candidate。
```

阶段目标：

```text
1. 建立真实模型集合验收；
2. 建立性能与内存门槛；
3. 建立 productionAllowed 的 release gate；
4. 明确哪些 profile 可启用 surface_shell；
5. 判断是否允许 production experimental；
6. 生成 REPORT_09P_R4。
```

---

## 7. Mesh Repair / Admission Gate 专项

该阶段可能插入在 09P-R2 后、09P-R3 前，也可能并行调研。

触发条件：

```text
真实 OBJ/3MF 仍因 non-manifold、duplicate/opposite duplicate、local winding、多组件无法 strict admission。
```

阶段目标：

```text
1. duplicate / opposite duplicate face repair 评估；
2. local winding repair 评估；
3. multi-component admission policy；
4. repair report；
5. repair 前后 hash；
6. repair_then_strict 是否可落地。
```

不应做：

```text
不把自动 repair 作为隐式默认；
不绕过 stable issue code；
不把 repair 后未复验模型标记为 production-safe。
```

---

## 8. 09C / 09D / 10 / 11

### 8.1 09C：SDF compensated varnish prototype

目标：

```text
基于 SDF distance / surface normal 研究光油补偿几何。
```

边界：

```text
prototype 隔离；
不直接写 production V channel；
不修改 p0.rgbwsv.2。
```

### 8.2 09D：SDF support clearance / overhang diagnostics

目标：

```text
基于 SDF distance / normal / overhang angle 研究支撑 clearance 和 overhang 诊断。
```

边界：

```text
不替代 SupportShapePipeline；
不直接修改 production S channel。
```

### 8.3 10：切片输出交付契约 / 纹理保真验收

进入条件：

```text
RGBWSV package 稳定；
下游消费契约清楚；
纹理 / UV / 材质映射保真策略清楚；
production profile 清楚；
失败策略清楚；
输出 package / manifest / report / layer summary 足够下游 RIP 工程师消费。
```

边界：

```text
不实现 RIP 半色调；
不实现设备通信；
不实现喷头 bitstream；
不把 RIP 工序并入本项目主线。
```

### 8.4 11：UI 切片层预览 / 交互配置 / 多模型能力评估

目标：

```text
让切片完成后的数据能在 UI 软件中按层浏览；
通过伪彩显示 RGBWSV / mask / support / varnish / texture fidelity 等关键层信息；
优化当前 UI 布局和显示效果；
把常用配置从手工编辑配置文件迁移到 UI 控件；
评估并设计多模型导入、排版、联合切片或顺序切片能力。
```

推荐拆分：

```text
11A：Layer Preview Data Contract，定义 UI 可读取的层数据、缩略图、统计和伪彩映射；
11B：Layer Slider / Pseudo Color Viewer，实现按层滑动、通道切换、缩放和平移；
11C：UI Layout Refresh，调整作业区、预览区、参数区、报告区布局；
11D：Interactive Settings Panel，将常用配置做成按钮、下拉、滑块、复选框；
11E：Multi-Model Capability Decision，判断 11 阶段内的多模型最小范围、数据模型和是否只做评估；
11F：UI Smoke / Golden Preview，建立层预览与配置面板的自动化验证。
```

阶段边界：

```text
UI 不直接访问 slicer.cpp 内部临时结构；
UI 不直接依赖 OpenVDB 类型；
UI 通过 package/report/preview data contract 读取切片结果；
11 阶段不改变 p0.rgbwsv.2；
11 阶段不默认引入多模型 production 输出，先完成能力评估和数据模型设计。
```

---

### 8.5 13：模型场景、排版联合切片与 TIFF 原生预览

Stage 11 只完成多模型能力决策。2026-07-24 用户已提出正式生产需求，因此新增独立 Stage 13：

```text
13A：切片前模型俯视、选择、XY/rotateZ/uniformScale/mirror 变换；
13B：最多 11 列 x 2 行、20/30 mm 可配置净距和多模型联合切片；
13C：直接从 RGBWSV TIFF 显示单通道、伪彩和 RGB+S+W+V，减少重复 preview IO。
```

执行关系：

```text
13A-01 + 13B-01
  -> scene-aware 12E-09A-02
  -> 13A/13B P0
  -> 13C
  -> 12E-09A-03..06
  -> 12E-10。
```

Stage 13 不修改 RGBWSV 协议；自动 nesting、跨模型联合支撑和完整 3D gizmo 属于后续阶段。
12G-TCWS 纹理载体/白色分色/RIP 铺底候选专项于 2026-07-27 冻结，不属于 Stage 13 或当前执行
序列。

---

## 9. 文档产出规则

每个后续阶段必须输出：

```text
docs/slice/PRD/PRD_<stage>
docs/slice/DEV/DEV_<stage>
docs/slice/DEMO/DEMO_<stage>
docs/slice/REPORT/REPORT_<stage>
docs/slice/DOC/DOC_DECISION_<stage>，如有不可逆决策
docs/codex_task/current/TASKS_<stage>
docs/codex_task/current/CODEX_PROMPT_<stage>
```

09P-R2 已补齐并完成：

```text
docs/slice/PRD/PRD_09P_R2_OpenVDB实验生产管线Hardening.md
docs/slice/DEV/DEV_09P_R2_ReportSchema_AdmissionGate_CI_UI设计.md
docs/slice/DEMO/DEMO_09P_R2_OpenVDB实验生产管线Hardening验证方案.md
docs/codex_task/current/TASKS_09P_R2_正式化前置文档治理与Hardening任务清单.md
docs/codex_task/current/CODEX_PROMPT_09P_R2_OpenVDB实验生产管线Hardening执行指令.md
docs/slice/REPORT/REPORT_09P_R2_OpenVDB实验生产管线Hardening当前状态.md
```

10 阶段已完成，见：

```text
docs/slice/PRD/PRD_10_切片输出交付契约与纹理保真验收.md
docs/slice/DEV/DEV_10_OutputContract_TextureFidelity设计.md
docs/slice/DEMO/DEMO_10_切片输出契约与纹理保真验证方案.md
docs/slice/DOC/DOC_DECISION_10_RIP边界与切片输出契约.md
docs/codex_task/current/TASKS_10_切片输出交付契约与纹理保真验收任务清单.md
docs/codex_task/current/CODEX_PROMPT_10_切片输出交付契约与纹理保真验收执行指令.md
docs/slice/REPORT/REPORT_10_切片输出交付契约与纹理保真验收当前状态.md
```

11 阶段历史入口：

```text
docs/slice/PRD/PRD_11_UI切片层预览交互配置与多模型能力.md
docs/slice/DEV/DEV_11_LayerPreview_UIConfig_MultiModel设计.md
docs/slice/DEMO/DEMO_11_UI切片层预览交互配置验证方案.md
docs/slice/DOC/DOC_DECISION_11_多模型切片处理范围决策.md
docs/codex_task/current/TASKS_11_UI切片层预览交互配置与多模型评估任务清单.md
docs/codex_task/current/CODEX_PROMPT_11_UI切片层预览交互配置与多模型评估执行指令.md
```

Stage 13 当前入口：

```text
docs/slice/DOC/DOC_DECISION_13_模型场景排版与TIFF原生预览专项拆分.md
docs/slice/ROADMAP/ROADMAP_13_模型场景排版联合切片与TIFF预览路线.md
docs/slice/REPORT/REPORT_13_模型场景排版与TIFF原生预览准备状态.md
docs/codex_task/current/TASKS_13_模型场景排版联合切片与TIFF预览任务清单.md
docs/codex_task/current/CODEX_PROMPT_13_模型场景排版联合切片与TIFF预览执行指令.md
```

---

## 10. 总结

当前最稳路线：

```text
先整理文档真源
→ 09P-R2 hardening 已完成
→ 10 输出契约与纹理保真验收已完成
→ 11 已完成 UI/多模型能力评估
→ 12A..12D 已收口，12E-09C 已完成
→ Stage 13 P0 总体文档、17 个近程原子任务准备与首批合同完成
→ 13A-01..05、13B-01..04、scene-aware 12E-09A-02 COMPLETE
→ 下一任务 13B-05 全局 Raster 与联合层合成 FIXTURE READY
```

不要把 Stage 11 的 capability decision 误解成多模型 production 已实现。Stage 13 必须通过 scene identity、
实例准入、幅面/碰撞、联合 package 和 TIFF 原生预览逐项建立证据。
