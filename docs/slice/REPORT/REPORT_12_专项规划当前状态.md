# REPORT_12_专项规划当前状态

> 文档版本：v0.1
> 文档状态：REPORT / Stage 12 Planning
> 生成日期：2026-07-05

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
1. 先执行 12A-01：确认术语和默认策略；
2. 执行 12A-02/03：配置占位和 report semantic；
3. 同步启动 12C-01：ProfileCatalog，把历史配置从普通用户视野中收束；
4. 12A 语义稳定后，再执行 12B Release core-only benchmark；
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
