# REPORT_12_专项规划当前状态

> 文档版本：v0.4
> 文档状态：REPORT / Stage 12 Planning
> 生成日期：2026-07-05
> 更新日期：2026-07-06
> 最新更新：2026-07-10

---

## 1. 本次处理目标

根据当前切片软件暴露的问题，建立后续可跟踪的专项阶段文档，重点确认：

```text
1. 当前切片策略是否偏离 PRD/DEV；
2. 彩色纹理模型的颜色层、填充层、支撑、光油语义；
3. OpenVDB 与 legacy 的引擎方向和性能比较方式；
4. Qt UI 配置、预览、报告曲线的产品化收口路线。
```

---

## 2. 已完成文档

新增 Stage 12 总控文档：

```text
docs/slice/DOC/DOC_DECISION_12_11B后进入切片语义引擎性能与UI产品化专项.md
docs/slice/DOC/DOC_AUDIT_12_当前切片策略与需求偏差审查.md
docs/slice/ROADMAP/ROADMAP_12_切片语义引擎性能UI专项路线.md
```

新增 12A 文档：

```text
docs/slice/PRD/PRD_12A_彩色纹理材料填充支撑光油策略.md
docs/slice/DEV/DEV_12A_彩色纹理材料填充支撑光油策略设计.md
docs/slice/DEMO/DEMO_12A_彩色纹理材料支撑光油验证方案.md
docs/codex_task/current/TASKS_12A_彩色纹理材料支撑光油任务清单.md
```

新增 12B 文档：

```text
docs/slice/PRD/PRD_12B_切片引擎性能与OpenVDB替代评估.md
docs/slice/DEV/DEV_12B_切片引擎性能与OpenVDB替代评估设计.md
docs/slice/DEMO/DEMO_12B_切片引擎性能验证方案.md
docs/codex_task/current/TASKS_12B_切片引擎性能与OpenVDB替代任务清单.md
```

新增 12C 文档：

```text
docs/slice/PRD/PRD_12C_Qt_UI配置预览工作台收口.md
docs/slice/DEV/DEV_12C_Qt_UI配置预览工作台设计.md
docs/slice/DEMO/DEMO_12C_Qt_UI配置预览验证方案.md
docs/codex_task/current/TASKS_12C_Qt_UI配置预览任务清单.md
```

---

## 3. 当前策略判断

当前项目没有偏离正式 SliceSoft 目标：

```text
1. RGBWSV 协议仍稳定；
2. legacy slicer_cli 仍是默认 production path；
3. OpenVDB 仍是 optional/candidate；
4. Qt UI 已围绕 config/package/report/preview 工作；
5. 11B 已开始区分生产 RGB 与 preview 显示。
```

但当前存在明显的产品语义缺口：

```text
1. 模型填充层没有显式策略，仍容易被 RGB 黑色 fallback 混淆；
2. 支撑 placement 与内部镂空支撑没有产品化；
3. 外侧光油壳层没有 production 实现；
4. OpenVDB 当前性能和语义都不能替代 legacy；
5. UI 暴露了过多历史配置和分散预览入口。
```

---

## 4. 专项阶段结论

### 4.1 12A 优先级最高

原因：

```text
如果颜色层、模型填充层、支撑层、外侧光油层没有语义定稿，后续任何 OpenVDB/legacy 性能比较都不可比，UI 设置也会继续膨胀。
```

2026-07-06 已完成 12A 关键需求确认：

```text
1. 填充层分为模型内部填充和模型外部支撑填充；
2. 模型内部填充默认白墨，可选择光油或后续扩展材料，生产 Profile 不允许为空；
3. 模型外部填充只能是 S 通道可剥离支撑；
4. 内部镂空区域一律填支撑，internalVoidSupport 默认开启；
5. 支撑 placement 默认 lower；
6. 上表面支撑是模型外部可剥离支撑；如启用外侧光油，支撑生成在光油壳层之外；
7. 外侧光油层允许扩张模型 XY 尺寸；
8. 外侧光油厚度按 mm 配置，默认 0mm，精度 0.01mm，默认按 42.3um/px 换算；
9. 语义优先级为 Model > OuterVarnishShell > Support > Empty；
10. 彩色纹理模型与单材料模型除材料颜色外，几何轮廓、支撑和通道统计逻辑应一致。
```

