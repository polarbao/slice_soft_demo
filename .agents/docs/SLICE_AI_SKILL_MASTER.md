# SLICE AI SKILL MASTER

> 定位：`polarbao/slice_soft_demo` / UV 工业喷墨 3D 打印切片 Demo 与正式重构项目的 AI 协作总纲。
> 下游细分 Skill：`.agents/skills/slice-*`。
> 适用：ChatGPT / Codex / VS Code Copilot / Cursor / Antigravity / 其他支持 Skill 的 AI 工具。

---

## 1. 总纲定位

本文件统一项目身份、文档可信等级、架构红线、切片策略边界、AI 协作与上下文交接流程。

`.agents/skills/slice-*` 用于专项触发；本文件用于全局统一判断。

---

## 2. 项目身份

本项目是工业 UV / 喷墨 3D 打印上位机切片软件原型，不是普通 Qt Demo。

固定技术栈：

```text
C++20
Qt 5.15 Widgets
CMake target-based
Windows x64 / MSVC
vcpkg manifest mode when needed
RGBWSV TIFF / RIP Reader / 3MF / OBJ-MTL / MaterialPolicy / Support / Qt Debug UI
```

回答和编码必须考虑：

```text
切片数据
RGBWSV 通道
3MF / OBJ / MTL / Texture
MaterialRoleMapping
MaterialPolicy
MaterialProcessProfile
Support / SupportType
RIP reader
Preview / Reports
Regression
Qt Debug UI
未来正式 Host Software 架构
```

---

## 3. 文档可信等级

```text
A 当前实施基线：当前代码、配置、测试结果，可作为编码依据。
B 正式目标态设计：PRD/DEV/ARCH/DOC_DECISION，可作为方向，不能宣称已实现。
C 历史有效推导：历史对话、旧报告、归档，可补背景，必须标注历史状态。
D 废弃或冲突内容：仅追溯，不作为实现依据。
```

涉及切片策略、协议、架构、UI、R0/R1/R2、构建与测试时，必须区分：

```text
Current State
Target State
Historical State
Pending Confirmation
```

---

## 4. 必读顺序

```text
1. .agents/AGENTS.md
2. .agents/docs/SLICE_AI_SKILL_MASTER.md
3. .agents/docs/project-profile.md
4. .agents/docs/architecture-boundary.md
5. .agents/docs/build-and-test.md
6. ai_workspace/CONTEXT_INDEX.md
7. latest ai_workspace/context_handoff/*.md
8. ai_workspace/AI_WORKSPACE_TOPIC_INDEX.md
9. related ai_workspace/integrated_reports/*.md
10. docs/slice/README.md
11. related docs/slice/PRD/PRD_*.md / docs/slice/DEV/DEV_*.md / docs/slice/ROADMAP/ROADMAP_*.md / docs/slice/DOC/DOC_*.md
12. docs/codex_task/README.md
13. related docs/codex_task/current/*.md
14. related docs/archive/2026-06-30_slicer_legacy/**/*.md as historical evidence
15. current source code
```

---

## 5. 架构红线

```text
Qt 只允许在 apps/slicer_debug_ui / UI 层使用。
slicer_core 和未来 core/importers/materials/support/output 不应依赖 QString/QList/QObject/QWidget。
Importer 不直接写 TIFF。
Material policy 不直接读文件系统。
Support generation 不直接写 report 文件。
UI 不直接访问 slicer.cpp 内部临时结构。
Report writer 不决定业务策略。
```

---

## 6. RGBWSV 输出协议红线

```text
schema = p0.rgbwsv.2
storageMode = stripped / tiled
channelOrder = R G B W S V
bitDepth = 8
polarity = black_is_print
printValue = 0
emptyValue = 255
Model > Support > Empty
SupportType 不进入 TIFF channel
```

---

## 7. 切片策略边界

R0 后必须保留两个正式策略对象：

```text
TextureApplicationPolicy:
  FullVolume
  SurfaceShell
  TopSurfaceOnly
  OuterSurfaceShell

VarnishGeometryPolicy:
  InPlaceTopLayers
  AdditiveGrow
  CompensatedShrink
```

历史 R1 阶段不实现 `SurfaceShell` 和 `CompensatedShrink`，只建立策略对象、配置占位和 pipeline 插入点。

09P/09B 的 OpenVDB / SurfaceShell 工作属于历史实验能力。12B-R2 已将其正式定位为默认关闭的 SDF utility candidate。当前 12C 阶段只允许在 UI 中展示 utility/candidate 状态，且必须满足：

```text
OpenVDB optional and disabled by default
legacy slicer_cli production path is not replaced
production RGBWSV TIFF is not written by the experimental path unless explicitly approved
p0.rgbwsv.2 / RGBWSV channel order / uint8 / black_is_print remain unchanged
strict ProductionAdmissionPolicy blocks unsafe geometry
```

---

## 8. AI 工作流

涉及代码修改前必须输出：

```markdown
## Implementation Plan

### Problem Type
### Layer(s) Involved
### Official Documents
### Historical Documents
### AI Workspace Evidence
### Current Code Reality
### Current State
### Target State
### Historical State
### Pending Confirmation
### Risk Points
### Files To Change
### Verification Plan
```

未获得用户明确授权前，不要直接大范围修改代码。

---

## 9. 验证路径

```text
L1 Unit / schema / parser-level tests
L2 Golden package tests
L3 regression quick/full/heavy
L4 rip_reader_test strict validation
L5 slicer_debug_ui self-test / ui-smoke-test
L6 downstream RIP/device handoff evidence from external team, future only
```

`--self-test` 和 `--ui-smoke-test` 不能当作真机打印验证结论。

---

## 10. 归档与交接

重要会话归档到：

```text
ai_workspace/<model>/chat_logs/YYYY-MM-DD.md
```

可复用结论归档到：

```text
ai_workspace/<model>/analysis_reports/
ai_workspace/integrated_reports/
```

跨设备/跨模型交接归档到：

```text
ai_workspace/context_handoff/
ai_workspace/CONTEXT_INDEX.md
ai_workspace/AI_WORKSPACE_TOPIC_INDEX.md
```

---

## 11. 一句话原则

```text
先查文档，再看代码；
先定边界，再写实现；
先分当前态/目标态/历史态，再输出方案；
先保持回归，再重构模块；
先小步闭环，再扩大重构；
先归档决策，再继续迭代。
```
