# DOC_DECISION_12_11B后进入切片语义引擎性能与UI产品化专项

> 文档版本：v0.1
> 文档状态：DOC_DECISION / Stage 12 Entry
> 生成日期：2026-07-05
> 适用范围：11B 后续专项阶段拆分、需求校准、开发优先级

---

## 1. 决策结论

11B 之后不应继续用零散 bugfix 方式处理彩色纹理、支撑、光油、OpenVDB 性能和 UI 配置问题。应成立 Stage 12 专项组，先固化产品语义，再进入实现。

Stage 12 拆成三个并行但有顺序依赖的专项：

```text
12A：彩色纹理模型切片语义、填充层、支撑、外侧光油策略专项
12B：切片引擎性能、OpenVDB 替代评估与高效引擎路线专项
12C：Qt UI 配置、Profile、预览入口和报告曲线工作台专项
```

优先级：

```text
P0 = 12A 需求与语义校准
P1 = 12C UI 可操作性收口
P1 = 12B core-only benchmark 与引擎路线判断
P2 = 12A/12B 中涉及 OpenVDB 替代 legacy 的生产候选实现
```

原因：如果 12A 的“每层模型真实数据、填充层、支撑层、光油壳层”语义没有先确定，任何引擎性能比较都可能不可比，任何 UI 选项也会变成临时开关。

---

## 2. 证据等级

本决策使用以下证据等级：

```text
A：当前代码、配置、脚本、测试、最新 REPORT 中列明的验证结果
B：docs/slice 正式 PRD / DEV / ROADMAP / DOC_DECISION
C：归档历史文档和历史会话背景
D：与 A/B 冲突或已被新阶段替代的旧描述
```

关键 A/B 证据：

```text
A：src/slicer_core/config.h
A：src/slicer_core/slicer.cpp
A：apps/slicer_debug_ui/MainWindow.cpp
A：apps/slicer_cli/main.cpp
A：scripts/run_11b_openvdb_legacy_core_benchmark.ps1
B：PRD_FORMAL_SliceSoft_正式切片软件产品需求总览.md
B：DEV_FORMAL_SliceSoft_正式切片软件总体技术方案.md
B：PRD_10_切片输出交付契约与纹理保真验收.md
B：PRD_11_UI切片层预览交互配置与多模型能力.md
B：PRD_11B_UI配置生产预览与OpenVDB姿态收口.md
B：REPORT_11B_UI配置生产预览与OpenVDB姿态收口当前状态.md
```

---

## 3. 当前策略是否正确

### 3.1 正确的部分

当前项目主线策略没有根本偏离正式 PRD / DEV：

```text
1. legacy slicer_cli 仍是默认 production path；
2. RGBWSV p0.rgbwsv.2 协议保持稳定；
3. OpenVDB 仍为 optional / diagnostic / candidate，不默认替代 legacy；
4. Qt UI 通过 config/package/report/preview 工作，不直接依赖 slicer.cpp 临时结构；
5. 11B 已开始区分 production RGB 与 texture preview；
6. 已有 core-only benchmark 的基本入口，避免把 TIFF/preview/report I/O 混入核心切片耗时。
```

这些符合正式文档中的红线。

### 3.2 出现偏差或不完整的部分

当前偏差主要不是协议偏差，而是“产品语义没有完全固化”：

```text
1. 彩色纹理模型每层数据当前能写 RGB，但“颜色层 / 填充层 / 支撑层 / 光油层”的产品语义还不够清楚；
2. 当前 fill 行为主要由 texture.applyMode + nonSurfaceRgbPolicy + modelMaterial/materialPolicy 隐式组合，不是显式的 ModelFillPolicy；
3. 支撑已有 bottom_projection / unsupported_only / bottom_projection_plus_unsupported / full_vertical_projection，但缺少面向甲片业务的“下表面/上表面/上下表面/内部镂空补支撑”明确需求；
4. 外侧光油壳层当前只有 VarnishGeometryPolicy::AdditiveGrow 枚举边界，尚无 production 实现；
5. OpenVDB candidate 当前 outputSemanticsComparable=false，不能与 legacy 做替代引擎结论；
6. UI 仍暴露较多历史配置和调试入口，需要用 Profile + 设置面板收束。
```

---

## 4. 当前切片策略摘要

当前 legacy 一键切片策略可概括为：