新增示意图：

```text
docs/slice/DOC/DIAGRAM_12A_内部镂空支撑与外侧光油支撑关系.svg
```

2026-07-06 二次审查结论：

```text
1. 用户新增的 DIAGRAM_12A_指甲模型横截面材料示意图.png 与真实 RIP layer=446 更一致；
2. 旧 DIAGRAM_12A_内部镂空支撑与外侧光油支撑关系.svg 仅保留为优先级概念图，不作为几何验收图；
3. 12A 需要补充 SurfaceVarnishLayer、InnerSurfaceVarnishLayer、真实横截面材料栈和对应 report/preview 验收；
4. 审查记录见 DOC_REVIEW_12A_真实RIP横截面示意图对齐审查.md。
```

### 4.2 12B 不应默认押注 OpenVDB

原因：

```text
当前 OpenVDB core-only benchmark 明显慢于 legacy，且 outputSemanticsComparable=false。OpenVDB 后续更适合作为 SDF/壳层/距离场工具模块，是否替代 production 需要重新通过 gate。
```

### 4.3 12C 应以 Profile + 设置页收口

原因：

```text
不能简单合并所有配置文件，因为其中有大量 regression fixture 和阶段验证样例。正确方式是 UI 普通模式只显示长期 Profile，高级/fixture 模式保留测试配置。
```

---

## 5. 下一步建议

推荐执行顺序：

```text
1. 12A-01 已完成：术语和默认策略已确认；
2. 下一步执行 12A-02/03：配置占位和 report semantic；
3. 同步启动 12C-01：ProfileCatalog，把历史配置从普通用户视野中收束；
4. 12A 语义实现稳定后，再执行 12B Release core-only benchmark；
5. 最后根据 12B 结论决定 legacy 优化、heightfield fast path、OpenVDB hybrid 或 GPU/BVH 路线。
```

---

## 6. 当前未执行项

本报告阶段只建立系统性文档和任务拆分，尚未修改代码实现：

```text
1. 未新增 ModelFillPolicy；
2. 未新增 InternalVoidSupportPolicy；
3. 未新增 OuterVarnishShellPolicy；
4. 未新增 12B benchmark 脚本；
5. 未重构 Qt UI 工作台。
```

---

## 7. 验证状态

文档完成后需运行：

```powershell
git diff --check
```

不需要运行 CMake 构建，因为本阶段只做文档和任务规划。

---

## 8. 2026-07-10 阶段推进更新

此前第 5、6 节是 2026-07-06 的规划快照，不再代表当前实现状态。当前最新状态：

```text
12A：P0/P1 基本完成，见 REPORT_12A_彩色纹理材料支撑光油当前状态.md；
12B-R0：完成 benchmark 契约和 replacement gate；
12B-R1：完成 legacy profile 和首个低风险优化；
12B-R2：完成 OpenVDB SDF utility 定位和 OFF/ON report；
12B：COMPLETE；
12C：完成文档准入、产品默认值冻结和上下文交接，可以进入 R0-01；
12D：已有正式文档和任务入口，但不作为 12C R0/R1 的前置阻断。
```

12C 当前第一优先级不是继续增加 UI 控件，而是解决 Qt 5.15.2 / MSVC 19.51 fresh build blocker，并在现有 ScenarioRegistry、QuickConfigPanel 和 preview panels 上进行增量收口。

12C 准入真源为 `DOC_CHECKLIST_12C_阶段准入与上下文完整性.md`，会话续接真源为 `ai_workspace/context_handoff/2026-07-10_12B-R2到12C-R0阶段交接.md`。
