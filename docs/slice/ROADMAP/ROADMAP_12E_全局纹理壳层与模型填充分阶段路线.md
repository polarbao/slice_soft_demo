# ROADMAP_12E 全局纹理壳层与模型填充分阶段路线

> 文档状态：ROADMAP / Stage 12E Planning
> 日期：2026-07-16
> 更新日期：2026-07-27
> 当前项目执行阶段：12D COMPLETE；12E-01..07、12E-08A/08B/08C、R1/R2/R3/R4 COMPLETE；R4-08-R2 GO；12E-08D-01..06 COMPLETE；12E-09B-01..06 COMPLETE；12E-09C-01..06 COMPLETE；12E-09A-01/02 COMPLETE；下一执行入口为 Stage 13A-02

## 1. Goal

把 12A 已定义但未形成完整组合的 `Texture Surface Layer` 和 `Model Fill Layer` 收敛为全模型三维互补分区，并通过动态宽度、UI、report、closure 和 regression 形成可执行闭环。

## 2. Activation Gate

```text
1. 12D-R0/R1/R2/R3 已完成；
2. 12E-R0 的 Config/DTO、report schema、fixture matrix 和启动状态准备已完成；
3. 12E-01/02/03/04/05/06/07 与 12E-08A/08B/08C、R1/R2/R3、R4-01..07/R1/R2、Quick-CI-R1 和 12E-08D-01..06 已完成；两个 Global 显式候选通过 0.01 mm TIFF/RIP，Legacy 仍因性能与内存保持默认；12E-09A diagnostic 任务可独立执行；12E-09B-01 已完成，09B-02 为下一任务；12E-09C DPI 专项已准备；
4. 12E composer/production 接入需要已完成的 12D semantic_masks exact contract；
5. 不要求 12D repair R3 完成后才能做 12E R1 算法原型，但 production admission 必须重新确认；
6. 12E-01 已完成配置、DTO、稳定错误码和 unavailable report 骨架；未接入 production generation。
```

12E-08D 前新增正式插入路线：

```text
12E-08C-R1 Contract & Eligibility；
12E-08C-R2 Conservative Repair；
12E-08C-R3 Real Model & Release Gate；
12E-08C-R4 Model Preflight & Repair Asset Admission。
```

详细路线以 `ROADMAP_12E_08C_真实模型拓扑修复分阶段路线.md` 为准。

## 3. R0：契约与准入

输出：

```text
12E Decision/PRD/DEV/DEMO/ROADMAP/TASKS/CODEX_PROMPT；
global_surface_shell config schema；
texture/fill partition DTO；
slicesoft.texture_fill_partition.12e.1 report schema；
Config/DTO 准备文档和 fixture/验收矩阵；
minimum/dynamic maximum/allTexture contract；
backend role decision。
```

退出标准：

```text
不变量、配置兼容、fail policy、UI 字段和验证矩阵冻结；
global_surface_shell 在 backend 未实现时于写包前明确阻断；
默认 USE_OPENVDB=OFF 不改变。
```

## 4. R1：全局 3D 分类原型

输出：

```text
backend-neutral GlobalTextureFillPartitionService；
完整 3D occupancy/distance/closest-surface contract；
Legacy CPU production candidate prototype；
OpenVDB optional conformance adapter；
box/sphere/thin-wall/cavity/non-manifold fixtures；
性能和内存 baseline。
```

退出标准：

```text
partition invariants PASS；
width sweep 单调；
全纹理阈值成立；
global 3D 几何证据通过；
任何 backend 都不写 production TIFF。
```

## 5. R2：Composer 与 Closure 接入

输出：

```text
TextureSurfaceMask3D/ModelFillMask3D layer view；
最近表面纹理传递；
semantic composer exact masks；
texture_fill_partition_report；
12D material closure 联合诊断；
diagnostic-only package comparison。
```

退出标准：

```text
texture + fill = model；
overlap/unassigned = 0；
allTexture 合法 fill=0；
旧 Profile 默认输出保持兼容；
尚未通过 production gate 的结果不标记 production-safe。
```

## 6. R3：UI 与 Production Admission

输出：

```text
Qt 全局纹理策略；
0.01 mm slider/spinbox；
动态 min/max/allTexture threshold；
coverage/partition/diagnostics；
session effective config；
新 Profile candidate；
production admission decision。
```

退出标准：

```text
UI self-test/smoke 通过；
默认 OFF lane 通过；
RIP strict 和 RGBWSV 协议通过；
性能、内存、拓扑、纹理传递和 closure gate 全部满足；
未满足时保持 diagnostic-only。
```

## 7. R4：真实模型回归与收口

输出：