```text
输入：OBJ / MTL / PNG / 3MF / STL
姿态：UI 一键 legacy 默认 scale=[0.8,0.8,0.8]，autoOrient=true，maxHeightMm=6.0
切片：relief_heightfield + intersection_range
纹理：OBJ/3MF 默认 texture.applyMode=top_surface_band，topSurfaceLayers=50
非表面 RGB：texture.nonSurfaceRgbPolicy=model_material
模型实体：modelMaterial 默认 RGB=(0,0,0)，materialChannel=RGB
支撑：UI 一键 legacy 默认 support.mode=full_vertical_projection
支撑优先级：Model > Support > Empty
白墨/光油：MaterialPolicy 可写 W/V；legacy 光油主要是 all_model 或 top_n_layers/in-place
输出：RGBWSV TIFF，uint8，black_is_print，channelOrder=R G B W S V
预览：preview PNG/PPM 是显示数据，不等同于生产 TIFF 六通道
```

当前 OpenVDB candidate 策略可概括为：

```text
显式按钮触发；
要求 OpenVDB ON build；
texture.applyMode=surface_shell_from_sdf；
strict_closed admission；
failurePolicy=non_production_only；
可写 candidate/non-production package；
当前支撑语义尚未与 legacy 等价；
当前不能替代 legacy 默认 production path。
```

---

## 5. Stage 12 专项边界

### 5.1 12A：切片语义与材料策略

解决问题：

```text
颜色层、填充层、支撑层、光油层在 RGBWSV 中的稳定语义；
彩色纹理模型和单材料模型的一致切片行为；
甲片类模型的底部/顶部/内部镂空支撑；
外侧光油层厚度和像素/物理尺寸换算；
不规则浮雕/高 Z 局部区域的支撑判定。
```

### 5.2 12B：引擎性能与 OpenVDB 替代

解决问题：

```text
只比较核心切片耗时，不混入输出保存耗时；
解释为什么当前 OpenVDB 更慢；
建立 legacy / OpenVDB / hybrid / GPU / heightfield fast path 的评估矩阵；
明确 OpenVDB 什么时候可以进入 production candidate，什么时候应该降级为 SDF 专项能力。
```

### 5.3 12C：UI 配置与预览产品化

解决问题：

```text
配置选项说明；
Profile / fixture / advanced 配置分层；
层预览 / 叠加预览 / 原始预览整合；
报告、曲线、诊断区域位置与显示方式；
把用户常用切片设置从大量 JSON 中收口到 UI。
```

---

## 6. 非目标

Stage 12 不做：

```text
不修改 p0.rgbwsv.2；
不修改 RGBWSV channel order；
不修改 uint8 / black_is_print；
不实现 RIP 半色调；
不实现设备通信或喷头 bitstream；
不默认启用 OpenVDB；
不删除现有 regression fixture；
不把 OpenVDB non-production 输出当作 production 输出；
不直接重写 slicer.cpp 大段逻辑。
```

---

## 7. 后续文档包

本决策对应文档包：

```text
docs/slice/DOC/DOC_AUDIT_12_当前切片策略与需求偏差审查.md
docs/slice/ROADMAP/ROADMAP_12_切片语义引擎性能UI专项路线.md
docs/slice/PRD/PRD_12A_彩色纹理材料填充支撑光油策略.md
docs/slice/DEV/DEV_12A_彩色纹理材料填充支撑光油策略设计.md
docs/slice/DEMO/DEMO_12A_彩色纹理材料支撑光油验证方案.md
docs/codex_task/current/TASKS_12A_彩色纹理材料支撑光油任务清单.md
docs/slice/PRD/PRD_12B_切片引擎性能与OpenVDB替代评估.md
docs/slice/DEV/DEV_12B_切片引擎性能与OpenVDB替代评估设计.md
docs/slice/DEMO/DEMO_12B_切片引擎性能验证方案.md
docs/codex_task/current/TASKS_12B_切片引擎性能与OpenVDB替代任务清单.md
docs/slice/PRD/PRD_12C_Qt_UI配置预览工作台收口.md
docs/slice/DEV/DEV_12C_Qt_UI配置预览工作台设计.md
docs/slice/DEMO/DEMO_12C_Qt_UI配置预览验证方案.md
docs/codex_task/current/TASKS_12C_Qt_UI配置预览任务清单.md
docs/slice/REPORT/REPORT_12_专项规划当前状态.md
```

---

## 8. 决策

从 2026-07-05 起，后续与以下问题相关的代码改动必须挂靠 Stage 12 专项之一：

```text
彩色纹理模型填充 / 支撑 / 光油壳层 => 12A
OpenVDB/legacy 性能、替代引擎、算法加速 => 12B
配置界面、Profile、预览入口、报告/曲线布局 => 12C
```

在 12A PRD 未完成验收前，不应直接改变生产输出语义。