```text
真实 OBJ/3MF matrix；
minimum/intermediate/allTexture golden；
Release runtime/peak memory report；
用户手册同步；
REPORT_12E。
```

退出标准：

```text
典型甲片、薄壁、内腔、纹理缺失和 topology blocker 均有证据；
完成报告区分 Current/Target/Historical/Pending；
明确 production admitted 或 keep diagnostic。
```

## 8. Dependencies

```text
12A：ModelFill/TextureSurface/Support/Varnish 语义；
12B-R2：OpenVDB optional SDF utility 角色；
12C：Profile/effective config/Preview/Diagnostics UI 框架；
12D：semantic mask exact 和 material closure 诊断；
Architecture：core 不依赖 Qt，UI 不访问 OpenVDB 内部类型。
```

## 9. Stop Conditions

出现以下情况必须停止 production 推进并保留 diagnostic-only：

```text
没有默认 OFF production candidate；
partition overlap/unassigned 非 0；
global 3D 几何验证失败；
strict topology blocker 被绕过；
全纹理模式无法证明 model 全覆盖；
性能/内存未形成实际 baseline；
UI effective config 与核心实际值不一致；
RGBWSV 或 RIP strict 回归失败。
```

## 10. Rollback

```text
global_surface_shell 必须显式启用；
旧 Profile 和旧 config parser 默认行为保留；
新 report/preview 缺失不影响旧路径；
UI 新控件可按 capability 隐藏；
OpenVDB 保持 optional/OFF；
production gate 失败时不写生产包。
```

## 11. 双模式生产化增量路线

为满足 UI/配置可选 `legacy` 与 `global_surface_shell`，原 R3 production admission 进一步拆为：

| 阶段 | 工作 | Gate |
|---|---|---|
| 12E-08C-R1/R2/R3 | 真实模型 repair-then-strict、属性保持、完整自相交证据、Release 预算 | COMPLETE；R3-04 NO-GO |
| 12E-08C-R4-01..06 | 模型预检、模式准入、Qt 阻断、正常模型正向矩阵和 required-family intake | 软件实现 COMPLETE；真实 family 0/3，不解除 required Gate |
| 12E-08C-R4-07/R1/R2 | Development/受限候选/预算 Gate | 两独立模型族、四 case、Release 候选预算、legacy/RIP 全部 PASS |
| Quick-CI-R1 | Golden Profile/Fixture 解耦 | Golden 连续两轮与完整 Quick CI PASS |
| 12E-08C-R4-08-R2 | 08D GO/NO-GO | GO；技术 Gate 和独立授权完成 |
| 12E-08D-01 | `slicePipeline.mode`、DTO、validator、Router 与 fail-closed | COMPLETE；省略字段兼容 legacy；非法值拒绝；无静默回退 |
| 12E-08D-02 | global classification 到现有生产层 DTO 的 adapter | COMPLETE；通道、真实层序和 material closure 语义完整，不写文件 |
| 12E-08D-03 | COMPLETE：两模式共享 RGBWSV writer、manifest、preview/report 与 RIP 回归 | synthetic Legacy/Global 与 Global Adapter 桥接均生成一致 TIFF；RIP strict PASS |
| 12E-08D-04 | COMPLETE：真实模型 Release matrix 与 GO/NO-GO | restricted Profile GO；ordinary parity NO-GO |
| 12E-09B | Qt 双模式选择、能力提示和 Production Effective Config | 09B-01 COMPLETE；仅对 admitted restricted/material-parity Profile 开放；中文入口；requested/effective 可追踪 |
| 12E-09C | X/Y DPI 非等方配置与协议兼容 | 默认 635/600；显式 600/600 兼容；Core/Reader/两引擎/光油/Qt/preview 同合同 |
| 12E-09A-02 | Diagnostic Effective Config | COMPLETE；single_model/scene 身份、事务、hash 和 stale 已冻结 |
| 12E-09A-03..06 | 中文控件、worker 和同层语义 preview | 独立支线；按 Stage 13 依赖顺序推进，09A-05 是 10A 前置 |
| 12E-10 | 双模式真实模型、preview、RIP、性能与报告收口 | 09B/09C 已完成；等待 09A-05，并补齐独立执行文档；输出矩阵完整，残余风险明确 |
| Stage 13 前置 | ModelInstance/scene identity 与 TIFF 原生生产预览 | 13A-01/13B-01 先于 09A-02；13C-03 先于 09A-05；多模型生产由 Stage 13B 独立验收 |

`legacy` 始终保留为默认生产路径。`global_surface_shell` 的 CPU/OpenVDB 后端选择是内部能力，不能成为
第三个产品切片模式，也不能因后端不可用而静默改变端到端流水线。
